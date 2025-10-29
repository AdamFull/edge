#include "gfx_imgui_pass.h"
#include "gfx_renderer.h"
#include "gfx_resource_updater.h"
#include "gfx_resource_uploader.h"

#include "../../assets/shaders/imgui.h"

#include <imgui.h>

#define EDGE_LOGGER_SCOPE "ImGuiPass"

namespace edge::gfx {
	inline auto grow(uint32_t start, uint32_t required, uint32_t factor = 2u) -> uint32_t {
		uint32_t result = start;
		while (result < required) {
			result *= factor;
		}
		return result;
	}

	auto ImGuiPass::create(Renderer& renderer, ResourceUpdater& updater, ResourceUploader& uploader, Pipeline const* pipeline) -> Owned<ImGuiPass> {
		auto self = std::make_unique<ImGuiPass>();
		self->renderer_ = &renderer;
		self->updater_ = &updater;
		self->uploader_ = &uploader;
		self->pipeline_ = pipeline;

		self->vertex_buffer_render_resource_id_ = renderer.create_render_resource();
		self->update_buffer_resource(self->vertex_buffer_render_resource_id_, self->current_vertex_capacity_, sizeof(ImDrawVert), kDynamicVertexBuffer);

		self->index_buffer_render_resource_id_ = renderer.create_render_resource();
		self->update_buffer_resource(self->index_buffer_render_resource_id_, self->current_index_capacity_, sizeof(ImDrawIdx), kDynamicIndexBuffer);

		return self;
	}

