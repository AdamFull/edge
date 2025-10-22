#include "gfx_shader_library.h"

#include "../filesystem/filesystem.h"

#define EDGE_LOGGER_SCOPE "gfx::ShaderLibrary"

namespace edge::gfx {
	ShaderLibrary::ShaderLibrary(Context const& ctx)
		: ctx_{ &ctx }
		, pipelines_{}
		, pipeline_cache_path_{} {

	}

	ShaderLibrary::~ShaderLibrary() {
		if (pipeline_cache_) {

			mi::Vector<uint8_t> pipeline_cache_data{};
			auto result = pipeline_cache_.get_data(pipeline_cache_data);
			if (result != vk::Result::eSuccess) {
				EDGE_SLOGE("Failed to read pipeline cache data.");
				return;
			}

			fs::OutputFileStream outfile(pipeline_cache_path_, std::ios_base::binary);
			if (!outfile.is_open()) {
				EDGE_SLOGE("Failed to open output file.");
				return;
			}

			outfile.write(reinterpret_cast<const char*>(pipeline_cache_data.data()), pipeline_cache_data.size());
		}
	}

	auto ShaderLibrary::construct(Context const& ctx, PipelineLayout const& pipeline_layout, std::u8string_view pipeline_cache_path, std::u8string_view shaders_path) -> Result<ShaderLibrary> {
		ShaderLibrary self{ ctx };
		self.pipeline_cache_path_ = pipeline_cache_path;
		if (auto result = self._construct(pipeline_layout, shaders_path); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}
		return self;
	}

