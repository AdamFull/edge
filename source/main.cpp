#include "main.h"

#include <allocator.hpp>
#include <logger.hpp>
#include <scheduler.hpp>

#include <assert.h>

#include <mimalloc-stats.h>
#include <mimalloc.h>

#include <math.hpp>

#include <imgui.h>

static edge::Allocator allocator = {};

static edge::Logger logger = {};
static edge::Scheduler *sched = nullptr;

namespace edge {
bool FrameTimeController::create() {
#if EDGE_PLATFORM_WINDOWS
  waitable_timer = CreateWaitableTimer(nullptr, FALSE, nullptr);
  if (waitable_timer) {
    return true;
  }
#endif
  return false;
}

void FrameTimeController::destroy() const {
#if EDGE_PLATFORM_WINDOWS
  CloseHandle(waitable_timer);
#endif
}

void FrameTimeController::set_limit(const f64 target_frame_rate) {
  target_frame_time =
      std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(
          std::chrono::duration<f64>(1.0 / target_frame_rate));
  last_frame_time = std::chrono::high_resolution_clock::now();
}

void FrameTimeController::accurate_sleep(f64 seconds) {
  // Adaptive algorithm (same across all platforms)
  while (seconds - welford_estimate > 1e-7) {
    const f64 to_wait = seconds - welford_estimate;

    auto start = std::chrono::high_resolution_clock::now();

#if EDGE_PLATFORM_ANDROID
    // Android nanosleep implementation
    // Use nanosleep for better precision on mobile devices
    struct timespec req, rem;
    req.tv_sec = static_cast<time_t>(to_wait);
    req.tv_nsec = static_cast<long>((to_wait - req.tv_sec) * 1e9);

    // Handle potential interruptions
    while (nanosleep(&req, &rem) == -1) {
      req = rem;
    }
#elif EDGE_PLATFORM_WINDOWS
    LARGE_INTEGER due;
    due.QuadPart = -static_cast<i64>(to_wait * 1e7); // Convert to 100ns units
    SetWaitableTimerEx(waitable_timer, &due, 0, nullptr, nullptr, nullptr, 0);
    WaitForSingleObject(waitable_timer, INFINITE);
#else
    // TODO: Linux
    std::this_thread::sleep_for(std::chrono::duration<f64>(to_wait));
#endif
    auto end = std::chrono::high_resolution_clock::now();

    const f64 observed = std::chrono::duration<f64>(end - start).count();
    seconds -= observed;

    // Update statistics using Welford's online algorithm
    ++welford_count;
    const f64 error = observed - to_wait;
    const f64 delta = error - welford_mean;
    welford_mean += delta / welford_count;
    welford_m2 += delta * (error - welford_mean);
    const f64 stddev =
        welford_count > 1 ? sqrt(welford_m2 / (welford_count - 1)) : 0.0;
    welford_estimate = welford_mean + stddev;
  }

  // Spin lock for remaining time (cross-platform)
  const auto start = std::chrono::high_resolution_clock::now();
  const auto spin_duration = std::chrono::duration<f64>(seconds);
  while (std::chrono::high_resolution_clock::now() - start < spin_duration) {
    // Tight spin loop for maximum precision
  }
}

bool EngineContext::create(const NotNull<const Allocator *> alloc,
                           const NotNull<RuntimeLayout *> runtime_layout) {
  if (!event_dispatcher.create(alloc)) {
    EDGE_LOG_FATAL("Failed to initialize EventDispatcher.");
    return false;
  }
  EDGE_LOG_INFO("EventDispatcher initialized.");

  if (!input_system.create(&allocator)) {
    EDGE_LOG_FATAL("Failed to initialize InputSystem.");
    return false;
  }
  EDGE_LOG_INFO("InputSystem initialized.");

  const RuntimeInitInfo runtime_info = {.alloc = &allocator,
                                  .layout = runtime_layout.m_ptr,
                                  .input_system = &input_system,

                                  .title = "Vulkan",
                                  .width = 1920,
                                  .height = 1080};

  runtime = create_runtime(&allocator);
  if (!runtime || !runtime->init(runtime_info)) {
    EDGE_LOG_FATAL("Failed to initialize engine runtime.");
    return false;
  }
  EDGE_LOG_INFO("Engine runtime initialized.");

  const gfx::ContextCreateInfo gfx_cteate_info = {.alloc = &allocator,
                                                  .runtime = runtime};

  if (!gfx::context_init(&gfx_cteate_info)) {
    EDGE_LOG_FATAL("Failed to initialize graphics context.");
    return false;
  }
  EDGE_LOG_INFO("Graphics initialized.");

  constexpr gfx::QueueRequest direct_queue_request = {
      .required_caps = gfx::QUEUE_CAPS_GRAPHICS | gfx::QUEUE_CAPS_COMPUTE |
                       gfx::QUEUE_CAPS_TRANSFER | gfx::QUEUE_CAPS_PRESENT,
      .preferred_caps = gfx::QUEUE_CAPS_NONE,
      .strategy = gfx::QUEUE_SELECTION_STRATEGY_PREFER_DEDICATED,
      .prefer_separate_family = false};

  if (!main_queue.request(direct_queue_request)) {
    EDGE_LOG_FATAL("Failed to find direct queue.");
    return false;
  }
  EDGE_LOG_INFO("Direct queue found.");

  // NOTE: Optional, not required
  constexpr gfx::QueueRequest copy_queue_request = {
      .required_caps = gfx::QUEUE_CAPS_TRANSFER,
      .strategy = gfx::QUEUE_SELECTION_STRATEGY_PREFER_DEDICATED,
      .prefer_separate_family = false};
  if (copy_queue.request(copy_queue_request)) {
    EDGE_LOG_INFO("Copy queue found.");
  }

  const gfx::RendererCreateInfo renderer_create_info = {.main_queue =
                                                            main_queue};

  if (!renderer.create(&allocator, renderer_create_info)) {
    EDGE_LOG_FATAL("Failed to initialize main renderer context.");
    return false;
  }

  const gfx::UploaderCreateInfo uploader_create_info = {
      .sched = sched, .queue = copy_queue ? copy_queue : main_queue};

  if (!uploader.create(&allocator, uploader_create_info)) {
    EDGE_LOG_FATAL("Failed to initialize uploader context.");
    return false;
  }

  const ImGuiLayerInitInfo imgui_init_info = {.runtime = runtime,
                                              .input_system = &input_system};

  // TODO: This should be optional in future
  if (!imgui_layer.create(&allocator, imgui_init_info)) {
    EDGE_LOG_FATAL("Failed to initialize ImGuiLayer.");
    return false;
  }
  EDGE_LOG_INFO("ImGuiLayer initialized.");

  const gfx::ImGuiRendererCreateInfo imgui_renderer_create_info = {
      .renderer = &renderer};

  // TODO: This should be optional in future
  if (!imgui_renderer.create(&allocator, imgui_renderer_create_info)) {
    EDGE_LOG_FATAL("Failed to initialize ImGuiRenderer.");
    return false;
  }
  EDGE_LOG_INFO("ImGuiRenderer initialized.");

  if (!frame_time_controller.create()) {
    EDGE_LOG_FATAL("Failed to initialize FrameTimeController.");
    return false;
  }

  test_tex = HANDLE_INVALID;

  constexpr VkSamplerCreateInfo sampler_create_info = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .mipLodBias = 1.0f,
      .anisotropyEnable = VK_TRUE,
      .maxAnisotropy = 4.0f};

