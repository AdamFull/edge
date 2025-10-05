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
		uint8_t pad0 : 1;

		uint8_t tessellation_state_control_points : 6;
		uint8_t pad1 : 2;
		uint8_t vertex_input_state_binding_count : 4;
		uint8_t vertex_input_state_attribute_count : 4;
		uint8_t color_blending_state_attachment_count : 4;
		uint8_t pad2 : 4;

		uint8_t pad3;

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

		float color_blending_state_blend_constants[4];
	};
}