	auto ImGuiPass::execute(CommandBuffer const& cmd, float delta_time) -> void {
		if (!ImGui::GetCurrentContext()) {
			return;
		}

		auto backbuffer_id = renderer_->get_backbuffer_resource_id();
		auto& backbuffer_resource = renderer_->get_render_resource(backbuffer_id);
		auto& backbuffer_image = backbuffer_resource.get_handle<gfx::Image>();
		auto& backbuffer_image_view = backbuffer_resource.get_srv_view<ImageView>();
		auto backbuffer_extent = backbuffer_image.get_extent();

		auto backbuffer_state = backbuffer_resource.get_state();
		auto required_backbuffer_state = ResourceStateFlag::eRenderTarget;
		if (backbuffer_state != required_backbuffer_state) {
			auto src_state = util::get_resource_state(backbuffer_state);
			auto dst_state = util::get_resource_state(required_backbuffer_state);

			vk::ImageMemoryBarrier2KHR image_barrier{};
			image_barrier.srcStageMask = src_state.stage_flags;
			image_barrier.srcAccessMask = src_state.access_flags;
			image_barrier.dstStageMask = dst_state.stage_flags;
			image_barrier.dstAccessMask = dst_state.access_flags;
			image_barrier.oldLayout = util::get_image_layout(backbuffer_state);
			image_barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
			image_barrier.image = backbuffer_image.get_handle();
			image_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			image_barrier.subresourceRange.baseMipLevel = 0u;
			image_barrier.subresourceRange.levelCount = 1u;
			image_barrier.subresourceRange.baseArrayLayer = 0u;
			image_barrier.subresourceRange.layerCount = 1u;
			image_barriers_.push_back(image_barrier);

			backbuffer_resource.set_state(required_backbuffer_state);
		}

		auto& vertex_buffer_resource = renderer_->get_render_resource(vertex_buffer_render_resource_id_);
		auto& vertex_buffer = vertex_buffer_resource.get_handle<gfx::Buffer>();

		vk::BufferMemoryBarrier2KHR vertex_buffer_barrier{};
		vertex_buffer_barrier.srcStageMask = vk::PipelineStageFlagBits2KHR::eHost;
		vertex_buffer_barrier.srcAccessMask = vk::AccessFlagBits2KHR::eHostWrite;
		vertex_buffer_barrier.dstStageMask = vk::PipelineStageFlagBits2KHR::eVertexShader;
		vertex_buffer_barrier.dstAccessMask = vk::AccessFlagBits2KHR::eShaderRead;
		vertex_buffer_barrier.buffer = vertex_buffer.get_handle();
		vertex_buffer_barrier.offset = 0ull;
		vertex_buffer_barrier.size = VK_WHOLE_SIZE;
		buffer_barriers_.push_back(vertex_buffer_barrier);

		auto& index_buffer_resource = renderer_->get_render_resource(index_buffer_render_resource_id_);
		auto& index_buffer = index_buffer_resource.get_handle<gfx::Buffer>();

		vk::BufferMemoryBarrier2KHR index_buffer_barrier{};
		index_buffer_barrier.srcStageMask = vk::PipelineStageFlagBits2KHR::eHost;
		index_buffer_barrier.srcAccessMask = vk::AccessFlagBits2KHR::eHostWrite;
		index_buffer_barrier.dstStageMask = vk::PipelineStageFlagBits2KHR::eIndexInput;
		index_buffer_barrier.dstAccessMask = vk::AccessFlagBits2KHR::eIndexRead;
		index_buffer_barrier.buffer = index_buffer.get_handle();
		index_buffer_barrier.offset = 0ull;
		index_buffer_barrier.size = VK_WHOLE_SIZE;
		buffer_barriers_.push_back(index_buffer_barrier);

		// Process images
		ImDrawData* draw_data = ImGui::GetDrawData();
		if (draw_data && draw_data->Textures) {
			for (ImTextureData* tex : *draw_data->Textures) {
				if (tex->Status == ImTextureStatus_OK) {
					// Update pending resources
					auto found = pending_image_uploads_.find(tex->GetTexID());
					if (found != pending_image_uploads_.end()) {
						if (!uploader_->is_task_done(found->second)) {
							continue;
						}

						auto uploading_result = uploader_->get_task_result(found->second);
						if (uploading_result) {
							renderer_->setup_render_resource(found->first, std::move(std::get<gfx::Image>(uploading_result->data)), uploading_result->state);
						}

						pending_image_uploads_.erase(found);
					}
					else {
						// TODO: Check for layout translation
					}
				}
				else if (tex->Status == ImTextureStatus_WantCreate) {
					auto new_texture = renderer_->create_render_resource();

					gfx::ImageImportInfo import_info{};
					import_info.raw.width = tex->Width;
					import_info.raw.height = tex->Height;
					import_info.raw.data.resize(tex->Width * tex->Height * tex->BytesPerPixel);
					std::memcpy(import_info.raw.data.data(), tex->Pixels, import_info.raw.data.size());
					pending_image_uploads_[new_texture] = uploader_->load_image(std::move(import_info));

					tex->SetTexID((ImTextureID)new_texture);
					tex->SetStatus(ImTextureStatus_OK);
				}
				else if (tex->Status == ImTextureStatus_WantUpdates) {
					auto resource_id = tex->GetTexID();
					auto& render_resource = renderer_->get_render_resource(resource_id);
					auto& image = render_resource.get_handle<gfx::Image>();

					// TODO: Move it all to uploader
					// TODO: At first try to use command list from Updater
					uint64_t total_size = 0;
					for (auto& update_region : tex->Updates) {
						EDGE_SLOGD("Updating image {} region: [{}, {}, {}, {}]", tex->GetTexID(), update_region.x, update_region.y, update_region.w, update_region.h);
						auto region_pitch = update_region.w * tex->BytesPerPixel;
						total_size += region_pitch * update_region.h;
					}

					auto& resource_set = updater_->acquire_resource_set();
					auto allocation_result = updater_->get_or_allocate_staging_memory(resource_set, total_size, 4ull);
					GFX_ASSERT_MSG(allocation_result.has_value(), "Failed to create staging buffer for texture update.");
					auto buffer_range = std::move(allocation_result.value());
					auto mapped_range = buffer_range.get_range();

					render_resource.transfer_state(resource_set.command_buffer, gfx::ResourceStateFlag::eCopyDst);

					uint64_t buffer_offset = 0;
					mi::Vector<vk::BufferImageCopy2KHR> copy_regions{};
					for (auto& update_region : tex->Updates) {
						auto region_pitch = update_region.w * tex->BytesPerPixel;

						for (int y = 0; y < update_region.h; y++) {
							const void* src_pixels = tex->GetPixelsAt(update_region.x, update_region.y + y);
							std::memcpy(mapped_range.data() + buffer_offset + (region_pitch * y), src_pixels, region_pitch);
						}

						vk::BufferImageCopy2KHR image_copy_region{};
						image_copy_region.bufferOffset = buffer_offset + buffer_range.get_offset();
						image_copy_region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
						image_copy_region.imageSubresource.mipLevel = 0u;
						image_copy_region.imageSubresource.baseArrayLayer = 0u;
						image_copy_region.imageSubresource.layerCount = 1u;
						image_copy_region.imageOffset = vk::Offset3D{ update_region.x, update_region.y, 0 };
						image_copy_region.imageExtent = vk::Extent3D{ update_region.w, update_region.h, 1u };
						copy_regions.push_back(image_copy_region);

						buffer_offset += region_pitch * update_region.h;
					}

					vk::CopyBufferToImageInfo2KHR copy_info{};
					copy_info.srcBuffer = buffer_range.get_buffer();
					copy_info.dstImage = image.get_handle();
					copy_info.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
					copy_info.regionCount = static_cast<uint32_t>(copy_regions.size());
					copy_info.pRegions = copy_regions.data();
					resource_set.command_buffer->copyBufferToImage2KHR(&copy_info);

					render_resource.transfer_state(resource_set.command_buffer, gfx::ResourceStateFlag::eShaderResource);

					tex->SetStatus(ImTextureStatus_OK);

					// TODO: Uploader is not support partial update yet, because of that trying to update manually
				}
				else if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames >= 256) {
					// TODO: Delete resource
					EDGE_SLOGW("ImGui wants to delete image {}, but it's not implemented yet. ", tex->GetTexID());
				}
			}
		}

