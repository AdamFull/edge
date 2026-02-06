#include "input_system.h"
#include "runtime.h"

#include <logger.hpp>

#if defined(EDGE_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(EDGE_PLATFORM_LINUX)
#endif

#include <stdio.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <vulkan/vulkan.h>

extern i32 edge_main(edge::RuntimeLayout *runtime_layout);

namespace edge {
struct RuntimeLayout {
#if defined(EDGE_PLATFORM_WINDOWS)
  HINSTANCE hinst;
  HINSTANCE prev_hinst;
  PSTR cmd_line;
  INT cmd_show;
#endif
};

namespace {
Key glfw_key_to_engine_key(const i32 glfw_key) {
  switch (glfw_key) {
  case GLFW_KEY_SPACE:
    return Key::Space;
  case GLFW_KEY_APOSTROPHE:
    return Key::Apostrophe;
  case GLFW_KEY_COMMA:
    return Key::Comma;
  case GLFW_KEY_MINUS:
    return Key::Minus;
  case GLFW_KEY_PERIOD:
    return Key::Period;
  case GLFW_KEY_SLASH:
    return Key::Slash;
  case GLFW_KEY_0:
    return Key::_0;
  case GLFW_KEY_1:
    return Key::_1;
  case GLFW_KEY_2:
    return Key::_2;
  case GLFW_KEY_3:
    return Key::_3;
  case GLFW_KEY_4:
    return Key::_4;
  case GLFW_KEY_5:
    return Key::_5;
  case GLFW_KEY_6:
    return Key::_6;
  case GLFW_KEY_7:
    return Key::_7;
  case GLFW_KEY_8:
    return Key::_8;
  case GLFW_KEY_9:
    return Key::_9;
  case GLFW_KEY_SEMICOLON:
    return Key::Semicolon;
  case GLFW_KEY_EQUAL:
    return Key::Eq;
  case GLFW_KEY_A:
    return Key::A;
  case GLFW_KEY_B:
    return Key::B;
  case GLFW_KEY_C:
    return Key::C;
  case GLFW_KEY_D:
    return Key::D;
  case GLFW_KEY_E:
    return Key::E;
  case GLFW_KEY_F:
    return Key::F;
  case GLFW_KEY_G:
    return Key::G;
  case GLFW_KEY_H:
    return Key::H;
  case GLFW_KEY_I:
    return Key::I;
  case GLFW_KEY_J:
    return Key::J;
  case GLFW_KEY_K:
    return Key::K;
  case GLFW_KEY_L:
    return Key::L;
  case GLFW_KEY_M:
    return Key::M;
  case GLFW_KEY_N:
    return Key::N;
  case GLFW_KEY_O:
    return Key::O;
  case GLFW_KEY_P:
    return Key::P;
  case GLFW_KEY_Q:
    return Key::Q;
  case GLFW_KEY_R:
    return Key::R;
  case GLFW_KEY_S:
    return Key::S;
  case GLFW_KEY_T:
    return Key::T;
  case GLFW_KEY_U:
    return Key::U;
  case GLFW_KEY_V:
    return Key::V;
  case GLFW_KEY_W:
    return Key::W;
  case GLFW_KEY_X:
    return Key::X;
  case GLFW_KEY_Y:
    return Key::Y;
  case GLFW_KEY_Z:
    return Key::Z;
  case GLFW_KEY_LEFT_BRACKET:
    return Key::LeftBracket;
  case GLFW_KEY_BACKSLASH:
    return Key::Backslash;
  case GLFW_KEY_RIGHT_BRACKET:
    return Key::RightBracket;
  case GLFW_KEY_GRAVE_ACCENT:
    return Key::GraveAccent;
  case GLFW_KEY_ESCAPE:
    return Key::Esc;
  case GLFW_KEY_ENTER:
    return Key::Enter;
  case GLFW_KEY_TAB:
    return Key::Tab;
  case GLFW_KEY_BACKSPACE:
    return Key::Backspace;
  case GLFW_KEY_INSERT:
    return Key::Insert;
  case GLFW_KEY_DELETE:
    return Key::Del;
  case GLFW_KEY_RIGHT:
    return Key::Right;
  case GLFW_KEY_LEFT:
    return Key::Left;
  case GLFW_KEY_DOWN:
    return Key::Down;
  case GLFW_KEY_UP:
    return Key::Up;
  case GLFW_KEY_PAGE_UP:
    return Key::PageUp;
  case GLFW_KEY_PAGE_DOWN:
    return Key::PageDown;
  case GLFW_KEY_HOME:
    return Key::Home;
  case GLFW_KEY_END:
    return Key::End;
  case GLFW_KEY_CAPS_LOCK:
    return Key::CapsLock;
  case GLFW_KEY_SCROLL_LOCK:
    return Key::ScrollLock;
  case GLFW_KEY_NUM_LOCK:
    return Key::NumLock;
  case GLFW_KEY_PRINT_SCREEN:
    return Key::PrintScreen;
  case GLFW_KEY_PAUSE:
    return Key::Pause;
  case GLFW_KEY_F1:
    return Key::F1;
  case GLFW_KEY_F2:
    return Key::F2;
  case GLFW_KEY_F3:
    return Key::F3;
  case GLFW_KEY_F4:
    return Key::F4;
  case GLFW_KEY_F5:
    return Key::F5;
  case GLFW_KEY_F6:
    return Key::F6;
  case GLFW_KEY_F7:
    return Key::F7;
  case GLFW_KEY_F8:
    return Key::F8;
  case GLFW_KEY_F9:
    return Key::F9;
  case GLFW_KEY_F10:
    return Key::F10;
  case GLFW_KEY_F11:
    return Key::F11;
  case GLFW_KEY_F12:
    return Key::F12;
  case GLFW_KEY_KP_0:
    return Key::Kp0;
  case GLFW_KEY_KP_1:
    return Key::Kp1;
  case GLFW_KEY_KP_2:
    return Key::Kp2;
  case GLFW_KEY_KP_3:
    return Key::Kp3;
  case GLFW_KEY_KP_4:
    return Key::Kp4;
  case GLFW_KEY_KP_5:
    return Key::Kp5;
  case GLFW_KEY_KP_6:
    return Key::Kp6;
  case GLFW_KEY_KP_7:
    return Key::Kp7;
  case GLFW_KEY_KP_8:
    return Key::Kp8;
  case GLFW_KEY_KP_9:
    return Key::Kp9;
  case GLFW_KEY_KP_DECIMAL:
    return Key::KpDec;
  case GLFW_KEY_KP_DIVIDE:
    return Key::KpDiv;
  case GLFW_KEY_KP_MULTIPLY:
    return Key::KpMul;
  case GLFW_KEY_KP_SUBTRACT:
    return Key::KpSub;
  case GLFW_KEY_KP_ADD:
    return Key::KpAdd;
  case GLFW_KEY_KP_ENTER:
    return Key::KpEnter;
  case GLFW_KEY_KP_EQUAL:
    return Key::KpEq;
  case GLFW_KEY_LEFT_SHIFT:
    return Key::LeftShift;
  case GLFW_KEY_LEFT_CONTROL:
    return Key::LeftControl;
  case GLFW_KEY_LEFT_ALT:
    return Key::LeftAlt;
  case GLFW_KEY_LEFT_SUPER:
    return Key::LeftSuper;
  case GLFW_KEY_RIGHT_SHIFT:
    return Key::RightShift;
  case GLFW_KEY_RIGHT_CONTROL:
    return Key::RightControl;
  case GLFW_KEY_RIGHT_ALT:
    return Key::RightAlt;
  case GLFW_KEY_RIGHT_SUPER:
    return Key::RightSuper;
  case GLFW_KEY_MENU:
    return Key::Menu;
  default:
    return Key::Unknown;
  }
}

MouseBtn glfw_mouse_btn_to_engine_btn(const i32 glfw_btn) {
  switch (glfw_btn) {
  case GLFW_MOUSE_BUTTON_LEFT:
    return MouseBtn::Left;
  case GLFW_MOUSE_BUTTON_RIGHT:
    return MouseBtn::Right;
  case GLFW_MOUSE_BUTTON_MIDDLE:
    return MouseBtn::Middle;
  case GLFW_MOUSE_BUTTON_4:
    return MouseBtn::_4;
  case GLFW_MOUSE_BUTTON_5:
    return MouseBtn::_5;
  case GLFW_MOUSE_BUTTON_6:
    return MouseBtn::_6;
  case GLFW_MOUSE_BUTTON_7:
    return MouseBtn::_7;
  case GLFW_MOUSE_BUTTON_8:
    return MouseBtn::_8;
  default: {
    // TODO: ERROR
    return MouseBtn::Left;
  }
  }
}

PadBtn glfw_gamepad_btn_to_engine_btn(const i32 glfw_btn) {
  switch (glfw_btn) {
  case GLFW_GAMEPAD_BUTTON_A:
    return PadBtn::A;
  case GLFW_GAMEPAD_BUTTON_B:
    return PadBtn::B;
  case GLFW_GAMEPAD_BUTTON_X:
    return PadBtn::X;
  case GLFW_GAMEPAD_BUTTON_Y:
    return PadBtn::Y;
  case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER:
    return PadBtn::BumperLeft;
  case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER:
    return PadBtn::BumperRight;
  case GLFW_GAMEPAD_BUTTON_BACK:
    return PadBtn::Back;
  case GLFW_GAMEPAD_BUTTON_START:
    return PadBtn::Start;
  case GLFW_GAMEPAD_BUTTON_GUIDE:
    return PadBtn::Guide;
  case GLFW_GAMEPAD_BUTTON_LEFT_THUMB:
    return PadBtn::ThumbLeft;
  case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB:
    return PadBtn::ThumbRight;
  case GLFW_GAMEPAD_BUTTON_DPAD_UP:
    return PadBtn::DpadUp;
  case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT:
    return PadBtn::DpadRight;
  case GLFW_GAMEPAD_BUTTON_DPAD_DOWN:
    return PadBtn::DpadDown;
  case GLFW_GAMEPAD_BUTTON_DPAD_LEFT:
    return PadBtn::DpadLeft;
  default: {
    // TODO: ERROR
    return PadBtn::A;
  }
  }
}

f32 apply_deadzone(const f32 value, const f32 deadzone) {
  if (abs(value) < deadzone) {
    return 0.0f;
  }

  // Rescale to 0-1 range after deadzone
  const f32 sign = value < 0.0f ? -1.0f : 1.0f;
  const f32 abs_val = abs(value);
  return sign * ((abs_val - deadzone) / (1.0f - deadzone));
}
} // namespace

struct DesktopRuntime final : IRuntime {
  bool init(const RuntimeInitInfo &init_info) override {
    layout = init_info.layout;
    input_system = init_info.input_system;

#if defined(EDGE_PLATFORM_WINDOWS)
#if EDGE_DEBUG
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
      if (!AllocConsole()) {
        EDGE_LOG_DEBUG("Failed to allocate console.");
      }
    }

