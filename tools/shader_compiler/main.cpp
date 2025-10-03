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

#include "../../edge/core/gfx/gfx_shader_effect.h"

using namespace edge;

struct TechniqueStage {
	vk::ShaderStageFlagBits stage;
	std::string entry_point_name;
	std::vector<uint8_t> code;

	void serialize(BinaryWriter& writer) const {
		writer.write(static_cast<uint32_t>(stage));
		writer.write_string(entry_point_name);

		const auto max_compressed_size = ZSTD_compressBound(code.size());
		std::vector<uint8_t> compressed_code(max_compressed_size, 0);

		const auto compressed_size = ZSTD_compress(compressed_code.data(), max_compressed_size, code.data(), code.size(), 15);
		compressed_code.resize(compressed_size);
		
		writer.write_vector(compressed_code);
	}
};

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
	pipeline_state.vertex_input_state_binding_count = 0;
	pipeline_state.vertex_input_state_attribute_count = 0;

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
	pipeline_state.multisample_state_sample_mask = 0;
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
	pipeline_state.color_blending_state_attachment_count = 0;
	memset(pipeline_state.color_blending_state_blend_constants, 0, sizeof(float) * 4ull);
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
	slang::IGlobalSession* slang_session{ nullptr };
};

static Session g_session;

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

	gfx::PipelineStateHeader pipeline_state{};
	init_pipeline_state_header(pipeline_state);

	std::vector<gfx::ColorAttachment> color_attachments;
	std::vector<gfx::VertexInputAttribute> vertex_input_attributes;
	std::vector<gfx::VertexInputBinding> vertex_input_bindings;

	std::vector<TechniqueStage> shader_stages;
	std::string technique_name{ "unknown" };
	vk::PipelineBindPoint pipeline_bind_point;

	auto technique_content = read_file(g_session.input);
	if (technique_content.empty()) {
		spdlog::critical("Failed to read file: {}", g_session.input);
		return 1;
	}

	// Parse technique params
	auto techniaue_tree = ryml::parse_in_arena(ryml::to_csubstr(technique_content));
	auto root = techniaue_tree.rootref();

	if (root.has_child("name")) {
		ryml::csubstr name_value = root["name"].val();
		technique_name = std::string(name_value.data(), name_value.size());
	}

	std::string pipeline_type_str{ "unknown" };
	if (!root.has_child("type")) {
		spdlog::critical("Required parameter \"type\" is not set in \"{}\" technique description.", technique_name);
		return 3;
	}

	ryml::csubstr pipeline_type_value = root["type"].val();
	pipeline_type_str = std::string(pipeline_type_value.data(), pipeline_type_value.size());

	// Parse pipeline type
	if (pipeline_type_str.compare("graphics") == 0) {
		pipeline_bind_point = vk::PipelineBindPoint::eGraphics;
	}
	else if (pipeline_type_str.compare("compute") == 0) {
		pipeline_bind_point = vk::PipelineBindPoint::eCompute;
	}
	else if (pipeline_type_str.compare("ray_tracing") == 0) {
		pipeline_bind_point = vk::PipelineBindPoint::eRayTracingKHR;
	}
	else {
		spdlog::critical("Unknown type \"{}\" is set \"{}\" in technique description.", pipeline_type_str, technique_name);
	}

	if (!root.has_child("source")) {
		spdlog::critical("Required parameter \"source\" is not set in \"{}\" technique description.", technique_name);
		return 3;
	}

	std::string compiler_profile{ "spirv_1_4" };
	if (root.has_child("profile")) {
		ryml::csubstr value = root["profile"].val();
		compiler_profile = std::string(value.data(), value.size());
	}

	ryml::csubstr source_value = root["source"].val();
	auto source_file_name_str = std::string(source_value.data(), source_value.size());
	auto source_module_path = std::filesystem::path(g_session.input).parent_path() / source_file_name_str;
	auto source_module_path_string = source_module_path.string();

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

	Slang::ComPtr<slang::ISession> session{};
	if (SLANG_FAILED(g_session.slang_session->createSession(session_desc, session.writeRef()))) {
		spdlog::critical("Failed to create compilation session");
		return 2;
	}

	// Read shader file
	auto source_content = read_file(source_module_path_string);

	Slang::ComPtr<slang::IBlob> diagnostics; 
	slang::IModule* slang_module = session->loadModuleFromSourceString(technique_name.c_str(), source_module_path_string.c_str(), source_content.data(), diagnostics.writeRef());
	if (!slang_module) {
		spdlog::critical("Failed to load Slang module \"{}\".", technique_name);
		if (diagnostics) {
			spdlog::error("Compilation diagnostics:\n{}", static_cast<const char*>(diagnostics->getBufferPointer()));
		}
		return 3;
	}

	spdlog::info("Successfully loaded Slang module: {}", technique_name);
	
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
		TechniqueStage technique_stage{};
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

		shader_stages.push_back(std::move(technique_stage));
	}

	if (shader_stages.empty()) {
		spdlog::critical("No entry points were successfully compiled");
		return 4;
	}

	// Post process some structures
	pipeline_state.color_blending_state_attachment_count = static_cast<uint8_t>(color_attachments.size());
	pipeline_state.vertex_input_state_attribute_count = static_cast<uint8_t>(vertex_input_attributes.size());
	pipeline_state.vertex_input_state_binding_count = static_cast<uint8_t>(vertex_input_bindings.size());

	std::ofstream compillation_result_output(g_session.output, std::ios_base::binary);
	if (!compillation_result_output.is_open()) {
		spdlog::critical("Failed to save compillation result.");
		return 1;
	}

	BinaryWriter writer{ compillation_result_output };

	gfx::ShaderEffectHeader header{};
	writer.write(header);

	// Write technique data
	writer.write_string(technique_name);
	writer.write(static_cast<uint32_t>(pipeline_bind_point));

	// Serialize shader stages
	writer.write_vector(shader_stages);

	// Write pipeline state header
	writer.write(pipeline_state);
	if (!color_attachments.empty()) {
		writer.write_vector(color_attachments);
	}
	if (!vertex_input_attributes.empty()) {
		writer.write_vector(vertex_input_attributes);
	}
	if (!vertex_input_bindings.empty()) {
		writer.write_vector(vertex_input_bindings);
	}

	// TODO: serialize bindings

	spdlog::info("Shader compilation completed successfully!");
	spdlog::info("Compiled {} stages.", shader_stages.size());

	return 0;
}