		vk::DependencyInfoKHR dependency_info{};
		dependency_info.bufferMemoryBarrierCount = static_cast<uint32_t>(buffer_barriers_.size());
		dependency_info.pBufferMemoryBarriers = buffer_barriers_.data();
		dependency_info.imageMemoryBarrierCount = static_cast<uint32_t>(image_barriers_.size());
		dependency_info.pImageMemoryBarriers = image_barriers_.data();
		cmd->pipelineBarrier2KHR(&dependency_info);

		buffer_barriers_.clear();
		image_barriers_.clear();

		if (!draw_data || draw_data->TotalVtxCount == 0 || draw_data->TotalIdxCount == 0) {
			return;
		}

		if (draw_data->TotalVtxCount > static_cast<int>(current_vertex_capacity_)) {
			EDGE_SLOGW("ImGui vertex buffer too small ({} < {}), need to resize", current_vertex_capacity_, draw_data->TotalVtxCount);

			current_vertex_capacity_ = grow(current_vertex_capacity_, draw_data->TotalVtxCount);
			update_buffer_resource(vertex_buffer_render_resource_id_, current_vertex_capacity_, sizeof(ImDrawVert), kDynamicVertexBuffer);
		}

		if (draw_data->TotalIdxCount > static_cast<int>(current_index_capacity_)) {
			EDGE_SLOGW("ImGui index buffer too small ({} < {}), need to resize", current_index_capacity_, draw_data->TotalIdxCount);

			current_index_capacity_ = grow(current_index_capacity_, draw_data->TotalIdxCount);
			update_buffer_resource(index_buffer_render_resource_id_, current_index_capacity_, sizeof(ImDrawIdx), kDynamicIndexBuffer);
		}

		auto mapped_vertex_range = vertex_buffer.map().value();
		auto mapped_index_range = index_buffer.map().value();

		// Update geometry buffers
		int32_t global_vtx_offset = 0;
		int32_t global_idx_offset = 0;

		for (int32_t n = 0; n < draw_data->CmdListsCount; n++) {
			const ImDrawList* im_cmd_list = draw_data->CmdLists[n];

			auto vertex_byte_size = im_cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
			std::memcpy(mapped_vertex_range.data() + global_vtx_offset, im_cmd_list->VtxBuffer.Data, vertex_byte_size);

			auto index_byte_size = im_cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
			std::memcpy(mapped_index_range.data() + global_idx_offset, im_cmd_list->IdxBuffer.Data, index_byte_size);

			global_vtx_offset += vertex_byte_size;
			global_idx_offset += index_byte_size;
		}

		// Reset offsets for drawing
		global_vtx_offset = 0;
		global_idx_offset = 0;

		vk::RenderingAttachmentInfo color_attachment{};
		color_attachment.imageView = backbuffer_image_view.get_handle();
		color_attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
		color_attachment.storeOp = vk::AttachmentStoreOp::eNone;
		color_attachment.clearValue = vk::ClearValue{};

