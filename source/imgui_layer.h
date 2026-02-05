#ifndef IMGUI_LAYER_H
#define IMGUI_LAYER_H

#include <stddef.hpp>

namespace edge {
struct Allocator;
struct EventDispatcher;
struct IRuntime;
struct InputSystem;

struct ImGuiLayerInitInfo {
  IRuntime *runtime = nullptr;
  InputSystem *input_system = nullptr;
};

struct ImGuiLayer {
  IRuntime *runtime = nullptr;
  InputSystem *input_system = nullptr;

  u64 input_listener_id = 0;

  bool create(NotNull<const Allocator *> alloc, ImGuiLayerInitInfo init_info);
  void destroy(NotNull<const Allocator *> alloc);

  void on_frame_begin(f32 dt);
  void on_frame_end();
};

ImGuiLayer *imgui_layer_create(ImGuiLayerInitInfo init_info);
void imgui_layer_destroy(ImGuiLayer *layer);
void imgui_layer_update(NotNull<ImGuiLayer *> layer, f32 delta_time);
} // namespace edge

#endif