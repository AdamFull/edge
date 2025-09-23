#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <slang.h>
#include <slang-com-ptr.h>

#include <fstream>
#include <filesystem>

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
	slang::IGlobalSession* slang_session{ nullptr };
};

static Session g_session;

int main(int argc, char* argv[]) {
	FILE* fp;
	freopen_s(&fp, "conin$", "r", stdin);
	freopen_s(&fp, "conout$", "w", stdout);
	freopen_s(&fp, "conout$", "w", stderr);

	std::ios::sync_with_stdio(true);

	// Initialize logger
	spdlog::sinks_init_list sink_list{
		std::make_shared<spdlog::sinks::basic_file_sink_mt>(LOG_FILE_NAME, true),
		std::make_shared<spdlog::sinks::stdout_color_sink_mt>()
	};

	auto logger = std::make_shared<spdlog::logger>(APPLICATION_NAME, sink_list.begin(), sink_list.end());
	logger->set_level(spdlog::level::trace);
	logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");
	spdlog::set_default_logger(logger);

	// Initialize argument parser
	argparse::ArgumentParser argument_parser(APPLICATION_NAME);

	argument_parser
		.add_argument("-D", "--definitions")
		.help("Additional preprocessor args.")
		.append()
		.store_into(g_session.definitions);

	argument_parser
		.add_argument("-I", "--include_dir")
		.help("Additional include dirs.")
		.append()
		.store_into(g_session.include_directories);

	argument_parser
		.add_argument("-i", "--output")
		.help("Slang shader input.")
		.required()
		.store_into(g_session.input);

	argument_parser
		.add_argument("-o", "--output")
		.help("Output file/dir path.")
		.default_value(std::string("./"))
		.store_into(g_session.output);

	try {
#ifdef NDEBUG
		argument_parser.parse_args(argc, argv);
#else
		std::vector<const char*> args{ "shader_compiler.exe", "-i", "D:\\GitHub\\edge\\tools\\shader_compiler\\test_shader.slang" };
		argument_parser.parse_args(args.size(), args.data());
#endif
	}
	catch (const std::exception& err) {
		spdlog::error(err.what());
		spdlog::info(argument_parser.print_help());
		return 1;
	}

	if (!g_session.slang_session) {
		// Create a global Slang session (load Slang core module). 
		// Using default options: enable HLSL/Slang by default.
		Slang::ComPtr<slang::IGlobalSession> new_session;
		SlangGlobalSessionDesc session_desc = {};
		if (SLANG_SUCCEEDED(slang::createGlobalSession(&session_desc, new_session.writeRef()))) {
			g_session.slang_session = new_session.detach();  // keep it alive for reuse
			spdlog::info("New compiller session started.");
		}
		else {
			g_session.slang_session = nullptr;
			spdlog::critical("Failed to create compiler session.");
		}
	}

	std::vector<slang::PreprocessorMacroDesc> preprocessor_input{};
	for (auto const& definition : g_session.definitions) {
		preprocessor_input.emplace_back(definition.c_str(), "");
	}

	std::vector<const char*> search_paths(g_session.include_directories.size());
	for (auto const& path : g_session.include_directories) {
		search_paths.push_back(path.c_str());
	}

	// TODO: set optimizations
	slang::CompilerOptionEntry compiler_options[] = {
			{ slang::CompilerOptionName::Optimization, {.intValue0 = SLANG_OPTIMIZATION_LEVEL_NONE } },
			{ slang::CompilerOptionName::DebugInformation, {.intValue0 = SLANG_DEBUG_INFO_LEVEL_MAXIMAL } },
			{ slang::CompilerOptionName::EmitSpirvMethod, {.intValue0 = SLANG_EMIT_SPIRV_DIRECTLY } }
	};

	slang::TargetDesc target_desc{};
	target_desc.format = SLANG_SPIRV;
	target_desc.profile = g_session.slang_session->findProfile("spirv_1_5");
	target_desc.floatingPointMode = SLANG_FLOATING_POINT_MODE_PRECISE;
	//target_desc.forceGLSLScalarBufferLayout = true;
	target_desc.compilerOptionEntries = compiler_options;
	target_desc.compilerOptionEntryCount = static_cast<SlangInt>(std::size(compiler_options));

	slang::SessionDesc session_desc{};
	session_desc.targets = &target_desc;
	session_desc.targetCount = 1;
	session_desc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
	session_desc.searchPaths = search_paths.data();
	session_desc.searchPathCount = static_cast<SlangInt>(search_paths.size());
	session_desc.preprocessorMacros = preprocessor_input.data();
	session_desc.preprocessorMacroCount = static_cast<SlangInt>(preprocessor_input.size());
	//session_desc.fileSystem = filesystem;
	session_desc.compilerOptionEntries = compiler_options;
	session_desc.compilerOptionEntryCount = static_cast<SlangInt>(std::size(compiler_options));

	Slang::ComPtr<slang::ISession> session{};
	if (SLANG_FAILED(g_session.slang_session->createSession(session_desc, session.writeRef()))) {
		spdlog::critical("[Failed to create compile session.");
		return -2;
	}

	auto input_file_path = std::filesystem::path(g_session.input);
	auto module_name = input_file_path.stem().string();

	std::ifstream file_source(input_file_path, std::ios_base::binary);
	file_source.imbue(std::locale("C"));
	std::string source_code{ std::istreambuf_iterator<char>(file_source), std::istreambuf_iterator<char>() };

	Slang::ComPtr<slang::IBlob> diagnostics;
	slang::IModule* slang_module = session->loadModuleFromSourceString(module_name.c_str(),
		g_session.input.c_str(), source_code.data(), diagnostics.writeRef());

	if (!slang_module) {
		spdlog::critical("Failed to open slang module.");
		if (diagnostics) {
			spdlog::info("Diagnostics:\n{}", (const char*)diagnostics->getBufferPointer());
		}
		return -2;
	}

	for (SlangInt32 entry_point_index = 0; entry_point_index < slang_module->getDefinedEntryPointCount(); ++entry_point_index) {
		Slang::ComPtr<slang::IEntryPoint> entry_point;
		if (SLANG_FAILED(slang_module->getDefinedEntryPoint(entry_point_index, entry_point.writeRef()))) {
			spdlog::critical("Failed to get shader entry point.");
			return -2;
		}

		Slang::ComPtr<slang::IComponentType> linked_program;
		if (SLANG_FAILED(entry_point->link(linked_program.writeRef(), diagnostics.writeRef()))) {
			spdlog::critical("Failed to link slang module.");
			if (diagnostics) {
				spdlog::info("Diagnostics:\n{}", (const char*)diagnostics->getBufferPointer());
			}
			return -2;
		}
	}

	return 0;
}