		vk::RenderingInfoKHR rendering_info{};
		rendering_info.renderArea = vk::Rect2D{ { 0u, 0u }, { backbuffer_extent.width, backbuffer_extent.height } };
		rendering_info.layerCount = 1u;
		rendering_info.colorAttachmentCount = 1u;
		rendering_info.pColorAttachments = &color_attachment;
		cmd->beginRenderingKHR(&rendering_info);

		cmd->bindIndexBuffer(index_buffer.get_handle(), 0ull, sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32);

		cmd->bindPipeline(pipeline_->get_bind_point(), pipeline_->get_handle());

		vk::Viewport viewport{ 0.0f, 0.0f, static_cast<float>(backbuffer_extent.width), static_cast<float>(backbuffer_extent.height) };
		cmd->setViewport(0u, 1u, &viewport);

		// Will project scissor/clipping rectangles into framebuffer space
		ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
		ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

		imgui::PushConstant push_constant{};
		push_constant.vertices = vertex_buffer.get_device_address();
		push_constant.scale.x = 2.f / draw_data->DisplaySize.x;
		push_constant.scale.y = 2.f / draw_data->DisplaySize.y;
		push_constant.translate.x = -1.0f - draw_data->DisplayPos.x * push_constant.scale.x;
		push_constant.translate.y = -1.0f - draw_data->DisplayPos.y * push_constant.scale.y;
		push_constant.sampler_index = 0u;

		uint32_t last_image_index = ~0u;

		ImGuiIO& io = ImGui::GetIO();
		for (int32_t n = 0; n < draw_data->CmdListsCount; n++) {
			const ImDrawList* im_cmd_list = draw_data->CmdLists[n];
			for (int32_t cmd_i = 0; cmd_i < im_cmd_list->CmdBuffer.Size; cmd_i++) {
				const ImDrawCmd* pcmd = &im_cmd_list->CmdBuffer[cmd_i];

				// Project scissor/clipping rectangles into framebuffer space
				ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
				ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

				// Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
				if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
				if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
				if (clip_max.x > io.DisplaySize.x) { clip_max.x = io.DisplaySize.x; }
				if (clip_max.y > io.DisplaySize.y) { clip_max.y = io.DisplaySize.y; }
				if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
					continue;

				// Apply scissor/clipping rectangle
				vk::Rect2D scissor_rect{ { (int32_t)(clip_min.x), (int32_t)(clip_min.y) }, { (uint32_t)(clip_max.x - clip_min.x), (uint32_t)(clip_max.y - clip_min.y) } };
				cmd->setScissor(0u, 1u, &scissor_rect);

				uint32_t new_image_index = pcmd->GetTexID();
				if (new_image_index != last_image_index) {
					auto& render_resource = renderer_->get_render_resource(new_image_index);
					push_constant.image_index = render_resource.get_srv_index();
					auto constant_range_ptr = reinterpret_cast<const uint8_t*>(&push_constant);
					renderer_->push_constant_range(cmd,
						vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute,
						{ constant_range_ptr, sizeof(push_constant) });
					last_image_index = new_image_index;
				}

				cmd->drawIndexed(pcmd->ElemCount, 1u, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0u);
			}

			global_idx_offset += im_cmd_list->IdxBuffer.Size;
			global_vtx_offset += im_cmd_list->VtxBuffer.Size;
		}

		cmd->endRenderingKHR();
	}

	auto ImGuiPass::update_buffer_resource(uint32_t resource_id, vk::DeviceSize element_count, vk::DeviceSize element_size, BufferFlags usage) -> void {
		gfx::BufferCreateInfo buffer_create_info{};
		buffer_create_info.size = element_size;
		buffer_create_info.count = element_count;
		buffer_create_info.flags = usage;
		buffer_create_info.minimal_alignment = element_size;

		auto buffer_create_result = gfx::Buffer::create(buffer_create_info);
		GFX_ASSERT_MSG(buffer_create_result.has_value(), "Failed to create/update buffer.");
		auto& render_resource = renderer_->get_render_resource(resource_id);
		render_resource.update(std::move(buffer_create_result.value()), gfx::ResourceStateFlag::eUndefined);
	}
}

#undef EDGE_LOGGER_SCOPE