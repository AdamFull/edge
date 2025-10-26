#include "gfx_shader_library.h"

#include "../filesystem/filesystem.h"

#define EDGE_LOGGER_SCOPE "gfx::ShaderLibrary"

namespace edge::gfx {
	ShaderLibrary::~ShaderLibrary() {
		if (pipeline_cache_) {

			mi::Vector<uint8_t> pipeline_cache_data{};
			auto result = pipeline_cache_.get_data(pipeline_cache_data);
			if (result != vk::Result::eSuccess) {
				GFX_ASSERT_MSG(false, "Failed to read pipeline cache data.");
				return;
			}

			fs::OutputFileStream outfile(pipeline_cache_path_, std::ios_base::binary);
			if (!outfile.is_open()) {
				GFX_ASSERT_MSG(false, "Failed to open output file.");
				return;
			}

			outfile.write(reinterpret_cast<const char*>(pipeline_cache_data.data()), pipeline_cache_data.size());
		}
	}

	auto ShaderLibrary::construct(const ShaderLibraryInfo& info) -> Result<ShaderLibrary> {
		ShaderLibrary self{};
		self.pipeline_cache_path_ = info.pipeline_cache_path;
		self.pipeline_layout_ = info.pipeline_layout;
		self.backbuffer_format_ = info.backbuffer_format;
		if (auto result = self._construct(info); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}
		return self;
	}

	// TODO: build shader library in another function, for support in future hot reload
	auto ShaderLibrary::_construct(const ShaderLibraryInfo& info) -> vk::Result {
		GFX_ASSERT_MSG(pipeline_layout_, "PipelineLayout is null, but required.");
		GFX_ASSERT_MSG(!info.library_path.empty(), "Shaders path cannot be empty");

		// Load pipeline cache if exists
		mi::Vector<uint8_t> pipeline_cache_data{};

		fs::InputFileStream infile(pipeline_cache_path_, std::ios_base::binary);
		if (infile.is_open()) {
			pipeline_cache_data = { std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>() };
		}

		auto pipeline_cache_result = PipelineCache::create(pipeline_cache_data);
		if (!pipeline_cache_result) {
			GFX_ASSERT_MSG(false, "Failed to create pipeline cache.");
			return pipeline_cache_result.error();
		}
		pipeline_cache_ = std::move(pipeline_cache_result.value());

		static const char* entry_point_name = "main";

		// Load pipelines
		for (const auto& entry : fs::walk_directory(info.library_path)) {
			if (entry.is_directory) {
				continue;
			}

			auto ext = fs::path::extension(entry.path);
			if (ext.compare(u8".shfx") == 0) {
				auto shader_path = fs::path::append(info.library_path, entry.path);
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

					auto result = ShaderModule::create(stage.code);
					if (!result) {
						GFX_ASSERT_MSG(false, "Failed to create shader module at index {}, for effect \"{}\". Reason: {}.", i, shader_effect.name, vk::to_string(result.error()));
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

					mi::Vector<vk::Format> attachment_formats{};
					for (auto& format : shader_effect.attachment_formats) {
						// When format is undefined, it mean that expected backbuffer format
						if (format == vk::Format::eUndefined) {
							attachment_formats.push_back(backbuffer_format_);
							continue;
						}
						attachment_formats.push_back(format);
					}

					vk::PipelineRenderingCreateInfoKHR rendering_create_info{};
					rendering_create_info.colorAttachmentCount = static_cast<uint32_t>(attachment_formats.size());
					rendering_create_info.pColorAttachmentFormats = attachment_formats.data();
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
					create_info.layout = pipeline_layout_->get_handle();
					create_info.pNext = &rendering_create_info;

					vk::Pipeline pipeline;
					if (auto result = device_->createGraphicsPipelines(pipeline_cache_.get_handle(), 1u, &create_info, allocator_, &pipeline); result != vk::Result::eSuccess) {
						GFX_ASSERT_MSG(false, "Failed to create graphics pipeline. Reason: {}", vk::to_string(result));
						return result;
					}

					pipelines_[mi::String(shader_effect.name)] = Pipeline{ pipeline };
				}
				else if (shader_effect.bind_point == vk::PipelineBindPoint::eCompute) {
					vk::ComputePipelineCreateInfo create_info{};
					create_info.stage = shader_stages.front();
					create_info.layout = pipeline_layout_->get_handle();

					vk::Pipeline pipeline;
					if (auto result = device_->createComputePipelines(pipeline_cache_.get_handle(), 1u, &create_info, allocator_, &pipeline); result != vk::Result::eSuccess) {
						GFX_ASSERT_MSG(false, "Failed to create compute pipeline. Reason: {}", vk::to_string(result));
						return result;
					}

					pipelines_[mi::String(shader_effect.name)] = Pipeline{ pipeline };
				}
			}
		}

		return vk::Result::eSuccess;
	}
}

#undef EDGE_LOGGER_SCOPE