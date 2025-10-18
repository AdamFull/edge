#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <ryml.hpp>
#include <c4/std/string.hpp>

#include <slang.h>
#include <slang-com-ptr.h>

#include <zstd.h>

#include <fstream>
#include <filesystem>
#include <optional>
#include <unordered_set>

#include "../../edge/core/gfx/gfx_shader_effect.h"

#define APPLICATION_NAME "shader_compiler"
#define LOG_FILE_NAME "shader_compiler.log"

struct Session {
	std::string input{};
	std::string output{};
	std::vector<std::string> include_directories;
	std::vector<std::string> definitions;
	std::string optimization_level{ "none" };
	std::string debug_level{ "maximal" };
	bool verbose{ false };
	bool compress{ true };
	std::string depfile_path{};
	std::set<std::string> dependencies;
	slang::IGlobalSession* slang_session{ nullptr };
};

static Session g_session;

using namespace edge;

namespace edge::gfx {
	void TechniqueStage::serialize(BinaryWriter& writer) const {
		writer.write(static_cast<uint32_t>(stage));
		writer.write_string(entry_point_name);

		if (g_session.compress) {
			const auto max_compressed_size = ZSTD_compressBound(code.size());
			mi::Vector<uint8_t> compressed_code(max_compressed_size, 0);

			const auto compressed_size = ZSTD_compress(compressed_code.data(), max_compressed_size, code.data(), code.size(), 15);
			compressed_code.resize(compressed_size);

			writer.write_vector(compressed_code);
		}
		else {
			writer.write_vector(code);
		}
	}
}

inline auto init_color_attachment(gfx::ColorAttachment& attachment) {
	attachment.blend_enable = 0;
	attachment.src_color_blend_factor = static_cast<uint8_t>(vk::BlendFactor::eZero);
	attachment.dst_color_blend_factor = static_cast<uint8_t>(vk::BlendFactor::eZero);
	attachment.color_blend_op = static_cast<uint8_t>(vk::BlendOp::eAdd);
	attachment.src_alpha_blend_factor = static_cast<uint8_t>(vk::BlendFactor::eZero);
	attachment.dst_alpha_blend_factor = static_cast<uint8_t>(vk::BlendFactor::eZero);
	attachment.alpha_blend_op = static_cast<uint8_t>(vk::BlendOp::eAdd);
	attachment.color_write_mask = 0;
}

inline auto init_pipeline_state_header(gfx::PipelineStateHeader& pipeline_state) {
	pipeline_state.vertex_input_state_has_bindings = 0;
	pipeline_state.vertex_input_state_has_attributes = 0;

	pipeline_state.input_assembly_state_primitive_topology = static_cast<uint8_t>(vk::PrimitiveTopology::ePointList);
	pipeline_state.input_assembly_state_primitive_restart_enable = 0;

	pipeline_state.tessellation_state_control_points = 0;

	pipeline_state.rasterization_state_depth_clamp_enable = 0;
	pipeline_state.rasterization_state_discard_enable = 0;
	pipeline_state.rasterization_state_polygon_mode = static_cast<uint8_t>(vk::PolygonMode::eFill);
	pipeline_state.rasterization_state_cull_mode = static_cast<uint8_t>(vk::CullModeFlagBits::eNone);
	pipeline_state.rasterization_state_front_face = static_cast<uint8_t>(vk::FrontFace::eCounterClockwise);
	pipeline_state.rasterization_state_depth_bias_enable = 0;
	pipeline_state.rasterization_state_depth_bias_constant_factor = 0.0f;
	pipeline_state.rasterization_state_depth_bias_clamp = 0.0f;
	pipeline_state.rasterization_state_depth_bias_slope_factor = 0.0f;
	pipeline_state.rasterization_state_line_width = 1.0f;

	pipeline_state.multisample_state_sample_count = static_cast<uint8_t>(vk::SampleCountFlagBits::e1);
	pipeline_state.multisample_state_sample_shading_enable = 0;
	pipeline_state.multisample_state_min_sample_shading = 0;
	pipeline_state.multisample_state_alpha_to_coverage_enable = 0; 
	pipeline_state.multisample_state_alpha_to_one_enable = 0;

	pipeline_state.depth_state_depth_test_enable = 0;
	pipeline_state.depth_state_depth_write_enable = 0;
	pipeline_state.depth_state_depth_compare_op = static_cast<uint8_t>(vk::CompareOp::eNever);
	pipeline_state.depth_state_depth_bounds_test_enable = 0;
	pipeline_state.depth_state_min_depth_bounds = 0.0f;
	pipeline_state.depth_state_max_depth_bounds = 0.0f;
	pipeline_state.stencil_state_stencil_test_enable = 0;
	pipeline_state.stencil_state_front_fail_op = static_cast<uint8_t>(vk::StencilOp::eKeep);
	pipeline_state.stencil_state_front_pass_op = static_cast<uint8_t>(vk::StencilOp::eKeep);
	pipeline_state.stencil_state_front_depth_fail_op = static_cast<uint8_t>(vk::StencilOp::eKeep);
	pipeline_state.stencil_state_front_compare_op = static_cast<uint8_t>(vk::CompareOp::eNever);
	pipeline_state.stencil_state_front_compare_mask = 0;
	pipeline_state.stencil_state_front_write_mask = 0;
	pipeline_state.stencil_state_front_reference = 0;
	pipeline_state.stencil_state_back_fail_op = static_cast<uint8_t>(vk::StencilOp::eKeep);
	pipeline_state.stencil_state_back_pass_op = static_cast<uint8_t>(vk::StencilOp::eKeep);
	pipeline_state.stencil_state_back_depth_fail_op = static_cast<uint8_t>(vk::StencilOp::eKeep);
	pipeline_state.stencil_state_back_compare_op = static_cast<uint8_t>(vk::CompareOp::eNever);
	pipeline_state.stencil_state_back_compare_mask = 0;
	pipeline_state.stencil_state_back_write_mask = 0;
	pipeline_state.stencil_state_back_reference = 0;
	
	pipeline_state.color_blending_state_logic_op_enable = 0;
	pipeline_state.color_blending_state_logic_op = static_cast<uint8_t>(vk::LogicOp::eClear);
	pipeline_state.color_blending_state_has_attachments = 0;
	memset(pipeline_state.color_blending_state_blend_constants.data(), 0, sizeof(float) * 4ull);
}

inline auto parse_fill_mode(const std::string& mode) -> vk::PolygonMode {
	static const std::unordered_map<std::string, vk::PolygonMode> map = {
			{"fill", vk::PolygonMode::eFill},
			{"line", vk::PolygonMode::eLine},
			{"point", vk::PolygonMode::ePoint}
	};
	auto it = map.find(mode);
	if (it == map.end()) {
		spdlog::warn("Unknown polygon mode type: \"{}\".", mode);
		return vk::PolygonMode::eFill;
	}
	return it->second;
}

inline auto parse_cull_mode(const std::string& mode) -> vk::CullModeFlags {
	static const std::unordered_map<std::string, vk::CullModeFlags> map = {
		{"none", vk::CullModeFlagBits::eNone},
		{"front", vk::CullModeFlagBits::eFront},
		{"back", vk::CullModeFlagBits::eBack},
		{"front_and_back", vk::CullModeFlagBits::eFrontAndBack}
	};
	auto it = map.find(mode);
	if (it == map.end()) {
		spdlog::warn("Unknown cull mode type: \"{}\".", mode);
		return vk::CullModeFlagBits::eNone;
	}
	return it->second;
}

inline auto parse_front_face(const std::string& mode) -> vk::FrontFace {
	static const std::unordered_map<std::string, vk::FrontFace> map = {
		{"ccw", vk::FrontFace::eCounterClockwise},
		{"cw", vk::FrontFace::eClockwise}
	};
	auto it = map.find(mode);
	if (it == map.end()) {
		spdlog::warn("Unknown front face type: \"{}\".", mode);
		return vk::FrontFace::eCounterClockwise;
	}
	return it->second;
}

inline auto parse_compare_op(const std::string& op) -> vk::CompareOp {
	static const std::unordered_map<std::string, vk::CompareOp> map = {
		{"never", vk::CompareOp::eNever},
		{"less", vk::CompareOp::eLess},
		{"equal", vk::CompareOp::eEqual},
		{"less_or_equal", vk::CompareOp::eLessOrEqual},
		{"greater", vk::CompareOp::eGreater},
		{"not_equal", vk::CompareOp::eNotEqual},
		{"greater_or_equal", vk::CompareOp::eGreaterOrEqual},
		{"always", vk::CompareOp::eAlways}
	};
	auto it = map.find(op);
	if (it == map.end()) {
		spdlog::warn("Unknown compare op: \"{}\".", op);
		return vk::CompareOp::eNever;
	}
	return it->second;
}

inline auto parse_stencil_op(const std::string& op) -> vk::StencilOp {
	static const std::unordered_map<std::string, vk::StencilOp> map = {
		{"keep", vk::StencilOp::eKeep},
		{"zero", vk::StencilOp::eZero},
		{"replace", vk::StencilOp::eReplace},
		{"increment_and_clamp", vk::StencilOp::eIncrementAndClamp},
		{"decrement_and_clamp", vk::StencilOp::eDecrementAndClamp},
		{"invert", vk::StencilOp::eInvert},
		{"increment_and_wrap", vk::StencilOp::eIncrementAndWrap},
		{"decrement_and_wrap", vk::StencilOp::eDecrementAndWrap}
	};
	auto it = map.find(op);
	if (it == map.end()) {
		spdlog::warn("Unknown stencil op: \"{}\".", op);
		return vk::StencilOp::eKeep;
	}
	return it->second;
}

inline auto parse_logic_op(const std::string& op) -> vk::LogicOp {
	static const std::unordered_map<std::string, vk::LogicOp> map = {
		{"clear", vk::LogicOp::eClear},
		{"and", vk::LogicOp::eAnd},
		{"and_reverse", vk::LogicOp::eAndReverse},
		{"copy", vk::LogicOp::eCopy},
		{"and_inverted", vk::LogicOp::eAndInverted},
		{"no_op", vk::LogicOp::eNoOp},
		{"xor", vk::LogicOp::eXor},
		{"or", vk::LogicOp::eOr},
		{"nor", vk::LogicOp::eNor},
		{"equivalent", vk::LogicOp::eEquivalent},
		{"invert", vk::LogicOp::eInvert},
		{"or_reverse", vk::LogicOp::eOrReverse},
		{"copy_inverted", vk::LogicOp::eCopyInverted},
		{"or_inverted", vk::LogicOp::eOrInverted},
		{"nand", vk::LogicOp::eNand},
		{"set", vk::LogicOp::eSet}
	};
	auto it = map.find(op);
	if (it == map.end()) {
		spdlog::warn("Unknown logic op: \"{}\".", op);
		return vk::LogicOp::eClear;
	}
	return it->second;
}

