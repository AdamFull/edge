#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#define APPLICATION_NAME "file_server"
#define LOG_FILE_NAME "file_server.log"

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

struct Session {
	std::string share_path{};
	uint16_t port;
};

static Session g_session;

int main(int argc, char* argv[]) {
	argparse::ArgumentParser argument_parser(APPLICATION_NAME, "1.0", argparse::default_arguments::help);
	argument_parser.add_description("File server for streaming resources to engine.");
	argument_parser.add_epilog("Example: file_server --share directory_to_share_path --port 25565");

	argument_parser
		.add_argument("-s", "--share")
		.help("Directory to share")
		.required()
		.store_into(g_session.share_path);

	argument_parser
		.add_argument("-p", "--port")
		.help("Server port")
		.default_value(25565)
		.store_into(g_session.port);

	spdlog::sinks_init_list sink_list{
		std::make_shared<spdlog::sinks::basic_file_sink_mt>(LOG_FILE_NAME, true),
		std::make_shared<spdlog::sinks::stdout_color_sink_mt>()
	};

	auto logger = std::make_shared<spdlog::logger>(APPLICATION_NAME, sink_list.begin(), sink_list.end());
#ifdef _DEBUG
	logger->set_level(spdlog::level::debug);
#else
	logger->set_level(spdlog::level::info);
#endif
	logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
	spdlog::set_default_logger(logger);

	// Flush logger every 3 seconds or on critical errors
	spdlog::flush_every(std::chrono::seconds(3));
	spdlog::flush_on(spdlog::level::critical);

	try {
#ifdef NDEBUG
		argument_parser.parse_args(argc, argv);
#else
		if (argc == 1) {
			// Debug arguments for debugging
			std::vector<const char*> args{
				argv[0],
				"-s", "D:\\GitHub\\edge\\out\\build\\x64-Debug\\assets",
				"-p", "20205"
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

	return 0;
}