	auto ShaderLibrary::_construct(PipelineLayout const& pipeline_layout, std::u8string_view shaders_path) -> vk::Result {
		// Load pipeline cache if exists
		mi::Vector<uint8_t> pipeline_cache_data{};

		fs::InputFileStream infile(pipeline_cache_path_, std::ios_base::binary);
		if (infile.is_open()) {
			pipeline_cache_data = { std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>() };
		}

		if (auto result = ctx_->create_pipeline_cache(pipeline_cache_data); !result) {
			EDGE_SLOGE("Failed to create pipeline cache.");
			return result.error();
		}
		else {
			pipeline_cache_ = std::move(result.value());
		}

		static const char* entry_point_name = "main";

		// Load pipelines
		for (const auto& entry : fs::walk_directory(shaders_path, true)) {
			if (entry.is_directory) {
				continue;
			}

			auto ext = fs::path::extension(entry.path);
			if (ext.compare(u8".shfx") == 0) {
				auto shader_path = fs::path::append(shaders_path, entry.path);
				// Load shader file and parse
				fs::InputFileStream shader_file(shader_path, std::ios_base::binary);

				BinaryReader reader{ shader_file };

				ShaderEffect shader_effect;
				reader.read(shader_effect);

				// TODO: validate

				mi::Vector<ShaderModule> shader_modules{ shader_effect.stages.size() };
				mi::Vector<vk::PipelineShaderStageCreateInfo> shader_stages{ shader_effect.stages.size() };

				for (int32_t i = 0; i < static_cast<int32_t>(shader_effect.stages.size()); ++i) {
					const auto& stage = shader_effect.stages[i];

					auto result = ctx_->create_shader_module(stage.code);
					if (!result) {
						EDGE_SLOGE("Failed to create shader module at index {}, for effect \"{}\". Reason: {}.", i, shader_effect.name, vk::to_string(result.error()));
						continue;
					}

					shader_modules[i] = std::move(result.value());

					auto& shader_stage_create_info = shader_stages[i];
					shader_stage_create_info.stage = stage.stage;
					//shader_stage_create_info.pName = stage.entry_point_name.c_str();
					shader_stage_create_info.pName = entry_point_name;
					shader_stage_create_info.module = shader_modules[i].get_handle();
				}

				if (shader_effect.bind_point == vk::PipelineBindPoint::eGraphics) {
					mi::Vector<vk::VertexInputBindingDescription> vertex_input_binding_descriptions{};
					for (auto const& binding_desc : shader_effect.vertex_input_bindings) {
						vertex_input_binding_descriptions.push_back(binding_desc.to_vulkan());
					}

					mi::Vector<vk::VertexInputAttributeDescription> vertex_input_attribute_descriptions{};
					for (auto const& attribute_desc : shader_effect.vertex_input_attributes) {
						vertex_input_attribute_descriptions.push_back(attribute_desc.to_vulkan());
					}

					vk::PipelineVertexInputStateCreateInfo input_state_create_info{};
					input_state_create_info.vertexBindingDescriptionCount = static_cast<uint32_t>(vertex_input_binding_descriptions.size());
					input_state_create_info.pVertexBindingDescriptions = vertex_input_binding_descriptions.data();
					input_state_create_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_input_attribute_descriptions.size());
					input_state_create_info.pVertexAttributeDescriptions = vertex_input_attribute_descriptions.data();

					auto input_assembly_state_create_info = shader_effect.pipeline_state.get_input_assembly_state();
					auto tessellation_state_create_info = shader_effect.pipeline_state.get_tessellation_state();
					auto rasterization_state_create_info = shader_effect.pipeline_state.get_rasterization_state();
					auto multisample_state_create_info = shader_effect.pipeline_state.get_multisample_state();
					multisample_state_create_info.pSampleMask = shader_effect.multisample_sample_masks.data();
					auto depth_stencil_state_create_info = shader_effect.pipeline_state.get_depth_stencil_state();
					auto color_blend_state_create_info = shader_effect.pipeline_state.get_color_blending_state();
					color_blend_state_create_info.attachmentCount = static_cast<uint32_t>(shader_effect.color_attachments.size());

					mi::Vector<vk::PipelineColorBlendAttachmentState> attachment_states{ shader_effect.color_attachments.size() };
					for (int32_t i = 0; i < static_cast<int32_t>(shader_effect.color_attachments.size()); ++i) {
						attachment_states[i] = shader_effect.color_attachments[i].to_vulkan();
					}

					color_blend_state_create_info.pAttachments = attachment_states.data();

					vk::PipelineViewportStateCreateInfo viewport_state_create_info{};
					vk::Viewport base_viewport{ 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
					viewport_state_create_info.viewportCount = 1u;
					viewport_state_create_info.pViewports = &base_viewport;
					vk::Rect2D base_scissor{ {}, {1u, 1u} };
					viewport_state_create_info.scissorCount = 1u;
					viewport_state_create_info.pScissors = &base_scissor;

					vk::DynamicState dynamic_states[] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
					vk::PipelineDynamicStateCreateInfo dynamic_state_create_info{};
					dynamic_state_create_info.dynamicStateCount = 2u;
					dynamic_state_create_info.pDynamicStates = dynamic_states;

					vk::PipelineRenderingCreateInfoKHR rendering_create_info{};
					rendering_create_info.colorAttachmentCount = static_cast<uint32_t>(shader_effect.attachment_formats.size());
					rendering_create_info.pColorAttachmentFormats = shader_effect.attachment_formats.data();
					rendering_create_info.depthAttachmentFormat = shader_effect.depth_format;
					rendering_create_info.stencilAttachmentFormat = shader_effect.stencil_format;

					vk::GraphicsPipelineCreateInfo create_info{};
					create_info.stageCount = static_cast<uint32_t>(shader_stages.size());
					create_info.pStages = shader_stages.data();
					create_info.pVertexInputState = &input_state_create_info;
					create_info.pInputAssemblyState = &input_assembly_state_create_info;
					create_info.pTessellationState = &tessellation_state_create_info;
					// Check can i not set it by default
					create_info.pViewportState = &viewport_state_create_info;
					create_info.pRasterizationState = &rasterization_state_create_info;
					create_info.pMultisampleState = &multisample_state_create_info;
					create_info.pDepthStencilState = &depth_stencil_state_create_info;
					create_info.pColorBlendState = &color_blend_state_create_info;
					create_info.pDynamicState = &dynamic_state_create_info;
					create_info.layout = pipeline_layout.get_handle();
					create_info.pNext = &rendering_create_info;

					auto const& device = ctx_->get_device();

					vk::Pipeline pipeline;
					if (auto result = device->createGraphicsPipelines(pipeline_cache_.get_handle(), 1u, &create_info, ctx_->get_allocator(), &pipeline); result != vk::Result::eSuccess) {
						EDGE_SLOGE("Failed to create graphics pipeline. Reason: {}", vk::to_string(result));
						return result;
					}

					pipelines_[mi::String(shader_effect.name)] = Pipeline{ &device, pipeline };
				}
				else if (shader_effect.bind_point == vk::PipelineBindPoint::eCompute) {
					vk::ComputePipelineCreateInfo create_info{};
					create_info.stage = shader_stages.front();

					auto const& device = ctx_->get_device();

					vk::Pipeline pipeline;
					if (auto result = device->createComputePipelines(pipeline_cache_.get_handle(), 1u, &create_info, ctx_->get_allocator(), &pipeline); result != vk::Result::eSuccess) {
						EDGE_SLOGE("Failed to create compute pipeline. Reason: {}", vk::to_string(result));
						return result;
					}

					pipelines_[mi::String(shader_effect.name)] = Pipeline{ &device, pipeline };
				}
			}
		}

		return vk::Result::eSuccess;
	}
}

#undef EDGE_LOGGER_SCOPE