inline auto parse_blend_factor(const std::string& factor) -> vk::BlendFactor {
	static const std::unordered_map<std::string, vk::BlendFactor> map = {
		{"zero", vk::BlendFactor::eZero},
		{"one", vk::BlendFactor::eOne},
		{"src_color", vk::BlendFactor::eSrcColor},
		{"one_minus_src_color", vk::BlendFactor::eOneMinusSrcColor},
		{"dst_color", vk::BlendFactor::eDstColor},
		{"one_minus_dst_color", vk::BlendFactor::eOneMinusDstColor},
		{"src_alpha", vk::BlendFactor::eSrcAlpha},
		{"one_minus_src_alpha", vk::BlendFactor::eOneMinusSrcAlpha},
		{"dst_alpha", vk::BlendFactor::eDstAlpha},
		{"one_minus_dst_alpha", vk::BlendFactor::eOneMinusDstAlpha},
		{"constant_color", vk::BlendFactor::eConstantColor},
		{"one_minus_constant_color", vk::BlendFactor::eOneMinusConstantColor},
		{"constant_alpha", vk::BlendFactor::eConstantAlpha},
		{"one_minus_constant_alpha", vk::BlendFactor::eOneMinusConstantAlpha},
		{"src_alpha_saturate", vk::BlendFactor::eSrcAlphaSaturate}
	};
	auto it = map.find(factor);
	if (it == map.end()) {
		spdlog::warn("Unknown blend factor: \"{}\".", factor);
		return vk::BlendFactor::eZero;
	}
	return it->second;
}

inline auto parse_blend_op(const std::string& op) -> vk::BlendOp {
	static const std::unordered_map<std::string, vk::BlendOp> map = {
		{"add", vk::BlendOp::eAdd},
		{"subtract", vk::BlendOp::eSubtract},
		{"reverse_subtract", vk::BlendOp::eReverseSubtract},
		{"min", vk::BlendOp::eMin},
		{"max", vk::BlendOp::eMax}
	};
	auto it = map.find(op);
	if (it == map.end()) {
		spdlog::warn("Unknown blend op: \"{}\".", op);
		return vk::BlendOp::eAdd;
	}
	return it->second;
}

inline auto parse_primitive_topology(const std::string& topology) -> vk::PrimitiveTopology {
	static const std::unordered_map<std::string, vk::PrimitiveTopology> map = {
		{"point_list", vk::PrimitiveTopology::ePointList},
		{"line_list", vk::PrimitiveTopology::eLineList},
		{"line_strip", vk::PrimitiveTopology::eLineStrip},
		{"triangle_list", vk::PrimitiveTopology::eTriangleList},
		{"triangle_strip", vk::PrimitiveTopology::eTriangleStrip},
		{"triangle_fan", vk::PrimitiveTopology::eTriangleFan},
		{"line_list_with_adjacency", vk::PrimitiveTopology::eLineListWithAdjacency},
		{"line_strip_with_adjacency", vk::PrimitiveTopology::eLineStripWithAdjacency},
		{"triangle_list_with_adjacency", vk::PrimitiveTopology::eTriangleListWithAdjacency},
		{"triangle_strip_with_adjacency", vk::PrimitiveTopology::eTriangleStripWithAdjacency},
		{"patch_list", vk::PrimitiveTopology::ePatchList}
	};
	auto it = map.find(topology);
	if (it == map.end()) {
		spdlog::warn("Unknown primitive topology: \"{}\".", topology);
		return vk::PrimitiveTopology::ePointList;
	}
	return it->second;
}

inline auto parse_pipeline_bind_point(const std::string& bind_point) -> vk::PipelineBindPoint {
	static const std::unordered_map<std::string, vk::PipelineBindPoint> map = {
		{"graphics", vk::PipelineBindPoint::eGraphics},
		{"compute", vk::PipelineBindPoint::eCompute},
		{"ray_tracing", vk::PipelineBindPoint::eRayTracingKHR}
	};
	auto it = map.find(bind_point);
	if (it == map.end()) {
		spdlog::warn("Unknown pipeline type: \"{}\".", bind_point);
		return vk::PipelineBindPoint::eGraphics;
	}
	return it->second;
}