    const HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;

    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);

    FILE *fp = nullptr;
    freopen_s(&fp, "conin$", "r", stdin);
    freopen_s(&fp, "conout$", "w", stdout);
    freopen_s(&fp, "conout$", "w", stderr);
#endif // EDGE_DEBUG

    Logger *logger = logger_get_global();
    ILoggerOutput *debug_output = logger_create_debug_console_output(
        init_info.alloc, LogFormat::LogFormat_Default);
    logger->add_output(init_info.alloc, debug_output);

#elif defined(EDGE_PLATFORM_LINUX)
#endif

    const GLFWallocator glfw_allocator = {
        .allocate = [](const size_t size, void *user) -> void * {
          return static_cast<Allocator *>(user)->malloc(size);
        },
        .reallocate = [](void *block, const size_t size, void *user) -> void * {
          return static_cast<Allocator *>(user)->realloc(block, size);
        },
        .deallocate = [](void *block, void *user) -> void {
          static_cast<Allocator *>(user)->free(block);
        },
        .user = (void *)init_info.alloc};

    glfwInitAllocator(&glfw_allocator);

    if (!glfwInit()) {
      EDGE_LOG_ERROR("Failed to init glfw context.");
      return false;
    }

    glfwSetErrorCallback([](const i32 error, const char *description) -> void {
      EDGE_LOG_ERROR("GLFW error: %d. %s.", error, description);
    });
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    switch (init_info.mode) {
    case WindowMode::Fullscreen: {
      GLFWmonitor *monitor = glfwGetPrimaryMonitor();
      const GLFWvidmode *mode = glfwGetVideoMode(monitor);
      wnd_handle = glfwCreateWindow(mode->width, mode->height, init_info.title,
                                    monitor, nullptr);
      break;
    }
    case WindowMode::FullscreenBorderless: {
      GLFWmonitor *monitor = glfwGetPrimaryMonitor();
      const GLFWvidmode *mode = glfwGetVideoMode(monitor);

      glfwWindowHint(GLFW_RED_BITS, mode->redBits);
      glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
      glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
      glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

      wnd_handle = glfwCreateWindow(mode->width, mode->height, init_info.title,
                                    monitor, nullptr);
      break;
    }
    default: {
      wnd_handle = glfwCreateWindow(init_info.width, init_info.height,
                                    init_info.title, nullptr, nullptr);
      break;
    }
    }