  default_sampler_handle = renderer.create_sampler(sampler_create_info);

  return true;
}

void EngineContext::destroy(const NotNull<const Allocator *> alloc) {
  // Wait for all work done
  if (main_queue) {
    main_queue.wait_idle();
  }

  if (copy_queue) {
    copy_queue.wait_idle();
  }

  for (auto &[handle, promise] : pending_images) {
    alloc->deallocate(promise);
  }
  pending_images.destroy(alloc);

  frame_time_controller.destroy();

  imgui_layer.destroy(alloc);
  imgui_renderer.destroy(alloc);
  uploader.destroy(alloc);
  renderer.destroy(alloc);

  if (copy_queue) {
    copy_queue.release();
  }

  if (main_queue) {
    main_queue.release();
  }

  gfx::context_shutdown();

  if (runtime) {
    runtime->deinit(alloc);
    alloc->deallocate(runtime);
    runtime = nullptr;
  }

  input_system.destroy(&allocator);
  event_dispatcher.destroy(alloc);
}

bool EngineContext::run() {
  while (!runtime->requested_close()) {
    sched->tick();
    frame_time_controller.process(
        [this](const f32 delta_time) -> void { tick(delta_time); });
  }

  return true;
}

void EngineContext::tick(const f32 delta_time) {
  runtime->process_events();
  input_system.update();

  imgui_layer.on_frame_begin(delta_time);

  {
    constexpr ImGuiWindowFlags overlay_flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove;

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::SetNextWindowBgAlpha(0.35f);

    if (ImGui::Begin("Debug Overlay", nullptr, overlay_flags)) {
      ImGui::Text("FPS: %u", frame_time_controller.mean_fps);
      ImGui::Text("Delta Time: %.3f ms", delta_time * 1000.0f);
      ImGui::Text("Avg Frame Time: %.3f ms",
                  frame_time_controller.mean_frame_time * 1000.0f);
      ImGui::Text("GPU Delta Time: %.3f ms", renderer.gpu_delta_time);
      ImGui::Text(
          "Swapchain: %ux%u (%u images)", renderer.swapchain.extent.width,
          renderer.swapchain.extent.height, renderer.swapchain.image_count);
    }
    ImGui::End();
  }

  if (test_tex != HANDLE_INVALID) {
    const ImTextureBinding imgui_binding{test_tex, default_sampler_handle};
    ImGui::Image(static_cast<ImTextureRef>(imgui_binding), {512, 512});
  }

  ImGui::ShowDemoWindow();

  imgui_layer.on_frame_end();

  if (renderer.frame_begin()) {
    const auto semaphore =
        uploader.last_submitted_semaphore.load(std::memory_order_acquire);
    if (semaphore.semaphore) {
      for (usize i = pending_images.size(); i > 0; --i) {
        const usize index = i - 1;

        if (auto &[handle, promise] = pending_images[index];
            promise->is_done()) {
          pending_images.remove(index, nullptr);

          gfx::RenderResource *res =
              renderer.get_resource(handle);
          res->state = gfx::ResourceState::TransferDst;

          test_tex = handle;

          renderer.attach_image(handle,
                                promise->value);
          allocator.deallocate(promise);
        }
      }
    }

    imgui_renderer.execute(&allocator);

    renderer.frame_end(&allocator, semaphore);
  }
}
} // namespace edge