inline auto parse_format(const std::string& format) -> vk::Format {
	static const std::unordered_map<std::string, vk::Format> map = {
		{"undefined", vk::Format::eUndefined},
		{ "eR4G4UnormPack8", vk::Format::eR4G4UnormPack8 },
		{ "eR4G4B4A4UnormPack16", vk::Format::eR4G4B4A4UnormPack16 },
		{ "eB4G4R4A4UnormPack16", vk::Format::eB4G4R4A4UnormPack16 },
		{ "eR5G6B5UnormPack16", vk::Format::eR5G6B5UnormPack16 },
		{ "eB5G6R5UnormPack16", vk::Format::eB5G6R5UnormPack16 },
		{ "eR5G5B5A1UnormPack16", vk::Format::eR5G5B5A1UnormPack16 },
		{ "eB5G5R5A1UnormPack16", vk::Format::eB5G5R5A1UnormPack16 },
		{ "eA1R5G5B5UnormPack16", vk::Format::eA1R5G5B5UnormPack16 },
		{ "eR8Unorm", vk::Format::eR8Unorm },
		{ "eR8Snorm", vk::Format::eR8Snorm },
		{ "eR8Uscaled", vk::Format::eR8Uscaled },
		{ "eR8Sscaled", vk::Format::eR8Sscaled },
		{ "eR8Uint", vk::Format::eR8Uint },
		{ "eR8Sint", vk::Format::eR8Sint },
		{ "eR8Srgb", vk::Format::eR8Srgb },
		{ "eR8G8Unorm", vk::Format::eR8G8Unorm },
		{ "eR8G8Snorm", vk::Format::eR8G8Snorm },
		{ "eR8G8Uscaled", vk::Format::eR8G8Uscaled },
		{ "eR8G8Sscaled", vk::Format::eR8G8Sscaled },
		{ "eR8G8Uint", vk::Format::eR8G8Uint },
		{ "eR8G8Sint", vk::Format::eR8G8Sint },
		{ "eR8G8Srgb", vk::Format::eR8G8Srgb },
		{ "eR8G8B8Unorm", vk::Format::eR8G8B8Unorm },
		{ "eR8G8B8Snorm", vk::Format::eR8G8B8Snorm },
		{ "eR8G8B8Uscaled", vk::Format::eR8G8B8Uscaled },
		{ "eR8G8B8Sscaled", vk::Format::eR8G8B8Sscaled },
		{ "eR8G8B8Uint", vk::Format::eR8G8B8Uint },
		{ "eR8G8B8Sint", vk::Format::eR8G8B8Sint },
		{ "eR8G8B8Srgb", vk::Format::eR8G8B8Srgb },
		{ "eB8G8R8Unorm", vk::Format::eB8G8R8Unorm },
		{ "eB8G8R8Snorm", vk::Format::eB8G8R8Snorm },
		{ "eB8G8R8Uscaled", vk::Format::eB8G8R8Uscaled },
		{ "eB8G8R8Sscaled", vk::Format::eB8G8R8Sscaled },
		{ "eB8G8R8Uint", vk::Format::eB8G8R8Uint },
		{ "eB8G8R8Sint", vk::Format::eB8G8R8Sint },
		{ "eB8G8R8Srgb", vk::Format::eB8G8R8Srgb },
		{ "eR8G8B8A8Unorm", vk::Format::eR8G8B8A8Unorm },
		{ "eR8G8B8A8Snorm", vk::Format::eR8G8B8A8Snorm },
		{ "eR8G8B8A8Uscaled", vk::Format::eR8G8B8A8Uscaled },
		{ "eR8G8B8A8Sscaled", vk::Format::eR8G8B8A8Sscaled },
		{ "eR8G8B8A8Uint", vk::Format::eR8G8B8A8Uint },
		{ "eR8G8B8A8Sint", vk::Format::eR8G8B8A8Sint },
		{ "eR8G8B8A8Srgb", vk::Format::eR8G8B8A8Srgb },
		{ "eB8G8R8A8Unorm", vk::Format::eB8G8R8A8Unorm },
		{ "eB8G8R8A8Snorm", vk::Format::eB8G8R8A8Snorm },
		{ "eB8G8R8A8Uscaled", vk::Format::eB8G8R8A8Uscaled },
		{ "eB8G8R8A8Sscaled", vk::Format::eB8G8R8A8Sscaled },
		{ "eB8G8R8A8Uint", vk::Format::eB8G8R8A8Uint },
		{ "eB8G8R8A8Sint", vk::Format::eB8G8R8A8Sint },
		{ "eB8G8R8A8Srgb", vk::Format::eB8G8R8A8Srgb },
		{ "eA8B8G8R8UnormPack32", vk::Format::eA8B8G8R8UnormPack32 },
		{ "eA8B8G8R8SnormPack32", vk::Format::eA8B8G8R8SnormPack32 },
		{ "eA8B8G8R8UscaledPack32", vk::Format::eA8B8G8R8UscaledPack32 },
		{ "eA8B8G8R8SscaledPack32", vk::Format::eA8B8G8R8SscaledPack32 },
		{ "eA8B8G8R8UintPack32", vk::Format::eA8B8G8R8UintPack32 },
		{ "eA8B8G8R8SintPack32", vk::Format::eA8B8G8R8SintPack32 },
		{ "eA8B8G8R8SrgbPack32", vk::Format::eA8B8G8R8SrgbPack32 },
		{ "eA2R10G10B10UnormPack32", vk::Format::eA2R10G10B10UnormPack32 },
		{ "eA2R10G10B10SnormPack32", vk::Format::eA2R10G10B10SnormPack32 },
		{ "eA2R10G10B10UscaledPack32", vk::Format::eA2R10G10B10UscaledPack32 },
		{ "eA2R10G10B10SscaledPack32", vk::Format::eA2R10G10B10SscaledPack32 },
		{ "eA2R10G10B10UintPack32", vk::Format::eA2R10G10B10UintPack32 },
		{ "eA2R10G10B10SintPack32", vk::Format::eA2R10G10B10SintPack32 },
		{ "eA2B10G10R10UnormPack32", vk::Format::eA2B10G10R10UnormPack32 },
		{ "eA2B10G10R10SnormPack32", vk::Format::eA2B10G10R10SnormPack32 },
		{ "eA2B10G10R10UscaledPack32", vk::Format::eA2B10G10R10UscaledPack32 },
		{ "eA2B10G10R10SscaledPack32", vk::Format::eA2B10G10R10SscaledPack32 },
		{ "eA2B10G10R10UintPack32", vk::Format::eA2B10G10R10UintPack32 },
		{ "eA2B10G10R10SintPack32", vk::Format::eA2B10G10R10SintPack32 },
		{ "eR16Unorm", vk::Format::eR16Unorm },
		{ "eR16Snorm", vk::Format::eR16Snorm },
		{ "eR16Uscaled", vk::Format::eR16Uscaled },
		{ "eR16Sscaled", vk::Format::eR16Sscaled },
		{ "eR16Uint", vk::Format::eR16Uint },
		{ "eR16Sint", vk::Format::eR16Sint },
		{ "eR16Sfloat", vk::Format::eR16Sfloat },
		{ "eR16G16Unorm", vk::Format::eR16G16Unorm },
		{ "eR16G16Snorm", vk::Format::eR16G16Snorm },
		{ "eR16G16Uscaled", vk::Format::eR16G16Uscaled },
		{ "eR16G16Sscaled", vk::Format::eR16G16Sscaled },
		{ "eR16G16Uint", vk::Format::eR16G16Uint },
		{ "eR16G16Sint", vk::Format::eR16G16Sint },
		{ "eR16G16Sfloat", vk::Format::eR16G16Sfloat },
		{ "eR16G16B16Unorm", vk::Format::eR16G16B16Unorm },
		{ "eR16G16B16Snorm", vk::Format::eR16G16B16Snorm },
		{ "eR16G16B16Uscaled", vk::Format::eR16G16B16Uscaled },
		{ "eR16G16B16Sscaled", vk::Format::eR16G16B16Sscaled },
		{ "eR16G16B16Uint", vk::Format::eR16G16B16Uint },
		{ "eR16G16B16Sint", vk::Format::eR16G16B16Sint },
		{ "eR16G16B16Sfloat", vk::Format::eR16G16B16Sfloat },
		{ "eR16G16B16A16Unorm", vk::Format::eR16G16B16A16Unorm },
		{ "eR16G16B16A16Snorm", vk::Format::eR16G16B16A16Snorm },
		{ "eR16G16B16A16Uscaled", vk::Format::eR16G16B16A16Uscaled },
		{ "eR16G16B16A16Sscaled", vk::Format::eR16G16B16A16Sscaled },
		{ "eR16G16B16A16Uint", vk::Format::eR16G16B16A16Uint },
		{ "eR16G16B16A16Sint", vk::Format::eR16G16B16A16Sint },
		{ "eR16G16B16A16Sfloat", vk::Format::eR16G16B16A16Sfloat },
		{ "eR32Uint", vk::Format::eR32Uint },
		{ "eR32Sint", vk::Format::eR32Sint },
		{ "eR32Sfloat", vk::Format::eR32Sfloat },
		{ "eR32G32Uint", vk::Format::eR32G32Uint },
		{ "eR32G32Sint", vk::Format::eR32G32Sint },
		{ "eR32G32Sfloat", vk::Format::eR32G32Sfloat },
		{ "eR32G32B32Uint", vk::Format::eR32G32B32Uint },
		{ "eR32G32B32Sint", vk::Format::eR32G32B32Sint },
		{ "eR32G32B32Sfloat", vk::Format::eR32G32B32Sfloat },
		{ "eR32G32B32A32Uint", vk::Format::eR32G32B32A32Uint },
		{ "eR32G32B32A32Sint", vk::Format::eR32G32B32A32Sint },
		{ "eR32G32B32A32Sfloat", vk::Format::eR32G32B32A32Sfloat },
		{ "eR64Uint", vk::Format::eR64Uint },
		{ "eR64Sint", vk::Format::eR64Sint },
		{ "eR64Sfloat", vk::Format::eR64Sfloat },
		{ "eR64G64Uint", vk::Format::eR64G64Uint },
		{ "eR64G64Sint", vk::Format::eR64G64Sint },
		{ "eR64G64Sfloat", vk::Format::eR64G64Sfloat },
		{ "eR64G64B64Uint", vk::Format::eR64G64B64Uint },
		{ "eR64G64B64Sint", vk::Format::eR64G64B64Sint },
		{ "eR64G64B64Sfloat", vk::Format::eR64G64B64Sfloat },
		{ "eR64G64B64A64Uint", vk::Format::eR64G64B64A64Uint },
		{ "eR64G64B64A64Sint", vk::Format::eR64G64B64A64Sint },
		{ "eR64G64B64A64Sfloat", vk::Format::eR64G64B64A64Sfloat },
		{ "eB10G11R11UfloatPack32", vk::Format::eB10G11R11UfloatPack32 },
		{ "eE5B9G9R9UfloatPack32", vk::Format::eE5B9G9R9UfloatPack32 },
		{ "eD16Unorm", vk::Format::eD16Unorm },
		{ "eX8D24UnormPack32", vk::Format::eX8D24UnormPack32 },
		{ "eD32Sfloat", vk::Format::eD32Sfloat },
		{ "eS8Uint", vk::Format::eS8Uint },
		{ "eD16UnormS8Uint", vk::Format::eD16UnormS8Uint },
		{ "eD24UnormS8Uint", vk::Format::eD24UnormS8Uint },
		{ "eD32SfloatS8Uint", vk::Format::eD32SfloatS8Uint },
		{ "eBc1RgbUnormBlock", vk::Format::eBc1RgbUnormBlock },
		{ "eBc1RgbSrgbBlock", vk::Format::eBc1RgbSrgbBlock },
		{ "eBc1RgbaUnormBlock", vk::Format::eBc1RgbaUnormBlock },
		{ "eBc1RgbaSrgbBlock", vk::Format::eBc1RgbaSrgbBlock },
		{ "eBc2UnormBlock", vk::Format::eBc2UnormBlock },
		{ "eBc2SrgbBlock", vk::Format::eBc2SrgbBlock },
		{ "eBc3UnormBlock", vk::Format::eBc3UnormBlock },
		{ "eBc3SrgbBlock", vk::Format::eBc3SrgbBlock },
		{ "eBc4UnormBlock", vk::Format::eBc4UnormBlock },
		{ "eBc4SnormBlock", vk::Format::eBc4SnormBlock },
		{ "eBc5UnormBlock", vk::Format::eBc5UnormBlock },
		{ "eBc5SnormBlock", vk::Format::eBc5SnormBlock },
		{ "eBc6HUfloatBlock", vk::Format::eBc6HUfloatBlock },
		{ "eBc6HSfloatBlock", vk::Format::eBc6HSfloatBlock },
		{ "eBc7UnormBlock", vk::Format::eBc7UnormBlock },
		{ "eBc7SrgbBlock", vk::Format::eBc7SrgbBlock },
		{ "eEtc2R8G8B8UnormBlock", vk::Format::eEtc2R8G8B8UnormBlock },
		{ "eEtc2R8G8B8SrgbBlock", vk::Format::eEtc2R8G8B8SrgbBlock },
		{ "eEtc2R8G8B8A1UnormBlock", vk::Format::eEtc2R8G8B8A1UnormBlock },
		{ "eEtc2R8G8B8A1SrgbBlock", vk::Format::eEtc2R8G8B8A1SrgbBlock },
		{ "eEtc2R8G8B8A8UnormBlock", vk::Format::eEtc2R8G8B8A8UnormBlock },
		{ "eEtc2R8G8B8A8SrgbBlock", vk::Format::eEtc2R8G8B8A8SrgbBlock },
		{ "eEacR11UnormBlock", vk::Format::eEacR11UnormBlock },
		{ "eEacR11SnormBlock", vk::Format::eEacR11SnormBlock },
		{ "eEacR11G11UnormBlock", vk::Format::eEacR11G11UnormBlock },
		{ "eEacR11G11SnormBlock", vk::Format::eEacR11G11SnormBlock },
		{ "eAstc4x4UnormBlock", vk::Format::eAstc4x4UnormBlock },
		{ "eAstc4x4SrgbBlock", vk::Format::eAstc4x4SrgbBlock },
		{ "eAstc5x4UnormBlock", vk::Format::eAstc5x4UnormBlock },
		{ "eAstc5x4SrgbBlock", vk::Format::eAstc5x4SrgbBlock },
		{ "eAstc5x5UnormBlock", vk::Format::eAstc5x5UnormBlock },
		{ "eAstc5x5SrgbBlock", vk::Format::eAstc5x5SrgbBlock },
		{ "eAstc6x5UnormBlock", vk::Format::eAstc6x5UnormBlock },
		{ "eAstc6x5SrgbBlock", vk::Format::eAstc6x5SrgbBlock },
		{ "eAstc6x6UnormBlock", vk::Format::eAstc6x6UnormBlock },
		{ "eAstc6x6SrgbBlock", vk::Format::eAstc6x6SrgbBlock },
		{ "eAstc8x5UnormBlock", vk::Format::eAstc8x5UnormBlock },
		{ "eAstc8x5SrgbBlock", vk::Format::eAstc8x5SrgbBlock },
		{ "eAstc8x6UnormBlock", vk::Format::eAstc8x6UnormBlock },
		{ "eAstc8x6SrgbBlock", vk::Format::eAstc8x6SrgbBlock },
		{ "eAstc8x8UnormBlock", vk::Format::eAstc8x8UnormBlock },
		{ "eAstc8x8SrgbBlock", vk::Format::eAstc8x8SrgbBlock },
		{ "eAstc10x5UnormBlock", vk::Format::eAstc10x5UnormBlock },
		{ "eAstc10x5SrgbBlock", vk::Format::eAstc10x5SrgbBlock },
		{ "eAstc10x6UnormBlock", vk::Format::eAstc10x6UnormBlock },
		{ "eAstc10x6SrgbBlock", vk::Format::eAstc10x6SrgbBlock },
		{ "eAstc10x8UnormBlock", vk::Format::eAstc10x8UnormBlock },
		{ "eAstc10x8SrgbBlock", vk::Format::eAstc10x8SrgbBlock },
		{ "eAstc10x10UnormBlock", vk::Format::eAstc10x10UnormBlock },
		{ "eAstc10x10SrgbBlock", vk::Format::eAstc10x10SrgbBlock },
		{ "eAstc12x10UnormBlock", vk::Format::eAstc12x10UnormBlock },
		{ "eAstc12x10SrgbBlock", vk::Format::eAstc12x10SrgbBlock },
		{ "eAstc12x12UnormBlock", vk::Format::eAstc12x12UnormBlock },
		{ "eAstc12x12SrgbBlock", vk::Format::eAstc12x12SrgbBlock },
		{ "eG8B8G8R8422Unorm", vk::Format::eG8B8G8R8422Unorm },
		{ "eG8B8G8R8422UnormKHR", vk::Format::eG8B8G8R8422UnormKHR },
		{ "eB8G8R8G8422Unorm", vk::Format::eB8G8R8G8422Unorm },
		{ "eB8G8R8G8422UnormKHR", vk::Format::eB8G8R8G8422UnormKHR },
		{ "eG8B8R83Plane420Unorm", vk::Format::eG8B8R83Plane420Unorm },
		{ "eG8B8R83Plane420UnormKHR", vk::Format::eG8B8R83Plane420UnormKHR },
		{ "eG8B8R82Plane420Unorm", vk::Format::eG8B8R82Plane420Unorm },
		{ "eG8B8R82Plane420UnormKHR", vk::Format::eG8B8R82Plane420UnormKHR },
		{ "eG8B8R83Plane422Unorm", vk::Format::eG8B8R83Plane422Unorm },
		{ "eG8B8R83Plane422UnormKHR", vk::Format::eG8B8R83Plane422UnormKHR },
		{ "eG8B8R82Plane422Unorm", vk::Format::eG8B8R82Plane422Unorm },
		{ "eG8B8R82Plane422UnormKHR", vk::Format::eG8B8R82Plane422UnormKHR },
		{ "eG8B8R83Plane444Unorm", vk::Format::eG8B8R83Plane444Unorm },
		{ "eG8B8R83Plane444UnormKHR", vk::Format::eG8B8R83Plane444UnormKHR },
		{ "eR10X6UnormPack16", vk::Format::eR10X6UnormPack16 },
		{ "eR10X6UnormPack16KHR", vk::Format::eR10X6UnormPack16KHR },
		{ "eR10X6G10X6Unorm2Pack16", vk::Format::eR10X6G10X6Unorm2Pack16 },
		{ "eR10X6G10X6Unorm2Pack16KHR", vk::Format::eR10X6G10X6Unorm2Pack16KHR },
		{ "eR10X6G10X6B10X6A10X6Unorm4Pack16", vk::Format::eR10X6G10X6B10X6A10X6Unorm4Pack16 },
		{ "eR10X6G10X6B10X6A10X6Unorm4Pack16KHR", vk::Format::eR10X6G10X6B10X6A10X6Unorm4Pack16KHR },
		{ "eG10X6B10X6G10X6R10X6422Unorm4Pack16", vk::Format::eG10X6B10X6G10X6R10X6422Unorm4Pack16 },
		{ "eG10X6B10X6G10X6R10X6422Unorm4Pack16KHR", vk::Format::eG10X6B10X6G10X6R10X6422Unorm4Pack16KHR },
		{ "eB10X6G10X6R10X6G10X6422Unorm4Pack16", vk::Format::eB10X6G10X6R10X6G10X6422Unorm4Pack16 },
		{ "eB10X6G10X6R10X6G10X6422Unorm4Pack16KHR", vk::Format::eB10X6G10X6R10X6G10X6422Unorm4Pack16KHR },
		{ "eG10X6B10X6R10X63Plane420Unorm3Pack16", vk::Format::eG10X6B10X6R10X63Plane420Unorm3Pack16 },
		{ "eG10X6B10X6R10X63Plane420Unorm3Pack16KHR", vk::Format::eG10X6B10X6R10X63Plane420Unorm3Pack16KHR },
		{ "eG10X6B10X6R10X62Plane420Unorm3Pack16", vk::Format::eG10X6B10X6R10X62Plane420Unorm3Pack16 },
		{ "eG10X6B10X6R10X62Plane420Unorm3Pack16KHR", vk::Format::eG10X6B10X6R10X62Plane420Unorm3Pack16KHR },
		{ "eG10X6B10X6R10X63Plane422Unorm3Pack16", vk::Format::eG10X6B10X6R10X63Plane422Unorm3Pack16 },
		{ "eG10X6B10X6R10X63Plane422Unorm3Pack16KHR", vk::Format::eG10X6B10X6R10X63Plane422Unorm3Pack16KHR },
		{ "eG10X6B10X6R10X62Plane422Unorm3Pack16", vk::Format::eG10X6B10X6R10X62Plane422Unorm3Pack16 },
		{ "eG10X6B10X6R10X62Plane422Unorm3Pack16KHR", vk::Format::eG10X6B10X6R10X62Plane422Unorm3Pack16KHR },
		{ "eG10X6B10X6R10X63Plane444Unorm3Pack16", vk::Format::eG10X6B10X6R10X63Plane444Unorm3Pack16 },
		{ "eG10X6B10X6R10X63Plane444Unorm3Pack16KHR", vk::Format::eG10X6B10X6R10X63Plane444Unorm3Pack16KHR },
		{ "eR12X4UnormPack16", vk::Format::eR12X4UnormPack16 },
		{ "eR12X4UnormPack16KHR", vk::Format::eR12X4UnormPack16KHR },
		{ "eR12X4G12X4Unorm2Pack16", vk::Format::eR12X4G12X4Unorm2Pack16 },
		{ "eR12X4G12X4Unorm2Pack16KHR", vk::Format::eR12X4G12X4Unorm2Pack16KHR },
		{ "eR12X4G12X4B12X4A12X4Unorm4Pack16", vk::Format::eR12X4G12X4B12X4A12X4Unorm4Pack16 },
		{ "eR12X4G12X4B12X4A12X4Unorm4Pack16KHR", vk::Format::eR12X4G12X4B12X4A12X4Unorm4Pack16KHR },
		{ "eG12X4B12X4G12X4R12X4422Unorm4Pack16", vk::Format::eG12X4B12X4G12X4R12X4422Unorm4Pack16 },
		{ "eG12X4B12X4G12X4R12X4422Unorm4Pack16KHR", vk::Format::eG12X4B12X4G12X4R12X4422Unorm4Pack16KHR },
		{ "eB12X4G12X4R12X4G12X4422Unorm4Pack16", vk::Format::eB12X4G12X4R12X4G12X4422Unorm4Pack16 },
		{ "eB12X4G12X4R12X4G12X4422Unorm4Pack16KHR", vk::Format::eB12X4G12X4R12X4G12X4422Unorm4Pack16KHR },
		{ "eG12X4B12X4R12X43Plane420Unorm3Pack16", vk::Format::eG12X4B12X4R12X43Plane420Unorm3Pack16 },
		{ "eG12X4B12X4R12X43Plane420Unorm3Pack16KHR", vk::Format::eG12X4B12X4R12X43Plane420Unorm3Pack16KHR },
		{ "eG12X4B12X4R12X42Plane420Unorm3Pack16", vk::Format::eG12X4B12X4R12X42Plane420Unorm3Pack16 },
		{ "eG12X4B12X4R12X42Plane420Unorm3Pack16KHR", vk::Format::eG12X4B12X4R12X42Plane420Unorm3Pack16KHR },
		{ "eG12X4B12X4R12X43Plane422Unorm3Pack16", vk::Format::eG12X4B12X4R12X43Plane422Unorm3Pack16 },
		{ "eG12X4B12X4R12X43Plane422Unorm3Pack16KHR", vk::Format::eG12X4B12X4R12X43Plane422Unorm3Pack16KHR },
		{ "eG12X4B12X4R12X42Plane422Unorm3Pack16", vk::Format::eG12X4B12X4R12X42Plane422Unorm3Pack16 },
		{ "eG12X4B12X4R12X42Plane422Unorm3Pack16KHR", vk::Format::eG12X4B12X4R12X42Plane422Unorm3Pack16KHR },
		{ "eG12X4B12X4R12X43Plane444Unorm3Pack16", vk::Format::eG12X4B12X4R12X43Plane444Unorm3Pack16 },
		{ "eG12X4B12X4R12X43Plane444Unorm3Pack16KHR", vk::Format::eG12X4B12X4R12X43Plane444Unorm3Pack16KHR },
		{ "eG16B16G16R16422Unorm", vk::Format::eG16B16G16R16422Unorm },
		{ "eG16B16G16R16422UnormKHR", vk::Format::eG16B16G16R16422UnormKHR },
		{ "eB16G16R16G16422Unorm", vk::Format::eB16G16R16G16422Unorm },
		{ "eB16G16R16G16422UnormKHR", vk::Format::eB16G16R16G16422UnormKHR },
		{ "eG16B16R163Plane420Unorm", vk::Format::eG16B16R163Plane420Unorm },
		{ "eG16B16R163Plane420UnormKHR", vk::Format::eG16B16R163Plane420UnormKHR },
		{ "eG16B16R162Plane420Unorm", vk::Format::eG16B16R162Plane420Unorm },
		{ "eG16B16R162Plane420UnormKHR", vk::Format::eG16B16R162Plane420UnormKHR },
		{ "eG16B16R163Plane422Unorm", vk::Format::eG16B16R163Plane422Unorm },
		{ "eG16B16R163Plane422UnormKHR", vk::Format::eG16B16R163Plane422UnormKHR },
		{ "eG16B16R162Plane422Unorm", vk::Format::eG16B16R162Plane422Unorm },
		{ "eG16B16R162Plane422UnormKHR", vk::Format::eG16B16R162Plane422UnormKHR },
		{ "eG16B16R163Plane444Unorm", vk::Format::eG16B16R163Plane444Unorm },
		{ "eG16B16R163Plane444UnormKHR", vk::Format::eG16B16R163Plane444UnormKHR },
		{ "eG8B8R82Plane444Unorm", vk::Format::eG8B8R82Plane444Unorm },
		{ "eG8B8R82Plane444UnormEXT", vk::Format::eG8B8R82Plane444UnormEXT },
		{ "eG10X6B10X6R10X62Plane444Unorm3Pack16", vk::Format::eG10X6B10X6R10X62Plane444Unorm3Pack16 },
		{ "eG10X6B10X6R10X62Plane444Unorm3Pack16EXT", vk::Format::eG10X6B10X6R10X62Plane444Unorm3Pack16EXT },
		{ "eG12X4B12X4R12X42Plane444Unorm3Pack16", vk::Format::eG12X4B12X4R12X42Plane444Unorm3Pack16 },
		{ "eG12X4B12X4R12X42Plane444Unorm3Pack16EXT", vk::Format::eG12X4B12X4R12X42Plane444Unorm3Pack16EXT },
		{ "eG16B16R162Plane444Unorm", vk::Format::eG16B16R162Plane444Unorm },
		{ "eG16B16R162Plane444UnormEXT", vk::Format::eG16B16R162Plane444UnormEXT },
		{ "eA4R4G4B4UnormPack16", vk::Format::eA4R4G4B4UnormPack16 },
		{ "eA4R4G4B4UnormPack16EXT", vk::Format::eA4R4G4B4UnormPack16EXT },
		{ "eA4B4G4R4UnormPack16", vk::Format::eA4B4G4R4UnormPack16 },
		{ "eA4B4G4R4UnormPack16EXT", vk::Format::eA4B4G4R4UnormPack16EXT },
		{ "eAstc4x4SfloatBlock", vk::Format::eAstc4x4SfloatBlock },
		{ "eAstc4x4SfloatBlockEXT", vk::Format::eAstc4x4SfloatBlockEXT },
		{ "eAstc5x4SfloatBlock", vk::Format::eAstc5x4SfloatBlock },
		{ "eAstc5x4SfloatBlockEXT", vk::Format::eAstc5x4SfloatBlockEXT },
		{ "eAstc5x5SfloatBlock", vk::Format::eAstc5x5SfloatBlock },
		{ "eAstc5x5SfloatBlockEXT", vk::Format::eAstc5x5SfloatBlockEXT },
		{ "eAstc6x5SfloatBlock", vk::Format::eAstc6x5SfloatBlock },
		{ "eAstc6x5SfloatBlockEXT", vk::Format::eAstc6x5SfloatBlockEXT },
		{ "eAstc6x6SfloatBlock", vk::Format::eAstc6x6SfloatBlock },
		{ "eAstc6x6SfloatBlockEXT", vk::Format::eAstc6x6SfloatBlockEXT },
		{ "eAstc8x5SfloatBlock", vk::Format::eAstc8x5SfloatBlock },
		{ "eAstc8x5SfloatBlockEXT", vk::Format::eAstc8x5SfloatBlockEXT },
		{ "eAstc8x6SfloatBlock", vk::Format::eAstc8x6SfloatBlock },
		{ "eAstc8x6SfloatBlockEXT", vk::Format::eAstc8x6SfloatBlockEXT },
		{ "eAstc8x8SfloatBlock", vk::Format::eAstc8x8SfloatBlock },
		{ "eAstc8x8SfloatBlockEXT", vk::Format::eAstc8x8SfloatBlockEXT },
		{ "eAstc10x5SfloatBlock", vk::Format::eAstc10x5SfloatBlock },
		{ "eAstc10x5SfloatBlockEXT", vk::Format::eAstc10x5SfloatBlockEXT },
		{ "eAstc10x6SfloatBlock", vk::Format::eAstc10x6SfloatBlock },
		{ "eAstc10x6SfloatBlockEXT", vk::Format::eAstc10x6SfloatBlockEXT },
		{ "eAstc10x8SfloatBlock", vk::Format::eAstc10x8SfloatBlock },
		{ "eAstc10x8SfloatBlockEXT", vk::Format::eAstc10x8SfloatBlockEXT },
		{ "eAstc10x10SfloatBlock", vk::Format::eAstc10x10SfloatBlock },
		{ "eAstc10x10SfloatBlockEXT", vk::Format::eAstc10x10SfloatBlockEXT },
		{ "eAstc12x10SfloatBlock", vk::Format::eAstc12x10SfloatBlock },
		{ "eAstc12x10SfloatBlockEXT", vk::Format::eAstc12x10SfloatBlockEXT },
		{ "eAstc12x12SfloatBlock", vk::Format::eAstc12x12SfloatBlock },
		{ "eAstc12x12SfloatBlockEXT", vk::Format::eAstc12x12SfloatBlockEXT },
		{ "eA1B5G5R5UnormPack16", vk::Format::eA1B5G5R5UnormPack16 },
		{ "eA1B5G5R5UnormPack16KHR", vk::Format::eA1B5G5R5UnormPack16KHR },
		{ "eA8Unorm", vk::Format::eA8Unorm },
		{ "eA8UnormKHR", vk::Format::eA8UnormKHR },
		{ "ePvrtc12BppUnormBlockIMG", vk::Format::ePvrtc12BppUnormBlockIMG },
		{ "ePvrtc14BppUnormBlockIMG", vk::Format::ePvrtc14BppUnormBlockIMG },
		{ "ePvrtc22BppUnormBlockIMG", vk::Format::ePvrtc22BppUnormBlockIMG },
		{ "ePvrtc24BppUnormBlockIMG", vk::Format::ePvrtc24BppUnormBlockIMG },
		{ "ePvrtc12BppSrgbBlockIMG", vk::Format::ePvrtc12BppSrgbBlockIMG },
		{ "ePvrtc14BppSrgbBlockIMG", vk::Format::ePvrtc14BppSrgbBlockIMG },
		{ "ePvrtc22BppSrgbBlockIMG", vk::Format::ePvrtc22BppSrgbBlockIMG },
		{ "ePvrtc24BppSrgbBlockIMG", vk::Format::ePvrtc24BppSrgbBlockIMG },
		{ "eR16G16Sfixed5NV", vk::Format::eR16G16Sfixed5NV },
		{ "eR16G16S105NV", vk::Format::eR16G16S105NV }
	};

	auto it = map.find(format);
	if (it == map.end()) {
		spdlog::warn("Unknown pipeline type: \"{}\".", format);
		return vk::Format::eUndefined;
	}
	return it->second;
}

