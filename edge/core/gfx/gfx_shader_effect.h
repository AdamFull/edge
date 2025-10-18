#pragma once

#include <vulkan/vulkan.hpp>

#include "../foundation/foundation.h"

namespace edge::gfx {
	struct ShaderEffectHeader {
		char magic[4]{ 'S', 'H', 'F', 'X' };
		uint32_t version{ VK_MAKE_VERSION(0, 1, 0) };
		char compfmt[4]{ 'Z', 'S', 'T', 'D' };
		uint32_t flags;
	};

	struct TechniqueStage {
		vk::ShaderStageFlagBits stage;
		std::string entry_point_name;
		std::vector<uint8_t> code;

		void serialize(BinaryWriter& writer) const;
		void deserialize(BinaryReader& reader);
	};

	struct VertexInputBinding {
		uint16_t stride;
		uint8_t binding;
		uint8_t input_rate;

		auto to_vulkan() const noexcept -> vk::VertexInputBindingDescription {
			return { binding, stride, static_cast<vk::VertexInputRate>(input_rate) };
		}
	};

	struct VertexInputAttribute {
		uint8_t location;
		uint8_t binding;
		uint16_t format;
		uint32_t offset;

		auto to_vulkan() const noexcept -> vk::VertexInputAttributeDescription {
			return { location, binding, static_cast<vk::Format>(format), offset };
		}
	};

	union ColorAttachment {
		uint32_t bits;
		struct {
			uint32_t blend_enable : 1;
			uint32_t color_write_mask : 4;
			uint32_t src_color_blend_factor : 5;
			uint32_t dst_color_blend_factor : 5;
			uint32_t color_blend_op : 3;
			uint32_t src_alpha_blend_factor : 5;
			uint32_t dst_alpha_blend_factor : 5;
			uint32_t alpha_blend_op : 3;
		};

		auto to_vulkan() const noexcept -> vk::PipelineColorBlendAttachmentState {
			return {
				static_cast<vk::Bool32>(blend_enable),
				static_cast<vk::BlendFactor>(src_color_blend_factor),
				static_cast<vk::BlendFactor>(dst_color_blend_factor),
				static_cast<vk::BlendOp>(color_blend_op),
				static_cast<vk::BlendFactor>(src_alpha_blend_factor),
				static_cast<vk::BlendFactor>(dst_alpha_blend_factor),
				static_cast<vk::BlendOp>(alpha_blend_op),
				static_cast<vk::ColorComponentFlags>(color_write_mask)
			};
		}
	};

	struct PipelineStateHeader {
		uint8_t input_assembly_state_primitive_restart_enable : 1;
		uint8_t stencil_state_front_fail_op : 3;
		uint8_t rasterization_state_depth_clamp_enable : 1;
		uint8_t stencil_state_front_pass_op : 3;
		uint8_t rasterization_state_depth_bias_enable : 1;
		uint8_t stencil_state_front_depth_fail_op : 3;
		uint8_t rasterization_state_discard_enable : 1;
		uint8_t stencil_state_front_compare_op : 3;
		uint8_t multisample_state_sample_shading_enable : 1;
		uint8_t stencil_state_back_fail_op : 3;
		uint8_t multisample_state_alpha_to_coverage_enable : 1;
		uint8_t stencil_state_back_pass_op : 3;
		uint8_t multisample_state_alpha_to_one_enable : 1;
		uint8_t stencil_state_back_depth_fail_op : 3;
		uint8_t rasterization_state_front_face : 1;
		uint8_t stencil_state_back_compare_op : 3;
		uint8_t depth_state_depth_test_enable : 1;
		uint8_t depth_state_depth_compare_op : 3;
		uint8_t depth_state_depth_write_enable : 1;
		uint8_t multisample_state_sample_count : 7;
		uint8_t input_assembly_state_primitive_topology : 4;
		uint8_t color_blending_state_logic_op : 4;
		uint8_t rasterization_state_cull_mode : 2;
		uint8_t rasterization_state_polygon_mode : 2;

		uint8_t depth_state_depth_bounds_test_enable : 1;
		uint8_t stencil_state_stencil_test_enable : 1;
		uint8_t color_blending_state_logic_op_enable : 1;
		uint8_t color_blending_state_has_attachments : 1;

		uint8_t tessellation_state_control_points : 6;
		uint8_t vertex_input_state_has_bindings : 1;
		uint8_t vertex_input_state_has_attributes : 1;

		float multisample_state_min_sample_shading;

		uint32_t stencil_state_front_compare_mask;
		uint32_t stencil_state_back_compare_mask;
		uint32_t stencil_state_front_write_mask;
		uint32_t stencil_state_front_reference;
		uint32_t stencil_state_back_write_mask;
		uint32_t stencil_state_back_reference;
		float depth_state_min_depth_bounds;
		float depth_state_max_depth_bounds;

		float rasterization_state_depth_bias_constant_factor;
		float rasterization_state_depth_bias_clamp;
		float rasterization_state_depth_bias_slope_factor;
		float rasterization_state_line_width;

		std::array<float, 4> color_blending_state_blend_constants;

		inline auto get_input_assembly_state() const noexcept -> vk::PipelineInputAssemblyStateCreateInfo {
			return {
				{},
				static_cast<vk::PrimitiveTopology>(input_assembly_state_primitive_topology),
				static_cast<vk::Bool32>(input_assembly_state_primitive_restart_enable)
			};
		}

		inline auto get_tessellation_state() const noexcept -> vk::PipelineTessellationStateCreateInfo {
			return {
				{},
				static_cast<uint32_t>(tessellation_state_control_points)
			};
		}