using namespace edge;

int edge_main(RuntimeLayout *runtime_layout) {
  int return_value = 0;

#if EDGE_DEBUG
  allocator = Allocator::create_tracking();
#else
  allocator = Allocator::create(
      [](usize size, usize alignment, void *) {
        return mi_aligned_alloc(alignment, size);
      },
      [](void *ptr, void *) { mi_free(ptr); },
      [](void *ptr, usize size, usize alignment, void *) {
        return mi_aligned_recalloc(ptr, 1, size, alignment);
      },
      nullptr);
#endif

  if (!logger.create(&allocator, LogLevel::Trace)) {
    return_value = -1;
    goto cleanup;
  }

  logger_set_global(&logger);

  ILoggerOutput *stdout_output = logger_create_stdout_output(
      &allocator, LogFormat_Default | LogFormat_Color);
  logger.add_output(&allocator, stdout_output);

  ILoggerOutput *file_output = logger_create_file_output(
      &allocator, LogFormat_Default, "log.log", false);
  logger.add_output(&allocator, file_output);

  sched = Scheduler::create(&allocator);
  if (!sched) {
    EDGE_LOG_FATAL("Failed to initialize Scheduler.");
    return_value = -1;
    goto cleanup;
  }

  EngineContext engine = {};
  if (!engine.create(&allocator, runtime_layout)) {
    return_value = -1;
    goto cleanup;
  }

  engine.run();
cleanup:
  engine.destroy(&allocator);

  if (sched) {
    Scheduler::destroy(&allocator, sched);
  }

  logger.destroy(&allocator);

  const usize net_allocated = allocator.get_net();
  assert(net_allocated == 0 && "Memory leaks detected.");

  return return_value;
}