// TODO: write pipeline state parsing from yaml

#ifdef _WIN32
#include "Windows.h"

int main(int argc, char* argv[]);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
	// Attempt to attach to the parent process console if it exists
	if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
		// No parent console, allocate a new one for this process
		if (!AllocConsole()) {
			return -1;
		}
	}

	FILE* fp;
	freopen_s(&fp, "conin$", "r", stdin);
	freopen_s(&fp, "conout$", "w", stdout);
	freopen_s(&fp, "conout$", "w", stderr);

	std::ios::sync_with_stdio(true);

	return main(__argc, __argv);
}
#endif

auto slang_stage_to_vulkan(SlangStage stage) -> vk::ShaderStageFlagBits {
	switch (stage) {
	case SLANG_STAGE_VERTEX: return vk::ShaderStageFlagBits::eVertex;
	case SLANG_STAGE_FRAGMENT: return vk::ShaderStageFlagBits::eFragment;
	case SLANG_STAGE_GEOMETRY: return vk::ShaderStageFlagBits::eGeometry;
	case SLANG_STAGE_HULL: return vk::ShaderStageFlagBits::eTessellationControl;
	case SLANG_STAGE_DOMAIN: return vk::ShaderStageFlagBits::eTessellationEvaluation;
	case SLANG_STAGE_COMPUTE: return vk::ShaderStageFlagBits::eCompute;
	case SLANG_STAGE_RAY_GENERATION: return vk::ShaderStageFlagBits::eRaygenKHR;
	case SLANG_STAGE_INTERSECTION: return vk::ShaderStageFlagBits::eIntersectionKHR;
	case SLANG_STAGE_ANY_HIT: return vk::ShaderStageFlagBits::eAnyHitKHR;
	case SLANG_STAGE_CLOSEST_HIT: return vk::ShaderStageFlagBits::eClosestHitKHR;
	case SLANG_STAGE_MISS: return vk::ShaderStageFlagBits::eMissKHR;
	case SLANG_STAGE_CALLABLE: return vk::ShaderStageFlagBits::eCallableKHR;
	case SLANG_STAGE_MESH: return vk::ShaderStageFlagBits::eMeshEXT;
	case SLANG_STAGE_AMPLIFICATION: return vk::ShaderStageFlagBits::eTaskEXT;
	default:
		spdlog::warn("Unknown Slang stage: {}", static_cast<int>(stage));
		return vk::ShaderStageFlagBits::eVertex;
	}
}

