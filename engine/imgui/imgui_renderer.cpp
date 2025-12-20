#include "imgui_renderer.h"

#include "../gfx_context.h"
#include "../gfx_renderer.h"

#include "imgui_vs.h"
#include "imgui_fs.h"
#include "imgui_shdr.h"

#include <imgui.h>

#include <utility>

#include <volk.h>

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

	static void update_buffer_resource(NotNull<ImGuiRenderer*> imgui_renderer, Handle handle, u64 size, BufferFlags flags) {
		BufferCreateInfo buffer_create_info = {
			.size = size,
			.flags = flags
		};

		Buffer buffer;
		if (!buffer_create(buffer_create_info, buffer)) {
			return;
		}
		renderer_update_buffer_resource(imgui_renderer->renderer, handle, buffer);
	}

	static void update_imgui_texture(NotNull<ImGuiRenderer*> imgui_renderer, ImTextureData* tex) {
		if (!tex) {
			return;
		}


	}

	static void update_geometry_buffers(NotNull<ImGuiRenderer*> imgui_renderer, NotNull<ImDrawData*> draw_data) {
		if (draw_data->TotalVtxCount > imgui_renderer->vertex_buffer_capacity) {
			imgui_renderer->vertex_buffer_capacity = grow(imgui_renderer->vertex_buffer_capacity, draw_data->TotalVtxCount);
			update_buffer_resource(imgui_renderer, imgui_renderer->vertex_buffer,
				imgui_renderer->vertex_buffer_capacity * sizeof(ImDrawVert), k_vertex_buffer_flags);
		}

		if (draw_data->TotalIdxCount > imgui_renderer->index_buffer_capacity) {
			imgui_renderer->index_buffer_capacity = grow(imgui_renderer->index_buffer_capacity, draw_data->TotalIdxCount);
			update_buffer_resource(imgui_renderer, imgui_renderer->index_buffer, imgui_renderer->index_buffer_capacity * sizeof(ImDrawIdx), k_index_buffer_flags);
		}

		Resource* vertex_buffer_resorce = renderer_get_resource(imgui_renderer->renderer, imgui_renderer->vertex_buffer);
		Resource* index_buffer_resorce = renderer_get_resource(imgui_renderer->renderer, imgui_renderer->index_buffer);

		BufferUpdateInfo vb_update = { .dst_buffer = vertex_buffer_resorce->buffer };
		renderer_buffer_update_begin(imgui_renderer->renderer, draw_data->TotalVtxCount * sizeof(ImDrawVert), vb_update);

		BufferUpdateInfo ib_update = { .dst_buffer = index_buffer_resorce->buffer };
		renderer_buffer_update_begin(imgui_renderer->renderer, draw_data->TotalIdxCount * sizeof(ImDrawIdx), ib_update);

		VkDeviceSize vtx_offset = 0, idx_offset = 0;

		for (i32 n = 0; n < draw_data->CmdListsCount; n++) {
			const ImDrawList* im_cmd_list = draw_data->CmdLists[n];

			auto vtx_size = im_cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
			vb_update.write(imgui_renderer->allocator, im_cmd_list->VtxBuffer.Data, vtx_size, std::exchange(vtx_offset, vtx_offset + vtx_size));

			auto idx_size = im_cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
			ib_update.write(imgui_renderer->allocator, im_cmd_list->IdxBuffer.Data, idx_size, std::exchange(idx_offset, idx_offset + idx_size));
		}

		// TODO: barriers
		PipelineBarrierBuilder barrier_builder = {};
		pipeline_barrier_add_buffer(barrier_builder, vertex_buffer_resorce->buffer, BufferLayout::TransferDst, 0, VK_WHOLE_SIZE);
		vertex_buffer_resorce->buffer.layout = BufferLayout::TransferDst;
		pipeline_barrier_add_buffer(barrier_builder, index_buffer_resorce->buffer, BufferLayout::TransferDst, 0, VK_WHOLE_SIZE);
		index_buffer_resorce->buffer.layout = BufferLayout::TransferDst;

		cmd_pipeline_barrier(imgui_renderer->renderer->active_frame->cmd_buf, barrier_builder);
		pipeline_barrier_builder_reset(barrier_builder);

		renderer_buffer_update_end(imgui_renderer->renderer, vb_update);
		renderer_buffer_update_end(imgui_renderer->renderer, ib_update);

		pipeline_barrier_add_buffer(barrier_builder, vertex_buffer_resorce->buffer, BufferLayout::ShaderRead, 0, VK_WHOLE_SIZE);
		vertex_buffer_resorce->buffer.layout = BufferLayout::ShaderRead;

		pipeline_barrier_add_buffer(barrier_builder, index_buffer_resorce->buffer, BufferLayout::IndexBuffer, 0, VK_WHOLE_SIZE);
		index_buffer_resorce->buffer.layout = BufferLayout::IndexBuffer;

		cmd_pipeline_barrier(imgui_renderer->renderer->active_frame->cmd_buf, barrier_builder);
	}

	ImGuiRenderer* imgui_renderer_create(ImGuiRendererCreateInfo create_info) {
		ImGuiRenderer* imgui_renderer = create_info.allocator->allocate<ImGuiRenderer>();
		if (!imgui_renderer) {
			return nullptr;
		}

		imgui_renderer->allocator = create_info.allocator;
		imgui_renderer->renderer = create_info.renderer;

		if (!shader_module_create(imgui_vs, imgui_vs_size, imgui_renderer->vertex_shader)) {
			imgui_renderer_destroy(imgui_renderer);
			return nullptr;
		}

		if (!shader_module_create(imgui_fs, imgui_fs_size, imgui_renderer->fragment_shader)) {
			imgui_renderer_destroy(imgui_renderer);
			return nullptr;
		}

		VkPipelineShaderStageCreateInfo shader_stages[2] = {
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT,
				.module = imgui_renderer->vertex_shader.handle,
				.pName = "main"
			},
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = imgui_renderer->fragment_shader.handle,
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
			.pColorAttachmentFormats = &imgui_renderer->renderer->swapchain.format
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
			.layout = imgui_renderer->renderer->pipeline_layout.handle,
			.renderPass = VK_NULL_HANDLE
		};

		if (!pipeline_graphics_create(&pipeline_create_info, imgui_renderer->pipeline)) {
			imgui_renderer_destroy(imgui_renderer);
			return nullptr;
		}

		imgui_renderer->vertex_buffer = renderer_add_resource(imgui_renderer->renderer);
		imgui_renderer->vertex_buffer_capacity = k_initial_vertex_count;
		update_buffer_resource(imgui_renderer, imgui_renderer->vertex_buffer, k_initial_vertex_count * sizeof(ImDrawVert), k_vertex_buffer_flags);

		imgui_renderer->index_buffer = renderer_add_resource(imgui_renderer->renderer);
		imgui_renderer->index_buffer_capacity = k_initial_index_count;
		update_buffer_resource(imgui_renderer, imgui_renderer->index_buffer, k_initial_index_count * sizeof(ImDrawIdx), k_index_buffer_flags);

		return imgui_renderer;
	}

	void imgui_renderer_destroy(ImGuiRenderer* imgui_renderer) {
		if (!imgui_renderer) {
			return;
		}

		pipeline_destroy(imgui_renderer->pipeline);

		shader_module_destroy(imgui_renderer->fragment_shader);
		shader_module_destroy(imgui_renderer->vertex_shader);

		renderer_free_resource(imgui_renderer->renderer, imgui_renderer->index_buffer);
		renderer_free_resource(imgui_renderer->renderer, imgui_renderer->vertex_buffer);

		const Allocator* allocator = imgui_renderer->allocator;
		allocator->deallocate(imgui_renderer);
	}

	void imgui_renderer_execute(NotNull<ImGuiRenderer*> imgui_renderer) {
		if (!ImGui::GetCurrentContext()) {
			return;
		}

		ImDrawData* draw_data = ImGui::GetDrawData();
		if (draw_data && draw_data->Textures) {
			for (ImTextureData* tex : *draw_data->Textures) {
				update_imgui_texture(imgui_renderer, tex);
			}
		}

		if (!draw_data || draw_data->TotalVtxCount == 0 || draw_data->TotalIdxCount == 0) {
			return;
		}

		update_geometry_buffers(imgui_renderer, draw_data);

		Resource* vertex_buffer_resorce = renderer_get_resource(imgui_renderer->renderer, imgui_renderer->vertex_buffer);
		Resource* index_buffer_resorce = renderer_get_resource(imgui_renderer->renderer, imgui_renderer->index_buffer);

		Resource* backbuffer_resource = renderer_get_resource(imgui_renderer->renderer, imgui_renderer->renderer->backbuffer_handle);

		VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_LOAD;
		if (backbuffer_resource->image.layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
			PipelineBarrierBuilder barrier_builder = {};
			pipeline_barrier_add_image(barrier_builder, backbuffer_resource->image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VkImageSubresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
				});
			backbuffer_resource->image.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			cmd_pipeline_barrier(imgui_renderer->renderer->active_frame->cmd_buf, barrier_builder);
			load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
		}

		i32 global_vtx_offset = 0;
		i32 global_idx_offset = 0;

		const VkRenderingAttachmentInfo color_attachment = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
			.imageView = backbuffer_resource->srv.handle,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = load_op,
			.storeOp = VK_ATTACHMENT_STORE_OP_NONE,
			.clearValue = {}
		};

		const VkRenderingInfoKHR rendering_info = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
			.renderArea = {
				.offset = { 0, 0 },
				.extent = { backbuffer_resource->image.extent.width, backbuffer_resource->image.extent.height }
			},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment
		};

		CmdBuf cmd = imgui_renderer->renderer->active_frame->cmd_buf;

		cmd_begin_rendering(cmd, rendering_info);
		
		//cmd_bind_index_buffer(cmd, index_buffer_resorce->buffer, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
		vkCmdBindIndexBuffer(cmd.handle, index_buffer_resorce->buffer.handle, 0ull, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
		//cmd_bind_pipeline(cmd, imgui_renderer->pipeline);
		vkCmdBindPipeline(cmd.handle, imgui_renderer->pipeline.bind_point, imgui_renderer->pipeline.handle);

		VkViewport vp = {
			.x = 0.0f,
			.y = 0.0f,
			.width = (f32)backbuffer_resource->image.extent.width,
			.height = (f32)backbuffer_resource->image.extent.height,
			.minDepth = 0.0f,
			.maxDepth = 1.0f
		};
		//cmd_set_viewport(cmd, vp);
		vkCmdSetViewport(cmd.handle, 0u, 1u, &vp);

		ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
		ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

		imgui::PushConstant push_constant = {
			.vertices = vertex_buffer_resorce->buffer.address,
			.scale = {
				2.0f / draw_data->DisplaySize.x, 
				2.0f / draw_data->DisplaySize.y},
			.translate = {
				-1.0f - draw_data->DisplayPos.x * push_constant.scale.x,
				-1.0f - draw_data->DisplayPos.y * push_constant.scale.y
			},
			.sampler_index = 0
		};

		Handle last_image_index = HANDLE_INVALID;

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
				VkRect2D scissor_rect = {
					.offset = { (i32)clip_min.x, (i32)clip_min.y },
					.extent = { (u32)(clip_max.x - clip_min.x), (u32)(clip_max.y - clip_min.y) }
				};
				vkCmdSetScissor(cmd.handle, 0u, 1u, &scissor_rect);

#if 0 // TODO
				Handle new_image_index = (Handle)pcmd->GetTexID();
				if (new_image_index != last_image_index) {
					Resource* render_resource = renderer_get_resource(imgui_renderer->renderer, new_image_index);
					push_constant.image_index = render_resource->srv_index;
					auto constant_range_ptr = reinterpret_cast<const uint8_t*>(&push_constant);
					vkCmdPushConstants(cmd.handle, imgui_renderer->renderer->pipeline_layout.handle,
						VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constant), &push_constant);
					last_image_index = new_image_index;
				}
#else
				push_constant.image_index = 0;
				auto constant_range_ptr = reinterpret_cast<const uint8_t*>(&push_constant);
				vkCmdPushConstants(cmd.handle, imgui_renderer->renderer->pipeline_layout.handle,
					VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constant), &push_constant);
#endif

				vkCmdDrawIndexed(cmd.handle, pcmd->ElemCount, 1u, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0u);
			}

			global_idx_offset += im_cmd_list->IdxBuffer.Size;
			global_vtx_offset += im_cmd_list->VtxBuffer.Size;
		}

		cmd_end_rendering(cmd);
	}
}