		inline auto get_rasterization_state() const noexcept -> vk::PipelineRasterizationStateCreateInfo {
			return {
				{},
				static_cast<vk::Bool32>(rasterization_state_depth_clamp_enable),
				static_cast<vk::Bool32>(rasterization_state_discard_enable),
				static_cast<vk::PolygonMode>(rasterization_state_polygon_mode),
				static_cast<vk::CullModeFlags>(rasterization_state_cull_mode),
				static_cast<vk::FrontFace>(rasterization_state_front_face),
				static_cast<vk::Bool32>(rasterization_state_depth_bias_enable),
				rasterization_state_depth_bias_constant_factor,
				rasterization_state_depth_bias_clamp,
				rasterization_state_depth_bias_slope_factor,
				rasterization_state_line_width
			};
		}

		inline auto get_multisample_state() const noexcept -> vk::PipelineMultisampleStateCreateInfo {
			return {
				{},
				static_cast<vk::SampleCountFlagBits>(multisample_state_sample_count),
				static_cast<vk::Bool32>(multisample_state_sample_shading_enable),
				multisample_state_min_sample_shading,
				nullptr,
				static_cast<vk::Bool32>(multisample_state_alpha_to_coverage_enable),
				static_cast<vk::Bool32>(multisample_state_alpha_to_one_enable)
			};
		}

		inline auto get_depth_stencil_state() const noexcept -> vk::PipelineDepthStencilStateCreateInfo {
			return {
				{},
				static_cast<vk::Bool32>(depth_state_depth_test_enable),
				static_cast<vk::Bool32>(depth_state_depth_write_enable),
				static_cast<vk::CompareOp>(depth_state_depth_compare_op),
				static_cast<vk::Bool32>(depth_state_depth_bounds_test_enable),
				static_cast<vk::Bool32>(stencil_state_stencil_test_enable),
				{
					static_cast<vk::StencilOp>(stencil_state_front_fail_op),
					static_cast<vk::StencilOp>(stencil_state_front_pass_op),
					static_cast<vk::StencilOp>(stencil_state_front_depth_fail_op),
					static_cast<vk::CompareOp>(stencil_state_front_compare_op),
					stencil_state_front_compare_mask,
					stencil_state_front_write_mask,
					stencil_state_front_reference
				},
				{
					static_cast<vk::StencilOp>(stencil_state_back_fail_op),
					static_cast<vk::StencilOp>(stencil_state_back_pass_op),
					static_cast<vk::StencilOp>(stencil_state_back_depth_fail_op),
					static_cast<vk::CompareOp>(stencil_state_back_compare_op),
					stencil_state_back_compare_mask,
					stencil_state_back_write_mask,
					stencil_state_back_reference
				},
				depth_state_min_depth_bounds,
				depth_state_max_depth_bounds
			};
		}

		inline auto get_color_blending_state() const noexcept -> vk::PipelineColorBlendStateCreateInfo {
			return {
				{},
				static_cast<vk::Bool32>(color_blending_state_logic_op_enable),
				static_cast<vk::LogicOp>(color_blending_state_logic_op),
				0u,
				nullptr,
				color_blending_state_blend_constants
			};
		}
	};

	struct ShaderEffect {
		ShaderEffectHeader header;
		std::string name;
		vk::PipelineBindPoint bind_point;

		std::vector<TechniqueStage> stages;

		PipelineStateHeader pipeline_state;
		std::vector<gfx::ColorAttachment> color_attachments;
		std::vector<uint32_t> multisample_sample_masks{ 0x00000000 };
		std::vector<gfx::VertexInputAttribute> vertex_input_attributes;
		std::vector<gfx::VertexInputBinding> vertex_input_bindings;
		std::vector<vk::Format> attachment_formats;
		vk::Format depth_format{ vk::Format::eUndefined };
		vk::Format stencil_format{ vk::Format::eUndefined };

		void serialize(BinaryWriter& writer) const {
			writer.write(header);
			writer.write_string(name);
			writer.write(static_cast<uint32_t>(bind_point));
			writer.write_vector(stages);
			if (bind_point == vk::PipelineBindPoint::eGraphics) {
				writer.write(pipeline_state);

				writer.write(static_cast<uint32_t>(depth_format));
				writer.write(static_cast<uint32_t>(stencil_format));

				if (!color_attachments.empty()) {
					writer.write_vector(color_attachments);
					writer.write_vector(attachment_formats);
				}
				if (!multisample_sample_masks.empty()) {
					writer.write_vector(multisample_sample_masks);
				}
				if (!vertex_input_attributes.empty()) {
					writer.write_vector(vertex_input_attributes);
				}
				if (!vertex_input_bindings.empty()) {
					writer.write_vector(vertex_input_bindings);
				}
			}
		}

		void deserialize(BinaryReader& reader) {
			reader.read(header);
			name = reader.read_string();
			reader.read(bind_point);
			stages = reader.read_vector<TechniqueStage>();

			if (bind_point == vk::PipelineBindPoint::eGraphics) {
				reader.read(pipeline_state);

				depth_format = static_cast<vk::Format>(reader.read<uint32_t>());
				stencil_format = static_cast<vk::Format>(reader.read<uint32_t>());

				if (pipeline_state.color_blending_state_has_attachments) {
					color_attachments = reader.read_vector<ColorAttachment>();
					attachment_formats = reader.read_vector<vk::Format>();
				}

				if (pipeline_state.multisample_state_sample_count > 1) {
					multisample_sample_masks = reader.read_vector<uint32_t>();
				}

				if (pipeline_state.vertex_input_state_has_attributes) {
					vertex_input_attributes = reader.read_vector<VertexInputAttribute>();
				}

				if (pipeline_state.vertex_input_state_has_bindings) {
					vertex_input_bindings = reader.read_vector<VertexInputBinding>();
				}
			}
		}
	};
}