auto slang_descriptor_type_to_vulkan(slang::BindingType bindingType) -> vk::DescriptorType {
	switch (bindingType) {
	case slang::BindingType::Texture: return vk::DescriptorType::eSampledImage;
	case slang::BindingType::Sampler: return vk::DescriptorType::eSampler;
	case slang::BindingType::CombinedTextureSampler: return vk::DescriptorType::eCombinedImageSampler;
	case slang::BindingType::MutableTexture: return vk::DescriptorType::eStorageImage;
	case slang::BindingType::ConstantBuffer: return vk::DescriptorType::eUniformBuffer;
	case slang::BindingType::TypedBuffer:
	case slang::BindingType::RawBuffer:
	case slang::BindingType::MutableTypedBuffer:
	case slang::BindingType::MutableRawBuffer: return vk::DescriptorType::eStorageBuffer;
	case slang::BindingType::RayTracingAccelerationStructure: return vk::DescriptorType::eAccelerationStructureKHR;
	default:
		spdlog::warn("Unknown Slang binding type: {}", static_cast<int>(bindingType));
		return vk::DescriptorType::eUniformBuffer;
	}
}

auto read_file(const std::string& path) -> std::string {
	std::ifstream technique_file(path, std::ios_base::binary);
	if (!technique_file.is_open()) {
		spdlog::critical("Failed to open input file: {}", path);
		return {};
	}

	technique_file.imbue(std::locale("C"));
	return { std::istreambuf_iterator<char>(technique_file), std::istreambuf_iterator<char>() };
}

class StringBlob : public ISlangBlob {
public:
	StringBlob(std::string&& data)
		: data(std::move(data)) {
	}

	static auto create(const char* string) -> Slang::ComPtr<ISlangBlob> {
		return Slang::ComPtr<ISlangBlob>(new StringBlob(std::string(string)));
	}

	static auto create(std::string&& string) -> Slang::ComPtr<ISlangBlob> {
		return Slang::ComPtr<ISlangBlob>(new StringBlob(std::move(string)));
	}

	// IUnknown methods:
	virtual SlangResult SLANG_MCALL queryInterface(const SlangUUID& guid, void** outObject) override {
		if (!outObject) {
			return SLANG_FAIL;
		}
		*outObject = nullptr;

		if (guid == ISlangBlob::getTypeGuid() || guid == ISlangUnknown::getTypeGuid()) {
			*outObject = static_cast<ISlangBlob*>(this);
			return SLANG_OK;
		}
		return SLANG_E_NO_INTERFACE;
	}
	virtual uint32_t SLANG_MCALL addRef() override {
		return 1;
	}
	virtual uint32_t SLANG_MCALL release() override {
		return 1;
	}

	// ISlangBlob methods:
	virtual void const* SLANG_MCALL getBufferPointer() override {
		return data.data();
	}
	virtual size_t SLANG_MCALL getBufferSize() override {
		return data.size();
	}

private:
	std::string data;
};

class DependencyTrackingFileSystem final : public ISlangFileSystemExt {
public:
	DependencyTrackingFileSystem() : ref_cnt{ 1 } {
	}

	virtual ~DependencyTrackingFileSystem() {
		clearCache();
	}

	virtual SlangResult SLANG_MCALL queryInterface(const SlangUUID& guid, void** outObject) override {
		if (!outObject) {
			return SLANG_FAIL;
		}
		*outObject = nullptr;

		if (guid == ISlangFileSystemExt::getTypeGuid() || guid == ISlangFileSystem::getTypeGuid() ||
			guid == ISlangUnknown::getTypeGuid()) {
			*outObject = static_cast<ISlangFileSystemExt*>(this);
			return SLANG_OK;
		}
		return SLANG_E_NO_INTERFACE;
	}

	virtual uint32_t SLANG_MCALL addRef() override {
		return ++ref_cnt;
	}

	virtual uint32_t SLANG_MCALL release() override {
		uint32_t count = --ref_cnt;
		if (count == 0) {
			delete this;
		}
		return count;
	}

