#include "platform.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

extern int edge_main(edge_platform_layout_t* platform_layout);

struct edge_platform_layout {
	HINSTANCE hinst;
	HINSTANCE prev_hinst;
	PSTR cmd_line;
	INT cmd_show;
};

int APIENTRY WinMain(HINSTANCE hinst, HINSTANCE prev_hinst, PSTR cmd_line, INT cmd_show) {
	edge_platform_layout_t platform_layout = { 0 };
	platform_layout.hinst = hinst;
	platform_layout.prev_hinst = prev_hinst;
	platform_layout.cmd_line = cmd_line;
	platform_layout.cmd_show = cmd_show;

	return edge_main(&platform_layout);
}