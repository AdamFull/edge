#include "imgui_renderer.h"
#include <logger.hpp>

#include "../gfx_context.h"
#include "../gfx_renderer.h"

#include "imgui_vs.h"
#include "imgui_fs.h"
#include "imgui_shdr.h"

#include <imgui.h>

#include <utility>

namespace edge::gfx {
	constexpr u64 k_initial_vertex_count = 2048ull;
	constexpr u64 k_initial_index_count = 4096ull;
	constexpr BufferFlags k_vertex_buffer_flags = BUFFER_FLAG_DYNAMIC | BUFFER_FLAG_DEVICE_ADDRESS | BUFFER_FLAG_VERTEX;
	constexpr BufferFlags k_index_buffer_flags = BUFFER_FLAG_DYNAMIC | BUFFER_FLAG_DEVICE_ADDRESS | BUFFER_FLAG_INDEX;

	static uint32_t grow(uint32_t start, uint32_t required, uint32_t factor = 2u) {
		uint32_t result = start;
		while (result < required) {
			result *= factor;
		}
		return result;
	}

	bool ImGuiRenderer::create(NotNull<const Allocator*> alloc, ImGuiRendererCreateInfo create_info) {
		renderer = create_info.renderer;

		if (!vertex_shader.create(imgui_vs, imgui_vs_size)) {
			destroy(alloc);
			return false;
		}

		if (!fragment_shader.create(imgui_fs, imgui_fs_size)) {
			destroy(alloc);
			return false;
		}

		VkPipelineShaderStageCreateInfo shader_stages[2] = {
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT,
				.module = vertex_shader,
				.pName = "main"
			},
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = fragment_shader,
				.pName = "main"
			}
		};

		VkPipelineVertexInputStateCreateInfo vertex_input_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
		};

		VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		};

		VkPipelineTessellationStateCreateInfo tessellation_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO
		};

		VkViewport viewport_state = {
			0.0f, 0.0f,
			1280.0f, 720.0f,
			0.0f, 1.0f
		};

		VkRect2D scissor_rect = {
			.offset = { 0, 0 },
			.extent = { 1280, 720 }
		};

		VkPipelineViewportStateCreateInfo viewport_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1u,
			.pViewports = &viewport_state,
			.scissorCount = 1u,
			.pScissors = &scissor_rect
		};

		VkPipelineRasterizationStateCreateInfo rasterization_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_NONE,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.lineWidth = 1.0f
		};

		VkPipelineMultisampleStateCreateInfo miltisample_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
		};

		VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
		};

		VkPipelineColorBlendAttachmentState color_blend_attachment_state = {
			.blendEnable = VK_TRUE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.alphaBlendOp = VK_BLEND_OP_ADD,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};

		VkPipelineColorBlendStateCreateInfo color_blend_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 1u,
			.pAttachments = &color_blend_attachment_state,
			.blendConstants = { 1.0f, 1.0f, 1.0f, 1.0f }
		};

		VkDynamicState dynamic_states[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = 2u,
			.pDynamicStates = dynamic_states
		};

		VkPipelineRenderingCreateInfoKHR rendering_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
			.colorAttachmentCount = 1u,
			.pColorAttachmentFormats = &renderer->swapchain.format
		};

		VkGraphicsPipelineCreateInfo pipeline_create_info = {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &rendering_create_info,
			.stageCount = 2u,
			.pStages = shader_stages,
			.pVertexInputState = &vertex_input_create_info,
			.pInputAssemblyState = &input_assembly_create_info,
			.pTessellationState = &tessellation_create_info,
			.pViewportState = &viewport_create_info,
			.pRasterizationState = &rasterization_create_info,
			.pMultisampleState = &miltisample_create_info,
			.pDepthStencilState = &depth_stencil_create_info,
			.pColorBlendState = &color_blend_create_info,
			.pDynamicState = &dynamic_state_create_info,
			.layout = renderer->pipeline_layout,
			.renderPass = VK_NULL_HANDLE
		};

		if (!pipeline.create(&pipeline_create_info)) {
			destroy(alloc);
			return false;
		}

		vertex_buffer = renderer->add_resource();
		vertex_buffer_capacity = k_initial_vertex_count;

		index_buffer = renderer->add_resource();
		index_buffer_capacity = k_initial_index_count;

		update_buffers(alloc);

		return true;
	}

	void ImGuiRenderer::destroy(NotNull<const Allocator*> alloc) {
		pipeline.destroy();

		fragment_shader.destroy();
		vertex_shader.destroy();

		//renderer->free_resource(alloc, index_buffer);
		//renderer->free_resource(alloc, vertex_buffer);
	}

	void ImGuiRenderer::execute(NotNull<const Allocator*> alloc) {
		if (!ImGui::GetCurrentContext()) {
			return;
		}

		ImDrawData* draw_data = ImGui::GetDrawData();
		if (draw_data && draw_data->Textures) {
			for (ImTextureData* tex : *draw_data->Textures) {
				update_texture(alloc, tex);
			}
		}

		if (!draw_data || draw_data->TotalVtxCount == 0 || draw_data->TotalIdxCount == 0) {
			return;
		}

		update_geometry(alloc, draw_data);

		RenderResource* vertex_buffer_resource = renderer->get_resource(vertex_buffer);
		Buffer* vertex_buffer_ptr = renderer->buffer_handle_pool.get(vertex_buffer_resource->get_handle());
		RenderResource* index_buffer_resource = renderer->get_resource(index_buffer);
		Buffer* index_buffer_ptr = renderer->buffer_handle_pool.get(index_buffer_resource->get_handle());
		RenderResource* backbuffer_resource = renderer->get_resource(renderer->backbuffer_handle);
		Image* backbuffer_ptr = renderer->image_handle_pool.get(backbuffer_resource->get_handle());

		CmdBuf cmd = renderer->active_frame->cmd;

		VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_LOAD;
		if (backbuffer_ptr->layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
			PipelineBarrierBuilder barrier_builder = {};
			barrier_builder.add_image(*backbuffer_ptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VkImageSubresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
				});
			backbuffer_ptr->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			cmd.pipeline_barrier(barrier_builder);
			load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
		}

		i32 global_vtx_offset = 0;
		i32 global_idx_offset = 0;

		ImageView* image_view = renderer->image_srv_handle_pool.get(backbuffer_resource->get_srv_handle());

		const VkRenderingAttachmentInfo color_attachment = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
			.imageView = *image_view,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = load_op,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = {}
		};

		const VkRenderingInfoKHR rendering_info = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
			.renderArea = {
				.offset = { 0, 0 },
				.extent = { backbuffer_ptr->extent.width, backbuffer_ptr->extent.height }
			},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment
		};

		cmd.begin_rendering(rendering_info);

		cmd.bind_index_buffer(*index_buffer_ptr, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
		cmd.bind_pipeline(pipeline);

		cmd.set_viewport(0.0f, 0.0f, (f32)backbuffer_ptr->extent.width, (f32)backbuffer_ptr->extent.height);

		ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
		ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

		imgui::PushConstant push_constant = {
			.vertices = vertex_buffer_ptr->address,
			.scale = {
				2.0f / draw_data->DisplaySize.x,
				2.0f / draw_data->DisplaySize.y},
			.translate = {
				-1.0f - draw_data->DisplayPos.x * push_constant.scale.x,
				-1.0f - draw_data->DisplayPos.y * push_constant.scale.y
			},
			.sampler_index = 0
		};

		ImTextureBinding last_image_binding = ImTextureBinding{ HANDLE_INVALID, HANDLE_INVALID };

		ImGuiIO& io = ImGui::GetIO();
		for (i32 n = 0; n < draw_data->CmdListsCount; n++) {
			const ImDrawList* im_cmd_list = draw_data->CmdLists[n];
			for (i32 cmd_i = 0; cmd_i < im_cmd_list->CmdBuffer.Size; cmd_i++) {
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
				cmd.set_scissor(clip_min.x, clip_min.y, clip_max.x - clip_min.x, clip_max.y - clip_min.y);

				ImTextureBinding new_image_binding = ImTextureBinding::from_texture_id(pcmd->GetTexID());
				if (new_image_binding != last_image_binding) {
					RenderResource* image_resource = renderer->get_resource(new_image_binding.image);
					push_constant.image_index = image_resource->get_srv_handle().index;
					RenderResource* sampler_resource = renderer->get_resource(new_image_binding.sampler);
					push_constant.sampler_index = sampler_resource ? sampler_resource->get_srv_handle().index : 0u;
					// TODO: Add sampler
					renderer->push_constants(VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, push_constant);
					last_image_binding = new_image_binding;
				}

				cmd.draw_indexed(pcmd->ElemCount, 1u, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0u);
			}

			global_idx_offset += im_cmd_list->IdxBuffer.Size;
			global_vtx_offset += im_cmd_list->VtxBuffer.Size;
		}

		cmd.end_rendering();
	}

	void ImGuiRenderer::update_buffers(NotNull<const Allocator*> alloc) {
		if (vertex_need_to_grow) {
			BufferCreateInfo create_info = {
			.size = vertex_buffer_capacity * sizeof(ImDrawVert),
			.flags = k_vertex_buffer_flags
			};

			Buffer buffer;
			if (!buffer.create(create_info)) {
				return;
			}
			renderer->update_resource(alloc, vertex_buffer, buffer);
			vertex_need_to_grow = false;
		}

		if (index_need_to_grow) {
			BufferCreateInfo create_info = {
			.size = index_buffer_capacity * sizeof(ImDrawIdx),
			.flags = k_index_buffer_flags
			};

			Buffer buffer;
			if (!buffer.create(create_info)) {
				return;
			}
			renderer->update_resource(alloc, index_buffer, buffer);
			index_need_to_grow = false;
		}
	}

	void ImGuiRenderer::update_texture(NotNull<const Allocator*> alloc, NotNull<ImTextureData*> tex) {
		if (tex->Status == ImTextureStatus_OK) {
			return;
		}

		CmdBuf cmd = renderer->active_frame->cmd;

		if (tex->Status == ImTextureStatus_WantCreate) {
			Handle image_handle = renderer->add_resource();

			ImageCreateInfo create_info = {
				.extent = {
					.width = (u32)tex->Width,
					.height = (u32)tex->Height,
					.depth = 1u
					},
				.usage_flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.format = VK_FORMAT_R8G8B8A8_SRGB
			};

			Image image = {};
			if (!image.create(create_info)) {
				EDGE_LOG_ERROR("Failed to create font image.");
				tex->SetTexID(ImTextureID_Invalid);
				tex->SetStatus(ImTextureStatus_Destroyed);
				return;
			}

			PipelineBarrierBuilder barrier_builder = {};

			barrier_builder.add_image(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
				});
			cmd.pipeline_barrier(barrier_builder);
			image.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

			usize whole_size = tex->Width * tex->Height * tex->BytesPerPixel;

			ImageUpdateInfo update_info = {
				.dst_image = image,
				.buffer_view = renderer->active_frame->try_allocate_staging_memory(alloc, whole_size, 1)
			};

			update_info.write(alloc, {
				.data = { tex->Pixels, whole_size },
				.extent = {
					.width = (u32)tex->Width,
					.height = (u32)tex->Height,
					.depth = 1
					}
				});
			renderer->image_update_end(alloc, update_info);
			renderer->attach_resource(image_handle, image);

			ImTextureBinding binding{ image_handle, HANDLE_INVALID };
			tex->SetTexID((ImTextureID)binding);
			tex->SetStatus(ImTextureStatus_OK);
		}
		else if (tex->Status == ImTextureStatus_WantUpdates) {
			Handle resource_id = (Handle)tex->GetTexID();
			RenderResource* resource = renderer->get_resource(resource_id);

			usize total_size = 0;
			for (const ImTextureRect& update_region : tex->Updates) {
				//EDGE_LOG_DEBUG("Updating image {} region: [{}, {}, {}, {}]", tex->GetTexID(), update_region.x, update_region.y, update_region.w, update_region.h);
				usize region_pitch = update_region.w * tex->BytesPerPixel;
				total_size += region_pitch * update_region.h;
			}

			PipelineBarrierBuilder barrier_builder = {};
			VkImageSubresourceRange subresource_range = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			};

			Image* image = renderer->image_handle_pool.get(resource->get_handle());

			barrier_builder.add_image(*image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresource_range);
			cmd.pipeline_barrier(barrier_builder);
			barrier_builder.reset();
			image->layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

			ImageUpdateInfo update_info = {
				.dst_image = *image,
				.buffer_view = renderer->active_frame->try_allocate_staging_memory(alloc, total_size, 1)
			};

			u8* compacted_data = (u8*)alloc->malloc(total_size, 1);
			// TODO: Check allocation result

			usize buffer_offset = 0;
			for (const ImTextureRect& update_region : tex->Updates) {
				usize region_pitch = update_region.w * tex->BytesPerPixel;

				for (usize y = 0; y < update_region.h; y++) {
					const void* src_pixels = tex->GetPixelsAt(update_region.x, update_region.y + y);
					memcpy(compacted_data + buffer_offset + (region_pitch * y), src_pixels, region_pitch);
				}

				usize region_size = region_pitch * update_region.h;

				update_info.write(alloc, {
				.data = { compacted_data + buffer_offset, region_size },
				.offset = {
						.x = update_region.x,
						.y = update_region.y,
						.z = 0
					},
				.extent = {
					.width = update_region.w,
					.height = update_region.h,
					.depth = 1
					}
					});

				buffer_offset += region_size;
			}

			renderer->image_update_end(alloc, update_info);
			alloc->free(compacted_data);

			barrier_builder.add_image(*image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresource_range);
			cmd.pipeline_barrier(barrier_builder);
			image->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			tex->SetStatus(ImTextureStatus_OK);
		}
		else if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames >= 256) {
			Handle image_handle = (Handle)tex->GetTexID();
			renderer->free_resource(alloc, image_handle);

			tex->SetTexID(ImTextureID_Invalid);
			tex->SetStatus(ImTextureStatus_Destroyed);
		}
	}

	void ImGuiRenderer::update_geometry(NotNull<const Allocator*> alloc, NotNull<ImDrawData*> draw_data) {
		if (draw_data->TotalVtxCount > vertex_buffer_capacity) {
			vertex_buffer_capacity = grow(vertex_buffer_capacity, draw_data->TotalVtxCount);
			vertex_need_to_grow = true;
		}

		if (draw_data->TotalIdxCount > index_buffer_capacity) {
			index_buffer_capacity = grow(index_buffer_capacity, draw_data->TotalIdxCount);
			index_need_to_grow = true;
		}

		update_buffers(alloc);

		RenderResource* vertex_buffer_resource = renderer->get_resource(vertex_buffer);
		Buffer* vertex_buffer_ptr = renderer->buffer_handle_pool.get(vertex_buffer_resource->get_handle());
		RenderResource* index_buffer_resource = renderer->get_resource(index_buffer);
		Buffer* index_buffer_ptr = renderer->buffer_handle_pool.get(index_buffer_resource->get_handle());

		BufferUpdateInfo vb_update = {
			.dst_buffer = *vertex_buffer_ptr,
			.buffer_view = renderer->active_frame->try_allocate_staging_memory(alloc, draw_data->TotalVtxCount * sizeof(ImDrawVert), 1)
		};

		BufferUpdateInfo ib_update = {
			.dst_buffer = *index_buffer_ptr,
			.buffer_view = renderer->active_frame->try_allocate_staging_memory(alloc, draw_data->TotalIdxCount * sizeof(ImDrawIdx), 1)
		};

		VkDeviceSize vtx_offset = 0, idx_offset = 0;

		for (i32 n = 0; n < draw_data->CmdListsCount; n++) {
			const ImDrawList* im_cmd_list = draw_data->CmdLists[n];

			auto vtx_size = im_cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
			vb_update.write(alloc, { (u8*)im_cmd_list->VtxBuffer.Data, vtx_size }, std::exchange(vtx_offset, vtx_offset + vtx_size));

			auto idx_size = im_cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
			ib_update.write(alloc, { (u8*)im_cmd_list->IdxBuffer.Data, idx_size }, std::exchange(idx_offset, idx_offset + idx_size));
		}

		// TODO: barriers
		PipelineBarrierBuilder barrier_builder = {};
		barrier_builder.add_buffer(*vertex_buffer_ptr, BufferLayout::TransferDst, 0, VK_WHOLE_SIZE);
		vertex_buffer_ptr->layout = BufferLayout::TransferDst;
		barrier_builder.add_buffer(*index_buffer_ptr, BufferLayout::TransferDst, 0, VK_WHOLE_SIZE);
		index_buffer_ptr->layout = BufferLayout::TransferDst;

		CmdBuf cmd = renderer->active_frame->cmd;

		cmd.pipeline_barrier(barrier_builder);
		barrier_builder.reset();

		renderer->buffer_update_end(alloc, vb_update);
		renderer->buffer_update_end(alloc, ib_update);

		barrier_builder.add_buffer(*vertex_buffer_ptr, BufferLayout::ShaderRead, 0, VK_WHOLE_SIZE);
		vertex_buffer_ptr->layout = BufferLayout::ShaderRead;

		barrier_builder.add_buffer(*index_buffer_ptr, BufferLayout::IndexBuffer, 0, VK_WHOLE_SIZE);
		index_buffer_ptr->layout = BufferLayout::IndexBuffer;

		cmd.pipeline_barrier(barrier_builder);
	}
}