	virtual void* SLANG_MCALL castAs(const SlangUUID& guid) override {
		if (guid == ISlangFileSystemExt::getTypeGuid() || guid == ISlangFileSystem::getTypeGuid() ||
			guid == ISlangUnknown::getTypeGuid()) {
			return static_cast<ISlangFileSystemExt*>(this);
		}
		return nullptr;
	}

	SLANG_NO_THROW SlangResult SLANG_MCALL loadFile(char const* path, ISlangBlob** out_blob) SLANG_OVERRIDE {
		if (!path || !out_blob) {
			return SLANG_E_INVALID_ARG;
		}
		*out_blob = nullptr;

		if (!std::filesystem::exists(path)) {
			return SLANG_E_NOT_FOUND;
		}

		auto abs_path = std::filesystem::absolute(path).lexically_normal().string();
		g_session.dependencies.insert(abs_path);

		auto found_in_cache = file_cache.find(path);
		if (found_in_cache != file_cache.end()) {
			found_in_cache->second->addRef();
			*out_blob = found_in_cache->second;
			return SLANG_OK;
		}

		std::string file_data = read_file(path);
		if (file_data.empty()) {
			return SLANG_E_NOT_FOUND;
		}

		auto new_blob = StringBlob::create(std::move(file_data));
		file_cache[path] = new_blob;

		*out_blob = new_blob;

		return SLANG_OK;
	}

	virtual SlangResult SLANG_MCALL getFileUniqueIdentity(const char* path, ISlangBlob** outUniqueIdentity) override {
		if (!path || !outUniqueIdentity) {
			return SLANG_E_INVALID_ARG;
		}

		auto normalized = std::filesystem::weakly_canonical(path);
		*outUniqueIdentity = StringBlob::create(std::move(normalized.string()));
		return SLANG_OK;
	}

	virtual SlangResult SLANG_MCALL calcCombinedPath(SlangPathType fromPathType, const char* from_path, const char* path, ISlangBlob** outCombinedPath) override {
		if (!path || !outCombinedPath) {
			return SLANG_E_INVALID_ARG;
		}
		*outCombinedPath = nullptr;

		auto current_path = std::filesystem::path(from_path).parent_path();
		auto normalized_path = std::filesystem::weakly_canonical(current_path / path);
		*outCombinedPath = StringBlob::create(std::move(normalized_path.string()));

		return SLANG_OK;
	}

	virtual SlangResult SLANG_MCALL getPathType(const char* path, SlangPathType* outPathType) override {
		if (!path || !outPathType) {
			return SLANG_E_INVALID_ARG;
		}

		if (!std::filesystem::exists(path)) {
			return SLANG_E_NOT_FOUND;
		}

		*outPathType = std::filesystem::is_directory(path) ? SLANG_PATH_TYPE_DIRECTORY : SLANG_PATH_TYPE_FILE;
		return SLANG_OK;
	}

	virtual SlangResult SLANG_MCALL getPath(PathKind /*pathKind*/, const char* path, ISlangBlob** outPath) override {
		if (!path || !outPath) {
			return SLANG_E_INVALID_ARG;
		}

		*outPath = StringBlob::create(std::string(path, path + strlen(path)));
		return SLANG_OK;
	}

	virtual void SLANG_MCALL clearCache() override {
		file_cache.clear();
	}

	virtual SlangResult SLANG_MCALL enumeratePathContents(const char* path, FileSystemContentsCallBack callback, void* userData) override {
		if (!path || !callback) {
			return SLANG_E_INVALID_ARG;
		}

		if (!std::filesystem::is_directory(path)) {
			return SLANG_E_NOT_FOUND;
		}

		for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
			SlangPathType type = entry.is_directory() ? SLANG_PATH_TYPE_DIRECTORY : SLANG_PATH_TYPE_FILE;
			auto entry_path = entry.path();
			auto entry_path_string = entry_path.string();
			callback(type, entry_path_string.c_str(), userData);
		}

		return SLANG_OK;
	}

	virtual OSPathKind SLANG_MCALL getOSPathKind() override {
		return OSPathKind::Direct;
	}
private:
	std::atomic<uint32_t> ref_cnt;

	std::vector<std::string> search_paths{};
	std::unordered_map<std::string, Slang::ComPtr<ISlangBlob>> file_cache{};
};

