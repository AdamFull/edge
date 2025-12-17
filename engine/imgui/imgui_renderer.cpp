#include "imgui_renderer.h"

#include "../gfx_context.h"
#include "../gfx_renderer.h"

#include "imgui_vs.h"
#include "imgui_fs.h"
#include "imgui_shdr.h"

#include <imgui.h>

namespace edge::gfx {
	constexpr u64 k_initial_vertex_count = 2048ull;
	constexpr u64 k_initial_index_count = 4096ull;
	constexpr BufferFlags k_vertex_buffer_flags = BUFFER_FLAG_DYNAMIC | BUFFER_FLAG_DEVICE_ADDRESS | BUFFER_FLAG_VERTEX;
	constexpr BufferFlags k_index_buffer_flags = BUFFER_FLAG_DYNAMIC | BUFFER_FLAG_DEVICE_ADDRESS | BUFFER_FLAG_VERTEX;

	static uint32_t grow(uint32_t start, uint32_t required, uint32_t factor = 2u) {
		uint32_t result = start;
		while (result < required) {
			result *= factor;
		}
		return result;
	}

	static void update_buffer_resource(ImGuiRenderer* imgui_renderer, Handle handle, u64 size, BufferFlags flags) {
		BufferCreateInfo buffer_create_info = {
			.size = size,
			.flags = flags
		};

		Buffer buffer;
		if (!buffer_create(&buffer_create_info, &buffer)) {
			return;
		}
		renderer_update_buffer_resource(imgui_renderer->renderer, handle, buffer);
	}

	static void update_imgui_texture(ImGuiRenderer* imgui_renderer, ImTextureData* tex) {
		if (!imgui_renderer || !tex) {
			return;
		}


	}

	static void update_geometry_buffers(ImGuiRenderer* imgui_renderer, ImDrawData* draw_data) {
		if (!imgui_renderer || !draw_data) {
			return;
		}

		if (draw_data->TotalVtxCount > imgui_renderer->vertex_buffer_capacity) {
			imgui_renderer->vertex_buffer_capacity = grow(imgui_renderer->vertex_buffer_capacity, draw_data->TotalVtxCount);
			update_buffer_resource(imgui_renderer, imgui_renderer->index_buffer, imgui_renderer->vertex_buffer_capacity, k_vertex_buffer_flags);
		}

		if (draw_data->TotalIdxCount > imgui_renderer->index_buffer_capacity) {
			imgui_renderer->index_buffer_capacity = grow(imgui_renderer->index_buffer_capacity, draw_data->TotalIdxCount);
			update_buffer_resource(imgui_renderer, imgui_renderer->index_buffer, imgui_renderer->index_buffer_capacity, k_index_buffer_flags);
		}


	}

	ImGuiRenderer* imgui_renderer_create(const ImGuiRendererCreateInfo* create_info) {
		if (!create_info) {
			return nullptr;
		}

		ImGuiRenderer* imgui_renderer = create_info->allocator->allocate<ImGuiRenderer>();
		if (!imgui_renderer) {
			return nullptr;
		}

		imgui_renderer->allocator = create_info->allocator;
		imgui_renderer->renderer = create_info->renderer;

		if (!shader_module_create(imgui_vs, imgui_vs_size, &imgui_renderer->vertex_shader)) {
			imgui_renderer_destroy(imgui_renderer);
			return nullptr;
		}

		if (!shader_module_create(imgui_fs, imgui_fs_size, &imgui_renderer->fragment_shader)) {
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
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
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
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR,
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

		if (!pipeline_graphics_create(&pipeline_create_info, &imgui_renderer->pipeline)) {
			imgui_renderer_destroy(imgui_renderer);
			return nullptr;
		}

		imgui_renderer->allocator = create_info->allocator;
		imgui_renderer->renderer = create_info->renderer;

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

		pipeline_destroy(&imgui_renderer->pipeline);

		shader_module_destroy(&imgui_renderer->fragment_shader);
		shader_module_destroy(&imgui_renderer->vertex_shader);

		renderer_free_resource(imgui_renderer->renderer, imgui_renderer->index_buffer);
		renderer_free_resource(imgui_renderer->renderer, imgui_renderer->vertex_buffer);

		const Allocator* allocator = imgui_renderer->allocator;
		allocator->deallocate(imgui_renderer);
	}

	void imgui_renderer_execute(ImGuiRenderer* imgui_renderer) {
		if (!imgui_renderer || !ImGui::GetCurrentContext()) {
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


	}
}