    if (!wnd_handle) {
      return false;
    }

    glfwSetWindowFocusCallback(
        wnd_handle, [](GLFWwindow *window, const i32 focused) -> void {
          auto *rt = static_cast<DesktopRuntime *>(glfwGetWindowUserPointer(window));
          rt->focused = focused == GLFW_TRUE;
        });

    // For input
    glfwSetKeyCallback(
        wnd_handle,
        [](GLFWwindow *window, const i32 key, const i32 scancode, const i32 action,
           i32 mods) -> void {
          const auto *rt = static_cast<DesktopRuntime *>(glfwGetWindowUserPointer(window));
          KeyboardDevice *keyboard = rt->input_system->get_keyboard();

          if (const Key engine_key = glfw_key_to_engine_key(key);
              engine_key != Key::Unknown && action != GLFW_REPEAT) {
            keyboard->set_key(engine_key, action == GLFW_PRESS);
          }
        });

    glfwSetCursorPosCallback(
        wnd_handle, [](GLFWwindow *window, const double xpos, const double ypos) -> void {
          const auto *rt = static_cast<DesktopRuntime *>(glfwGetWindowUserPointer(window));
          MouseDevice *mouse = rt->input_system->get_mouse();
          mouse->set_axis(MouseAxis::Pos, {static_cast<f32>(xpos),
                                           static_cast<f32>(ypos), 0.0f});
        });