int main(int argc, char* argv[]) {
	// Initialize argument parser
	argparse::ArgumentParser argument_parser(APPLICATION_NAME, "1.0", argparse::default_arguments::help);
	argument_parser.add_description("Compile Slang shaders to SPIR-V with Vulkan reflection data");
	argument_parser.add_epilog("Example: shader_compiler -i shader.slang -o compiled/ --optimization speed");

	argument_parser
		.add_argument("-D", "--define")
		.help("Preprocessor definitions (e.g., -D DEBUG=1)")
		.append()
		.store_into(g_session.definitions);

	argument_parser
		.add_argument("-I", "--include")
		.help("Additional include directories")
		.append()
		.store_into(g_session.include_directories);

	argument_parser
		.add_argument("-i", "--input")
		.help("Input Slang shader file")
		.required()
		.store_into(g_session.input);

	argument_parser
		.add_argument("-o", "--output")
		.help("Output file path or directory")
		.default_value(std::string("./shader_output.bin"))
		.store_into(g_session.output);

	argument_parser
		.add_argument("--optimization")
		.help("Optimization level")
		.default_value(std::string("none"))
		.choices("none", "default", "high", "maximal")
		.store_into(g_session.optimization_level);

	argument_parser
		.add_argument("--debug")
		.help("Debug information level")
		.default_value(std::string("maximal"))
		.choices("none", "minimal", "standard", "maximal")
		.store_into(g_session.debug_level);

	argument_parser
		.add_argument("-v", "--verbose")
		.help("Enable verbose output")
		.default_value(false)
		.implicit_value(true)
		.store_into(g_session.verbose);

	argument_parser
		.add_argument("--depfile")
		.help("Generate dependency file for build systems (Makefile format)")
		.store_into(g_session.depfile_path);

	try {
#ifdef NDEBUG
		argument_parser.parse_args(argc, argv);
#else
		if (argc == 1) {
			// Debug arguments for testing
			std::vector<const char*> args{
				argv[0],
				"-i", "D:\\GitHub\\edge\\assets\\shaders\\imgui.technique.yaml",
				"-v"
			};
			argument_parser.parse_args(args.size(), args.data());
		}
		else {
			argument_parser.parse_args(argc, argv);
		}
#endif
	}
	catch (const std::exception& err) {
		std::cerr << err.what() << std::endl;
		std::cerr << argument_parser;
		return 1;
	}

	g_session.dependencies.clear();

	std::filesystem::path abs_input = std::filesystem::absolute(g_session.input);
	g_session.dependencies.insert(abs_input.string());

	// Init logger
	auto log_level = g_session.verbose ? spdlog::level::debug : spdlog::level::info;

	spdlog::sinks_init_list sink_list{
		std::make_shared<spdlog::sinks::basic_file_sink_mt>(LOG_FILE_NAME, true),
		std::make_shared<spdlog::sinks::stdout_color_sink_mt>()
	};

	auto logger = std::make_shared<spdlog::logger>(APPLICATION_NAME, sink_list.begin(), sink_list.end());
	logger->set_level(log_level);
	logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
	spdlog::set_default_logger(logger);

	// Flush logger every 3 seconds or on critical errors
	spdlog::flush_every(std::chrono::seconds(3));
	spdlog::flush_on(spdlog::level::critical);

	spdlog::info("Starting {} v1.0", APPLICATION_NAME);
	spdlog::debug("Input file: {}", g_session.input);
	spdlog::debug("Output: {}", g_session.output);
	spdlog::debug("Optimization: {}, Debug: {}", g_session.optimization_level, g_session.debug_level);

	if (!std::filesystem::exists(g_session.input)) {
		spdlog::critical("Input file does not exist: {}", g_session.input);
		return 1;
	}

	if (!g_session.slang_session) {
		Slang::ComPtr<slang::IGlobalSession> new_session;
		SlangGlobalSessionDesc session_desc = {};
		if (SLANG_SUCCEEDED(slang::createGlobalSession(&session_desc, new_session.writeRef()))) {
			g_session.slang_session = new_session.detach();
			spdlog::info("Slang compiler session created successfully");
		}
		else {
			spdlog::critical("Failed to create Slang compiler session");
			return 2;
		}
	}

	// Prepare preprocessor macros
	std::vector<slang::PreprocessorMacroDesc> preprocessor_input{};
	for (const auto& definition : g_session.definitions) {
		auto pos = definition.find('=');
		if (pos != std::string::npos) {
			preprocessor_input.emplace_back(definition.substr(0, pos).c_str(),
				definition.substr(pos + 1).c_str());
		}
		else {
			preprocessor_input.emplace_back(definition.c_str(), "1");
		}
	}

	// Prepare search paths
	std::vector<const char*> search_paths{};
	for (const auto& path : g_session.include_directories) {
		search_paths.push_back(path.c_str());
	}

	// Set optimization level
	SlangOptimizationLevel opt_level = SLANG_OPTIMIZATION_LEVEL_NONE;
	if (g_session.optimization_level == "default") opt_level = SLANG_OPTIMIZATION_LEVEL_DEFAULT;
	else if (g_session.optimization_level == "high") opt_level = SLANG_OPTIMIZATION_LEVEL_HIGH;
	else if (g_session.optimization_level == "maximal") opt_level = SLANG_OPTIMIZATION_LEVEL_MAXIMAL;

	// Set debug level
	SlangDebugInfoLevel debug_level = SLANG_DEBUG_INFO_LEVEL_MAXIMAL;
	if (g_session.debug_level == "none") debug_level = SLANG_DEBUG_INFO_LEVEL_NONE;
	else if (g_session.debug_level == "minimal") debug_level = SLANG_DEBUG_INFO_LEVEL_MINIMAL;
	else if (g_session.debug_level == "standard") debug_level = SLANG_DEBUG_INFO_LEVEL_STANDARD;

	// Configure compiler options
	std::array<slang::CompilerOptionEntry, 3ull> compiler_options = {
		slang::CompilerOptionEntry{ slang::CompilerOptionName::Optimization, {.intValue0 = (int32_t)opt_level } },
		slang::CompilerOptionEntry{ slang::CompilerOptionName::DebugInformation, {.intValue0 = (int32_t)debug_level } },
		slang::CompilerOptionEntry{ slang::CompilerOptionName::EmitSpirvMethod, {.intValue0 = SLANG_EMIT_SPIRV_DIRECTLY } }
	};

	gfx::ShaderEffect shader_effect{};
	init_pipeline_state_header(shader_effect.pipeline_state);

	auto technique_content = read_file(g_session.input);
	if (technique_content.empty()) {
		spdlog::critical("Failed to read file: {}", g_session.input);
		return 1;
	}

	// Parse technique params
	auto techniaue_tree = ryml::parse_in_arena(ryml::to_csubstr(technique_content));
	auto root = techniaue_tree.rootref();

	if (root.has_child("name")) {
		std::string name_str;
		root["name"] >> name_str;
		shader_effect.name = mi::String{ name_str.begin(), name_str.end() };
	}

	if (root.has_child("type")) {
		std::string pipeline_type_str{ "unknown" };
		root["type"] >> pipeline_type_str;
		shader_effect.bind_point = parse_pipeline_bind_point(pipeline_type_str);
	}
	else {
		spdlog::critical("Required parameter \"type\" is not set in \"{}\" technique description.", shader_effect.name);
		return 3;
	}

	std::string source_file_name_str{};
	if (root.has_child("source")) {
		root["source"] >> source_file_name_str;
	}
	else {
		spdlog::critical("Required parameter \"source\" is not set in \"{}\" technique description.", shader_effect.name);
		return 3;
	}

	std::string compiler_profile{ "spirv_1_4" };
	if (root.has_child("profile")) {
		root["profile"] >> compiler_profile;
	}

	if (root.has_child("tessellation")) {
		auto tess = root["tessellation"];
		if (tess.has_child("control_points")) {
			uint32_t tessellation_controll_points{ 0u };
			tess["control_points"] >> tessellation_controll_points;

			shader_effect.pipeline_state.tessellation_state_control_points = static_cast<uint8_t>(tessellation_controll_points);
		}
	}

	if (root.has_child("rasterization")) {
		auto rast = root["rasterization"];
		
		if (rast.has_child("clamp_enable")) {
			bool clamp_enable = false;
			rast["clamp_enable"] >> clamp_enable;
			shader_effect.pipeline_state.rasterization_state_depth_clamp_enable = static_cast<uint8_t>(clamp_enable);
		}

		if (rast.has_child("discard_enable")) {
			bool discard_enable = false;
			rast["discard_enable"] >> discard_enable;
			shader_effect.pipeline_state.rasterization_state_discard_enable = static_cast<uint8_t>(discard_enable);
		}

		if (rast.has_child("polygon_mode")) {
			std::string polygon_mode_str;
			rast["polygon_mode"] >> polygon_mode_str;
			shader_effect.pipeline_state.rasterization_state_polygon_mode = static_cast<uint8_t>(parse_fill_mode(polygon_mode_str));
		}

		if (rast.has_child("cull_mode")) {
			std::string cull_mode_str;
			rast["cull_mode"] >> cull_mode_str;
			shader_effect.pipeline_state.rasterization_state_cull_mode = static_cast<uint32_t>(parse_cull_mode(cull_mode_str));
		}

		if (rast.has_child("front_face")) {
			std::string front_face_str;
			rast["front_face"] >> front_face_str;
			shader_effect.pipeline_state.rasterization_state_front_face = static_cast<uint8_t>(parse_front_face(front_face_str));
		}
		if (rast.has_child("depth_bias_enable")) {
			bool depth_bias_enable = false;
			rast["depth_bias_enable"] >> depth_bias_enable;
			shader_effect.pipeline_state.rasterization_state_depth_bias_enable = static_cast<uint8_t>(depth_bias_enable);

			if (depth_bias_enable) {
				if (rast.has_child("depth_bias_constant_factor")) {
					rast["depth_bias_constant_factor"] >> shader_effect.pipeline_state.rasterization_state_depth_bias_constant_factor;
				}

				if (rast.has_child("depth_bias_clamp")) {
					rast["depth_bias_clamp"] >> shader_effect.pipeline_state.rasterization_state_depth_bias_clamp;
				}

				if (rast.has_child("depth_bias_slope_factor")) {
					rast["depth_bias_slope_factor"] >> shader_effect.pipeline_state.rasterization_state_depth_bias_slope_factor;
				}
			}
		}
		
		if (rast.has_child("line_width")) {
			rast["line_width"] >> shader_effect.pipeline_state.rasterization_state_line_width;
		}
	}

	if (root.has_child("multisample")) {
		auto ms = root["multisample"];

		if (ms.has_child("sample_count")) {
			uint32_t sample_count = 1;
			ms["sample_count"] >> sample_count;
			shader_effect.pipeline_state.multisample_state_sample_count = static_cast<uint8_t>(sample_count);
		}

		if (ms.has_child("sample_shading_enable")) {
			bool sample_shading_enable = false;
			ms["sample_shading_enable"] >> sample_shading_enable;

			if (sample_shading_enable) {
				if (ms.has_child("min_sample_shading")) {
					ms["min_sample_shading"] >> shader_effect.pipeline_state.multisample_state_min_sample_shading;
				}
			}
		}

		if (shader_effect.pipeline_state.multisample_state_sample_count > 1) {
			if (ms.has_child("sample_mask")) {
				auto masks = ms["sample_mask"];
				for (const auto& mask : masks) {
					uint32_t sample_mask;
					mask >> sample_mask;
					shader_effect.multisample_sample_masks.push_back(sample_mask);
				}
			}
		}

		if (shader_effect.multisample_sample_masks.size() != shader_effect.pipeline_state.multisample_state_sample_count) {
			spdlog::warn("Number of samples and number of sample masks should be equal!");
		}

		if (ms.has_child("alpha_to_coverage_enable")) {
			bool alpha_to_coverage = false;
			ms["alpha_to_coverage_enable"] >> alpha_to_coverage;
			shader_effect.pipeline_state.multisample_state_alpha_to_coverage_enable = static_cast<uint8_t>(alpha_to_coverage);
		}

		if (ms.has_child("alpha_to_one_enable")) {
			bool alpha_to_one = false;
			ms["alpha_to_one_enable"] >> alpha_to_one;
			shader_effect.pipeline_state.multisample_state_alpha_to_one_enable = static_cast<uint8_t>(alpha_to_one);
		}
	}

	if (root.has_child("depth_stencil")) {
		auto ds = root["depth_stencil"];

		if (ds.has_child("depth_test_enable")) {
			bool depth_test = false;
			ds["depth_test_enable"] >> depth_test;
			shader_effect.pipeline_state.depth_state_depth_test_enable = static_cast<uint8_t>(depth_test);
		}
		
		if (ds.has_child("depth_write_enable")) {
			bool depth_write = false;
			ds["depth_write_enable"] >> depth_write;
			shader_effect.pipeline_state.depth_state_depth_write_enable = static_cast<uint8_t>(depth_write);
		}

		if (ds.has_child("compare_op")) {
			std::string compare_op_str;
			ds["compare_op"] >> compare_op_str;
			shader_effect.pipeline_state.depth_state_depth_compare_op = static_cast<uint8_t>(parse_compare_op(compare_op_str));
		}

		if (ds.has_child("bounds_test_enable")) { 
			bool bounds_test = false;
			ds["bounds_test_enable"] >> bounds_test; 
			shader_effect.pipeline_state.depth_state_depth_bounds_test_enable = static_cast<uint8_t>(bounds_test);

			if (bounds_test) {
				if (ds.has_child("min_depth_bounds")) {
					ds["min_depth_bounds"] >> shader_effect.pipeline_state.depth_state_min_depth_bounds;
				}

				if (ds.has_child("max_depth_bounds")) {
					ds["max_depth_bounds"] >> shader_effect.pipeline_state.depth_state_max_depth_bounds;
				}
			}
		}

		if (ds.has_child("stencil_test_enable")) {
			bool stencil_test = false;
			ds["stencil_test_enable"] >> stencil_test;
			shader_effect.pipeline_state.stencil_state_stencil_test_enable = static_cast<uint8_t>(stencil_test);

			if (stencil_test) {
				if (ds.has_child("front_fail_op")) {
					std::string front_fail_str;
					ds["front_fail_op"] >> front_fail_str;
					shader_effect.pipeline_state.stencil_state_front_fail_op = static_cast<uint8_t>(parse_stencil_op(front_fail_str));
				}

				if (ds.has_child("front_pass_op")) {
					std::string front_pass_str;
					ds["front_pass_op"] >> front_pass_str;
					shader_effect.pipeline_state.stencil_state_front_pass_op = static_cast<uint8_t>(parse_stencil_op(front_pass_str));
				}

				if (ds.has_child("front_depth_fail_op")) {
					std::string front_depth_fail_str;
					ds["front_depth_fail_op"] >> front_depth_fail_str;
					shader_effect.pipeline_state.stencil_state_front_depth_fail_op = static_cast<uint8_t>(parse_stencil_op(front_depth_fail_str));
				}

				if (ds.has_child("front_compare_op")) {
					std::string front_compare_str;
					ds["front_compare_op"] >> front_compare_str;
					shader_effect.pipeline_state.stencil_state_front_compare_op = static_cast<uint8_t>(parse_compare_op(front_compare_str));
				}

				if (ds.has_child("front_compare_mask")) {
					ds["front_compare_mask"] >> shader_effect.pipeline_state.stencil_state_front_compare_mask;
				}

				if (ds.has_child("front_write_mask")) {
					uint32_t front_write_mask = 0;
					ds["front_write_mask"] >> shader_effect.pipeline_state.stencil_state_front_write_mask;
				}

				if (ds.has_child("front_reference")) {
					ds["front_reference"] >> shader_effect.pipeline_state.stencil_state_front_reference;
				}

				if (ds.has_child("back_fail_op")) {
					std::string back_fail_str;
					ds["back_fail_op"] >> back_fail_str;
					shader_effect.pipeline_state.stencil_state_back_fail_op = static_cast<uint8_t>(parse_stencil_op(back_fail_str));
				}

				if (ds.has_child("back_pass_op")) {
					std::string back_pass_str;
					ds["back_pass_op"] >> back_pass_str;
					shader_effect.pipeline_state.stencil_state_back_pass_op = static_cast<uint8_t>(parse_stencil_op(back_pass_str));
				}

				if (ds.has_child("back_depth_fail_op")) {
					std::string back_depth_fail_str;
					ds["back_depth_fail_op"] >> back_depth_fail_str;
					shader_effect.pipeline_state.stencil_state_back_depth_fail_op = static_cast<uint8_t>(parse_stencil_op(back_depth_fail_str));
				}

				if (ds.has_child("back_compare_op")) {
					std::string back_compare_str;
					ds["back_compare_op"] >> back_compare_str;
					shader_effect.pipeline_state.stencil_state_back_compare_op = static_cast<uint8_t>(parse_compare_op(back_compare_str));
				}

				if (ds.has_child("back_compare_mask")) {
					ds["back_compare_mask"] >> shader_effect.pipeline_state.stencil_state_back_compare_mask;
				}

				if (ds.has_child("back_write_mask")) {
					ds["back_write_mask"] >> shader_effect.pipeline_state.stencil_state_back_write_mask;
				}

				if (ds.has_child("back_reference")) {
					ds["back_reference"] >> shader_effect.pipeline_state.stencil_state_back_reference;
				}
			}
		}
	}

	if (root.has_child("color_blending")) {
		auto cb = root["color_blending"];

		if (cb.has_child("logic_op_enable")) {
			bool logic_op_enable = false;
			cb["logic_op_enable"] >> logic_op_enable;
			shader_effect.pipeline_state.color_blending_state_logic_op_enable = static_cast<uint8_t>(logic_op_enable);
		}

		if (cb.has_child("logic_op")) {
			std::string logic_op_str;
			cb["logic_op"] >> logic_op_str;
			shader_effect.pipeline_state.color_blending_state_logic_op = static_cast<uint8_t>(parse_logic_op(logic_op_str));
		}

		if (cb.has_child("attachments")) {
			auto attachments = cb["attachments"];
			for (const auto& attachment : attachments) {
				gfx::ColorAttachment gfx_attachment{};
				init_color_attachment(gfx_attachment);

				if (attachment.has_child("blending")) {
					bool blending = false;
					attachment["blending"] >> blending;
					gfx_attachment.blend_enable = static_cast<uint8_t>(blending);

					if (attachment.has_child("format")) {
						std::string format_str;
						attachment["format"] >> format_str;
						shader_effect.attachment_formats.push_back(parse_format(format_str));
					}
					else {
						shader_effect.attachment_formats.push_back(vk::Format::eUndefined);
					}

					if (attachment.has_child("src_color")) {
						std::string src_color_str;
						attachment["src_color"] >> src_color_str;
						gfx_attachment.src_color_blend_factor = static_cast<uint8_t>(parse_blend_factor(src_color_str));
					}

					if (attachment.has_child("dst_color")) {
						std::string dst_color_str;
						attachment["dst_color"] >> dst_color_str;
						gfx_attachment.dst_color_blend_factor = static_cast<uint8_t>(parse_blend_factor(dst_color_str));
					}

					if (attachment.has_child("color_blend_op")) {
						std::string color_blend_op_str;
						attachment["color_blend_op"] >> color_blend_op_str;
						gfx_attachment.color_blend_op = static_cast<uint8_t>(parse_blend_op(color_blend_op_str));
					}

					if (attachment.has_child("src_alpha")) {
						std::string src_alpha_str;
						attachment["src_alpha"] >> src_alpha_str;
						gfx_attachment.src_alpha_blend_factor = static_cast<uint8_t>(parse_blend_factor(src_alpha_str));
					}

					if (attachment.has_child("dst_alpha")) {
						std::string dst_alpha_str;
						attachment["dst_alpha"] >> dst_alpha_str;
						gfx_attachment.dst_alpha_blend_factor = static_cast<uint8_t>(parse_blend_factor(dst_alpha_str));
					}

					if (attachment.has_child("alpha_blend_op")) {
						std::string alpha_blend_op_str;
						attachment["alpha_blend_op"] >> alpha_blend_op_str;
						gfx_attachment.alpha_blend_op = static_cast<uint8_t>(parse_blend_op(alpha_blend_op_str));
					}

					if (attachment.has_child("color_write_mask")) {
						uint32_t color_write_mask = 0u;
						attachment["color_write_mask"] >> color_write_mask;
						gfx_attachment.color_write_mask = color_write_mask;
					}
				}

				shader_effect.color_attachments.push_back(gfx_attachment);
			}
		}

		shader_effect.pipeline_state.color_blending_state_has_attachments = !shader_effect.color_attachments.empty();
	}

	if (root.has_child("input_assembly")) {
		auto ia = root["input_assembly"];

		if (ia.has_child("primitive_topology")) {
			std::string topology_str;
			ia["primitive_topology"] >> topology_str;
			shader_effect.pipeline_state.input_assembly_state_primitive_topology = static_cast<uint8_t>(parse_primitive_topology(topology_str));
		}

		if (ia.has_child("primitive_restart")) {
			bool primitive_restart = false;
			ia["primitive_restart"] >> primitive_restart;
			shader_effect.pipeline_state.input_assembly_state_primitive_restart_enable = static_cast<uint8_t>(primitive_restart);
		}
	}

	if (root.has_child("depth_format")) {
		std::string format_str;
		root["depth_format"] >> format_str;
		shader_effect.depth_format = parse_format(format_str);
	}

	if (root.has_child("stencil_format")) {
		std::string format_str;
		root["stencil_format"] >> format_str;
		shader_effect.stencil_format = parse_format(format_str);
	}

	auto source_module_path = std::filesystem::path(g_session.input).parent_path() / source_file_name_str;
	auto source_module_path_string = source_module_path.string();

	auto filesystem = Slang::ComPtr<ISlangFileSystemExt>(new DependencyTrackingFileSystem);

	// Configure target
	slang::TargetDesc target_desc{};
	target_desc.format = SLANG_SPIRV;
	target_desc.profile = g_session.slang_session->findProfile(compiler_profile.c_str());
	target_desc.floatingPointMode = SLANG_FLOATING_POINT_MODE_PRECISE;
	target_desc.compilerOptionEntries = compiler_options.data();
	target_desc.compilerOptionEntryCount = static_cast<SlangInt>(compiler_options.size());

	// Create session
    slang::SessionDesc session_desc{};
    session_desc.targets = &target_desc;
    session_desc.targetCount = 1;
    session_desc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
    session_desc.searchPaths = search_paths.data();
    session_desc.searchPathCount = static_cast<SlangInt>(search_paths.size());
    session_desc.preprocessorMacros = preprocessor_input.data();
    session_desc.preprocessorMacroCount = static_cast<SlangInt>(preprocessor_input.size());
    session_desc.compilerOptionEntries = compiler_options.data();
    session_desc.compilerOptionEntryCount = static_cast<SlangInt>(compiler_options.size());
	session_desc.fileSystem = filesystem;

	Slang::ComPtr<slang::ISession> session{};
	if (SLANG_FAILED(g_session.slang_session->createSession(session_desc, session.writeRef()))) {
		spdlog::critical("Failed to create compilation session");
		return 2;
	}

	// Read shader file
	auto source_content = read_file(source_module_path_string);

	Slang::ComPtr<slang::IBlob> diagnostics; 
	slang::IModule* slang_module = session->loadModuleFromSourceString(shader_effect.name.c_str(), source_module_path_string.c_str(), source_content.data(), diagnostics.writeRef());
	if (!slang_module) {
		spdlog::critical("Failed to load Slang module \"{}\".", shader_effect.name);
		if (diagnostics) {
			spdlog::error("Compilation diagnostics:\n{}", static_cast<const char*>(diagnostics->getBufferPointer()));
		}
		return 3;
	}

	spdlog::info("Successfully loaded Slang module: {}", shader_effect.name);
	
	spdlog::info("Found {} entry points", slang_module->getDefinedEntryPointCount());
	for (SlangInt32 entry_point_index = 0; entry_point_index < slang_module->getDefinedEntryPointCount(); ++entry_point_index) {
		Slang::ComPtr<slang::IEntryPoint> entry_point;
		if (SLANG_FAILED(slang_module->getDefinedEntryPoint(entry_point_index, entry_point.writeRef()))) {
			spdlog::error("Failed to get entry point at index {}", entry_point_index);
			continue;
		}

		Slang::ComPtr<slang::IComponentType> linked_program;
		if (SLANG_FAILED(entry_point->link(linked_program.writeRef(), diagnostics.writeRef()))) {
			spdlog::error("Failed to link entry point at index {}", entry_point_index);
			if (diagnostics) {
				spdlog::error("Link diagnostics:\n{}",
					static_cast<const char*>(diagnostics->getBufferPointer()));
			}
			continue;
		}

		auto program_layout = linked_program->getLayout();

		// Get entry point info
		auto entry_layout = program_layout->getEntryPointByIndex(0);
		auto stage = entry_layout->getStage();
		auto entry_name = entry_layout->getName();

		// Fill shader stage data
		gfx::TechniqueStage technique_stage{};
		technique_stage.stage = slang_stage_to_vulkan(stage);
		technique_stage.entry_point_name = entry_name ? entry_name : "main";

		// Compile to SPIR-V
		Slang::ComPtr<slang::IBlob> spirv_blob;
		if (SLANG_FAILED(linked_program->getEntryPointCode(0, 0, spirv_blob.writeRef(), diagnostics.writeRef()))) {
			spdlog::error("Failed to compile entry point '{}' to SPIR-V", technique_stage.entry_point_name);
			if (diagnostics) {
				spdlog::error("Compilation diagnostics:\n{}",
					static_cast<const char*>(diagnostics->getBufferPointer()));
			}
			continue;
		}

		// Store compiled code
		technique_stage.code.resize(spirv_blob->getBufferSize());
		std::memcpy(technique_stage.code.data(), spirv_blob->getBufferPointer(), spirv_blob->getBufferSize());

		spdlog::info("Successfully compiled entry point '{}' (stage: {}, size: {} bytes)",
			technique_stage.entry_point_name,
			static_cast<int>(technique_stage.stage),
			technique_stage.code.size());

		shader_effect.stages.push_back(std::move(technique_stage));
	}

	if (shader_effect.stages.empty()) {
		spdlog::critical("No entry points were successfully compiled");
		return 4;
	}

	std::ofstream compillation_result_output(g_session.output, std::ios_base::binary);
	if (!compillation_result_output.is_open()) {
		spdlog::critical("Failed to save compillation result.");
		return 1;
	}

	BinaryWriter writer{ compillation_result_output };
	writer.write(shader_effect);

	spdlog::info("Shader compilation completed successfully!");
	spdlog::info("Compiled {} stages.", shader_effect.stages.size());

	if (!g_session.depfile_path.empty()) {
		std::ofstream depfile(g_session.depfile_path);
		if (!depfile) {
			spdlog::warn("Could not write depfile : {}", g_session.depfile_path);
			return 1;
		}

		auto escape_path = [](const std::string& path) -> std::string {
			std::string escaped = path;
			size_t pos = 0;
			while ((pos = escaped.find(' ', pos)) != std::string::npos) {
				escaped.insert(pos, "\\");
				pos += 2;
			}
			// Also escape other problematic characters for Make
			pos = 0;
			while ((pos = escaped.find('$', pos)) != std::string::npos) {
				escaped.insert(pos, "$");
				pos += 2;
			}
			return escaped;
			};

		depfile << escape_path(g_session.output) << ":";

		for (const auto& dep : g_session.dependencies) {
			depfile << " \\\n  " << escape_path(dep);
		}
		depfile << "\n";

		if (g_session.verbose) {
			spdlog::debug("Wrote {} dependencies to {}", g_session.dependencies.size(), g_session.depfile_path);
		}
	}

	return 0;
}