    glfwSetMouseButtonCallback(
        wnd_handle,
        [](GLFWwindow *window, const i32 button, const i32 action, const i32 mods) -> void {
          const auto *rt = static_cast<DesktopRuntime *>(glfwGetWindowUserPointer(window));
          MouseDevice *mouse = rt->input_system->get_mouse();

          const MouseBtn engine_btn = glfw_mouse_btn_to_engine_btn(button);
          if (action != GLFW_REPEAT) {
            mouse->set_btn(engine_btn, action == GLFW_PRESS);
          }
        });

    glfwSetScrollCallback(
        wnd_handle,
        [](GLFWwindow *window, const double xoffset, const double yoffset) -> void {
          const auto *rt = static_cast<DesktopRuntime *>(glfwGetWindowUserPointer(window));
          MouseDevice *mouse = rt->input_system->get_mouse();
          mouse->set_axis(MouseAxis::Scroll, {static_cast<f32>(xoffset),
                                              static_cast<f32>(yoffset), 0.0f});
        });

    glfwSetCharCallback(
        wnd_handle, [](GLFWwindow *window, const u32 codepoint) -> void {
          const auto *rt = static_cast<DesktopRuntime *>(glfwGetWindowUserPointer(window));
          rt->input_system->input_text.add_character(codepoint);
        });

    glfwSetJoystickCallback([](const i32 jid, const i32 event) -> void {
      const char *guid = glfwGetJoystickGUID(jid);

      i32 vendor_id = 0, product_id = 0;
      sscanf(guid, "%04x%04x", &vendor_id, &product_id);
    });

    glfwSetInputMode(wnd_handle, GLFW_STICKY_KEYS, 1);
    glfwSetInputMode(wnd_handle, GLFW_STICKY_MOUSE_BUTTONS, 1);

    glfwSetWindowUserPointer(wnd_handle, this);

    return true;
  }

  void deinit(NotNull<const Allocator *> alloc) override {
    glfwSetWindowShouldClose(wnd_handle, GLFW_TRUE);
    glfwDestroyWindow(wnd_handle);

    glfwTerminate();
    glfwSetErrorCallback(nullptr);
  }

  bool requested_close() const override {
    return glfwWindowShouldClose(wnd_handle) == GLFW_TRUE;
  }

  void process_events() override {
    glfwPollEvents();

    for (i32 jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
      const usize pad_idx = jid - GLFW_JOYSTICK_1;
      if (pad_idx >= MAX_GAMEPADS) {
        break;
      }

      PadDevice *pad = input_system->get_gamepad(pad_idx);

      if (glfwJoystickPresent(jid) && glfwJoystickIsGamepad(jid)) {
        if (!pad->connected) {
          pad->connected = true;

          if (const char *name = glfwGetGamepadName(jid)) {
            strncpy(pad->name, name, sizeof(pad->name) - 1);
          }

          if (const char *guid = glfwGetJoystickGUID(jid)) {
            sscanf(guid, "%4x%4x", &pad->vendor_id, &pad->product_id);
          }
        }

        GLFWgamepadstate state;
        if (glfwGetGamepadState(jid, &state)) {
          for (i32 btn = 0; btn < GLFW_GAMEPAD_BUTTON_LAST + 1; ++btn) {
            const PadBtn engine_btn = glfw_gamepad_btn_to_engine_btn(btn);
            pad->set_btn(engine_btn, state.buttons[btn] == GLFW_PRESS);
          }

          f32 left_x = apply_deadzone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X],
                                      pad->stick_deadzone);
          f32 left_y = apply_deadzone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y],
                                      pad->stick_deadzone);
          f32 right_x = apply_deadzone(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X],
                                       pad->stick_deadzone);
          f32 right_y = apply_deadzone(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y],
                                       pad->stick_deadzone);

          f32 trigger_left =
              apply_deadzone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER],
                             pad->trigger_deadzone);
          f32 trigger_right =
              apply_deadzone(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER],
                             pad->trigger_deadzone);

          // Map to 0-1 range for triggers (GLFW returns -1 to 1)
          trigger_left = (trigger_left + 1.0f) * 0.5f;
          trigger_right = (trigger_right + 1.0f) * 0.5f;

          pad->set_axis(PadAxis::StickLeft, {left_x, left_y, 0.0f});
          pad->set_axis(PadAxis::StickRight, {right_x, right_y, 0.0f});
          pad->set_axis(PadAxis::TriggerLeft, {trigger_left, 0.0f, 0.0f});
          pad->set_axis(PadAxis::TriggerRight, {trigger_right, 0.0f, 0.0f});

          pad->set_btn(PadBtn::TriggerLeft, trigger_left > 0.5f);
          pad->set_btn(PadBtn::TriggerRight, trigger_right > 0.5f);
        }
      } else {
        if (pad->connected) {
          pad->clear();
        }
      }
    }
  }

  void get_surface(void *surface_info) const override {
    if (!surface_info) {
      return;
    }

#if defined(EDGE_PLATFORM_WINDOWS)
    *static_cast<VkWin32SurfaceCreateInfoKHR *>(surface_info) = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = layout->hinst,
        .hwnd = glfwGetWin32Window(wnd_handle)};
#else
#error "Not implamented"
#endif
  }

  void get_surface_extent(i32 &width, i32 &height) const override {
    glfwGetWindowSize(wnd_handle, &width, &height);
  }

  [[nodiscard]] f32 get_surface_scale_factor() const override {
    f32 xscale, yscale;
    glfwGetWindowContentScale(wnd_handle, &xscale, &yscale);
    return xscale;
  }

  [[nodiscard]] bool is_focused() const override { return focused; }

  void set_title(const char *title) override {
    glfwSetWindowTitle(wnd_handle, title);
  }

private:
  RuntimeLayout *layout = nullptr;
  GLFWwindow *wnd_handle = nullptr;
  bool focused = true;
  InputSystem *input_system = nullptr;
};

IRuntime *create_runtime(const NotNull<const Allocator *> alloc) {
  return alloc->allocate<DesktopRuntime>();
}
} // namespace edge

#if defined(EDGE_PLATFORM_WINDOWS)
i32 APIENTRY WinMain(HINSTANCE hinst, HINSTANCE prev_hinst, PSTR cmd_line,
                     INT cmd_show) {
  edge::RuntimeLayout platform_layout = {.hinst = hinst,
                                         .prev_hinst = prev_hinst,
                                         .cmd_line = cmd_line,
                                         .cmd_show = cmd_show};

  return edge_main(&platform_layout);
}
#endif