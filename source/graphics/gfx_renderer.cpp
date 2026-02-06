#include "gfx_renderer.h"
#include "gfx_context.h"

#include <array.hpp>
#include <logger.hpp>
#include <inttypes.h>
#include <math.hpp>

#include <atomic>
#include <utility>

#include <volk.h>
#include <vulkan/vulkan.h>

namespace edge::gfx {
[[maybe_unused]] static bool is_depth_format(const VkFormat format) {
  return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT;
}

[[maybe_unused]] static bool is_depth_stencil_format(const VkFormat format) {
  return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
         format == VK_FORMAT_D16_UNORM_S8_UINT ||
         format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void ImageResource::destroy() {
  for (usize i = 0; i < handle.level_count; ++i) {
    uavs[i].destroy();
  }
  srv.destroy();
  handle.destroy();
}

void BufferResource::destroy() { handle.destroy(); }

void SamplerResource::destroy() { handle.destroy(); }

void RenderResource::destroy() {
  std::visit(
      []<typename T0>(T0 &&arg) {
        using T = std::decay_t<T0>;
        if constexpr (!std::is_same_v<T, std::monostate>) {
          arg.destroy();
        }
      },
      resource);
}

bool RendererFrame::create(const NotNull<const Allocator *> alloc,
                           const CmdPool cmd_pool) {
  constexpr BufferCreateInfo buffer_create_info = {.size =
                                             RENDERER_UPDATE_STAGING_ARENA_SIZE,
                                         .alignment = 1,
                                         .flags = BUFFER_FLAG_STAGING};

  if (!staging_memory.create(buffer_create_info)) {
    return false;
  }
  staging_memory.set_name("frame_staging_memory[%p]", this);

  temp_staging_memory.reserve(alloc, 128);

  if (!image_available.create(VK_SEMAPHORE_TYPE_BINARY, 0)) {
    return false;
  }
  image_available.set_name("frame_image_available_semaphore[%p]", this);

  if (!rendering_finished.create(VK_SEMAPHORE_TYPE_BINARY, 0)) {
    return false;
  }
  rendering_finished.set_name("frame_rendering_finished_semaphore[%p]", this);

  if (!fence.create(VK_FENCE_CREATE_SIGNALED_BIT)) {
    return false;
  }
  fence.set_name("frame_fence[%p]", this);

  if (!cmd.create(cmd_pool)) {
    return false;
  }
  cmd.set_name("frame_cmd_list[%p]", this);

  if (!pending_destroys.reserve(alloc, 256)) {
    return false;
  }

  return true;
}

void RendererFrame::destroy(const NotNull<const Allocator *> alloc,
                            NotNull<Renderer *> renderer) {
  staging_memory.destroy();

  for (auto &buffer : temp_staging_memory) {
    buffer.destroy();
  }
  temp_staging_memory.destroy(alloc);

  pending_destroys.destroy(alloc);

  cmd.destroy();
  fence.destroy();
  rendering_finished.destroy();
  image_available.destroy();
}

bool RendererFrame::begin(NotNull<Renderer *> renderer) {
  if (is_recording) {
    return false;
  }

  fence.wait(1000000000ull);
  fence.reset();
  cmd.reset();

  is_recording = cmd.begin();

  staging_offset = 0;

  for (Buffer &buffer : temp_staging_memory) {
    buffer.destroy();
  }
  temp_staging_memory.clear();

  return is_recording;
}

BufferView
RendererFrame::try_allocate_staging_memory(
    const NotNull<const Allocator *> alloc, const VkDeviceSize required_memory,
    const VkDeviceSize required_alignment) {
  if (!is_recording) {
    return {};
  }

  const VkDeviceSize aligned_requested_size =
      align_up(required_memory, required_alignment);

  if (const VkDeviceSize available_size =
          staging_memory.memory.size - staging_offset;
      staging_memory.memory.size < aligned_requested_size ||
      available_size < aligned_requested_size) {
    const BufferCreateInfo create_info = {.size = required_memory,
                                    .alignment = required_alignment,
                                    .flags = BUFFER_FLAG_STAGING};

    Buffer new_buffer;
    if (!new_buffer.create(create_info) ||
        !temp_staging_memory.push_back(alloc, new_buffer)) {
      return {};
    }

    return BufferView{.buffer = new_buffer,
                      .local_offset = 0,
                      .size = aligned_requested_size};
  }

  return BufferView{.buffer = staging_memory,
                    .local_offset =
                        std::exchange(staging_offset,
                                      staging_offset + aligned_requested_size),
                    .size = aligned_requested_size};
}

bool BufferUpdateInfo::write(const NotNull<const Allocator *> alloc,
                             const Span<const u8> data,
                             const VkDeviceSize dst_offset) {
  if (const VkDeviceSize available_size = buffer_view.size - offset;
      data.size() > available_size) {
    return false;
  }

  buffer_view.write(data, offset);
  copy_regions.push_back(
      alloc, {.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2_KHR,
              .srcOffset = buffer_view.local_offset +
                           std::exchange(offset, offset + data.size()),
              .dstOffset = dst_offset,
              .size = data.size()});

  return true;
}

bool ImageUpdateInfo::write(const NotNull<const Allocator *> alloc,
                            const ImageSubresourceData &subresource_info) {
  if (const VkDeviceSize available_size = buffer_view.size - offset;
      subresource_info.data.size() > available_size) {
    return false;
  }

  buffer_view.write(subresource_info.data, offset);
  copy_regions.push_back(
      alloc,
      {.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2_KHR,
       .bufferOffset =
           buffer_view.local_offset +
           std::exchange(offset, offset + subresource_info.data.size()),
       .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .mipLevel = subresource_info.mip_level,
                            .baseArrayLayer = subresource_info.array_layer,
                            .layerCount = subresource_info.layer_count},
       .imageOffset = subresource_info.offset,
       .imageExtent = subresource_info.extent});

  return true;
}

bool Renderer::create(const NotNull<const Allocator *> alloc,
                      const RendererCreateInfo create_info) {
  if (!resource_pool.create(alloc, RENDERER_HANDLE_MAX * 2)) {
    destroy(alloc);
    return false;
  }

  if (!smp_index_allocator.create(alloc, RENDERER_HANDLE_MAX)) {
    destroy(alloc);
    return false;
  }

  if (!srv_index_allocator.create(alloc, RENDERER_HANDLE_MAX)) {
    destroy(alloc);
    return false;
  }

  if (!uav_index_allocator.create(alloc, RENDERER_HANDLE_MAX)) {
    destroy(alloc);
    return false;
  }

  if (!write_descriptor_sets.reserve(alloc, 256)) {
    destroy(alloc);
    return false;
  }

  if (!image_descriptors.reserve(alloc, 256)) {
    destroy(alloc);
    return false;
  }

  if (!buffer_descriptors.reserve(alloc, 256)) {
    destroy(alloc);
    return false;
  }

  if (!create_info.main_queue) {
    return false;
  }

  direct_queue = create_info.main_queue;

  if (!cmd_pool.create(direct_queue)) {
    destroy(alloc);
    return false;
  }
  cmd_pool.set_name("direct_cmd_pool");

  if (!frame_timestamp.create(VK_QUERY_TYPE_TIMESTAMP, 1)) {
    destroy(alloc);
    return false;
  }
  frame_timestamp.set_name("timestamp_query");

  const VkPhysicalDeviceProperties *props = get_adapter_props();
  timestamp_freq = props->limits.timestampPeriod;

  DescriptorLayoutBuilder descriptor_layout_builder = {};

  constexpr VkDescriptorSetLayoutBinding samplers_binding = {
      .binding = RENDERER_SAMPLER_SLOT,
      .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
      .descriptorCount = RENDERER_HANDLE_MAX,
      .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT};

  constexpr VkDescriptorSetLayoutBinding srv_image_binding = {
      .binding = RENDERER_SRV_SLOT,
      .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
      .descriptorCount = RENDERER_HANDLE_MAX,
      .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT};

  constexpr VkDescriptorSetLayoutBinding uav_image_binding = {
      .binding = RENDERER_UAV_SLOT,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      .descriptorCount = RENDERER_HANDLE_MAX,
      .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT};

  constexpr VkDescriptorBindingFlags descriptor_binding_flags =
      VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

  descriptor_layout_builder.add_binding(samplers_binding,
                                        descriptor_binding_flags);
  descriptor_layout_builder.add_binding(srv_image_binding,
                                        descriptor_binding_flags);
  descriptor_layout_builder.add_binding(uav_image_binding,
                                        descriptor_binding_flags);

  if (!descriptor_layout.create(descriptor_layout_builder)) {
    destroy(alloc);
    return false;
  }
  descriptor_layout.set_name("bindless_layout");

  if (!descriptor_pool.create(descriptor_layout.descriptor_sizes)) {
    destroy(alloc);
    return false;
  }
  descriptor_pool.set_name("bindless_pool");

  if (!descriptor_set.create(descriptor_pool, &descriptor_layout)) {
    destroy(alloc);
    return false;
  }
  descriptor_set.set_name("bindless_set");

  PipelineLayoutBuilder pipeline_layout_builder = {};
  pipeline_layout_builder.add_layout(descriptor_layout);
  pipeline_layout_builder.add_range(VK_SHADER_STAGE_ALL_GRAPHICS |
                                        VK_SHADER_STAGE_COMPUTE_BIT,
                                    0u, props->limits.maxPushConstantsSize);

  if (!pipeline_layout.create(pipeline_layout_builder)) {
    destroy(alloc);
    return false;
  }
  pipeline_layout.set_name("base_pipeline_layout");

  SwapchainCreateInfo swapchain_create_info = {};
  if (!swapchain.create(swapchain_create_info)) {
    destroy(alloc);
    return false;
  }

  if (!swapchain.get_images(swapchain_images)) {
    destroy(alloc);
    return false;
  }

  for (usize i = 0; i < swapchain.image_count; ++i) {
    constexpr VkImageSubresourceRange subresource_range = {.aspectMask =
                                                     VK_IMAGE_ASPECT_COLOR_BIT,
                                                 .baseMipLevel = 0u,
                                                 .levelCount = 1u,
                                                 .baseArrayLayer = 0u,
                                                 .layerCount = 1u};

    ImageResource img_res = {};
    img_res.handle = swapchain_images[i];

    if (!img_res.srv.create(swapchain_images[i], VK_IMAGE_VIEW_TYPE_2D,
                            subresource_range)) {
      destroy(alloc);
      return false;
    }

    swapchain_images[i].set_name("backbuffer[%" PRIu64 "]", i);
    img_res.srv.set_name("backbuffer_view[%" PRIu64 "]", i);

    backbuffer_handles[i] = create_empty();
    RenderResource *res = resource_pool.get(backbuffer_handles[i]);

    if (!srv_index_allocator.allocate(&res->srv_index)) {
      destroy(alloc);
      return false;
    }

    res->resource = img_res;
  }

  for (auto & frame : frames) {
    if (!frame.create(alloc, cmd_pool)) {
      destroy(alloc);
      return false;
    }
  }

  return true;
}

void Renderer::destroy(const NotNull<const Allocator *> alloc) {
  direct_queue.wait_idle();

  write_descriptor_sets.destroy(alloc);
  image_descriptors.destroy(alloc);
  buffer_descriptors.destroy(alloc);

  for (auto & frame : frames) {
    flush_resource_destruction(frame);
    frame.destroy(alloc, this);
  }

  for (auto [handle, element] : resource_pool) {
    if (element) {
      element->destroy();
    }
  }
  resource_pool.destroy(alloc);

  smp_index_allocator.destroy(alloc);
  srv_index_allocator.destroy(alloc);
  uav_index_allocator.destroy(alloc);

  swapchain.destroy();
  pipeline_layout.destroy();
  descriptor_set.destroy();
  descriptor_pool.destroy();
  descriptor_layout.destroy();
  frame_timestamp.destroy();
  cmd_pool.destroy();
}

Handle Renderer::create_empty() {
  if (resource_pool.is_full()) {
    return HANDLE_INVALID;
  }
  return resource_pool.allocate();
}

Handle Renderer::create_image(const ImageCreateInfo &create_info) {
  Image image;
  if (!image.create(create_info)) {
    return HANDLE_INVALID;
  }

  const Handle h = resource_pool.allocate();
  if (!attach_image(h, image)) {
    image.destroy();
    return HANDLE_INVALID;
  }

  return h;
}

bool Renderer::attach_image(const Handle h, const Image &image) {
  RenderResource *res = resource_pool.get(h);
  if (!res) {
    return false;
  }

  ImageResource img_res = {};
  img_res.handle = image;

  const VkImageAspectFlags image_aspect =
      (image.usage_flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
          ? VK_IMAGE_ASPECT_DEPTH_BIT
          : VK_IMAGE_ASPECT_COLOR_BIT;

  VkImageViewType view_type = {};
  if (image.extent.depth > 1) {
    view_type = VK_IMAGE_VIEW_TYPE_3D;
  } else if (image.extent.height > 1) {
    if (image.layer_count > 1) {
      view_type = (image.face_count > 1) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
                                       : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    } else {
      view_type = (image.face_count > 1) ? VK_IMAGE_VIEW_TYPE_CUBE
                                       : VK_IMAGE_VIEW_TYPE_2D;
    }
  } else if (image.extent.width > 1) {
    view_type = image.layer_count > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY
                                    : VK_IMAGE_VIEW_TYPE_1D;
  }

  if (image.usage_flags & VK_IMAGE_USAGE_SAMPLED_BIT) {
    const VkImageSubresourceRange srv_subresource_range = {
        .aspectMask = image_aspect,
        .baseMipLevel = 0u,
        .levelCount = image.level_count,
        .baseArrayLayer = 0u,
        .layerCount = image.layer_count * image.face_count};

    if (!img_res.srv.create(image, view_type, srv_subresource_range)) {
      img_res.destroy();
      return false;
    }

    if (!srv_index_allocator.allocate(&res->srv_index)) {
      img_res.destroy();
      return false;
    }

    update_srv_descriptor(res->srv_index, img_res.srv);
  }

  if (image.usage_flags & VK_IMAGE_USAGE_STORAGE_BIT) {
    VkImageSubresourceRange uav_subresource_range = {
        .aspectMask = image_aspect,
        .baseMipLevel = 0u,
        .levelCount = 1u,
        .baseArrayLayer = 0u,
        .layerCount = image.layer_count * image.face_count};

    for (usize i = 0; i < image.level_count; ++i) {
      uav_subresource_range.baseMipLevel = i;

      if (!img_res.uavs[i].create(img_res.handle, view_type,
                                  uav_subresource_range)) {
        img_res.destroy();
        return false;
      }

      if (!uav_index_allocator.allocate(&res->uav_indices[i])) {
        img_res.destroy();
        return false;
      }

      update_uav_descriptor(res->uav_indices[i], img_res.uavs[i]);
    }
  }

  res->resource = img_res;
  return true;
}

bool Renderer::update_image(const NotNull<const Allocator *> alloc,
                            const Handle h, const Image &img) {
  if (!resource_pool.is_valid(h)) {
    return false;
  }

  if (active_frame) {
    const RenderResource *resource = resource_pool.get(h);
    assert(resource && "Resource should not be null.");
    active_frame->pending_destroys.push_back(alloc, *resource);
  }

  attach_image(h, img);
  return true;
}

Handle Renderer::create_buffer(const BufferCreateInfo &create_info) {
  Buffer buffer;
  if (!buffer.create(create_info)) {
    return HANDLE_INVALID;
  }

  const Handle h = resource_pool.allocate();
  if (!attach_buffer(h, buffer)) {
    buffer.destroy();
    return HANDLE_INVALID;
  }

  return h;
}

bool Renderer::attach_buffer(const Handle h, const Buffer &buf) {
  RenderResource *res = resource_pool.get(h);
  if (!res) {
    return false;
  }

  BufferResource buf_res = {};
  buf_res.handle = buf;
  res->resource = buf_res;

  return true;
}

bool Renderer::update_buffer(const NotNull<const Allocator *> alloc,
                             const Handle h,
                             const Buffer &buf) {
  if (!resource_pool.is_valid(h)) {
    return false;
  }

  if (active_frame) {
    const RenderResource *resource = resource_pool.get(h);
    assert(resource && "Resource should not be null.");
    active_frame->pending_destroys.push_back(alloc, *resource);
  }

  attach_buffer(h, buf);
  return true;
}

Handle Renderer::create_sampler(const VkSamplerCreateInfo &create_info) {
  Sampler sampler;
  if (!sampler.create(create_info)) {
    return HANDLE_INVALID;
  }

  const Handle h = resource_pool.allocate();
  if (!attach_sampler(h, sampler)) {
    sampler.destroy();
    return HANDLE_INVALID;
  }

  return h;
}

bool Renderer::attach_sampler(const Handle h, const Sampler& smp) {
  RenderResource *res = resource_pool.get(h);
  if (!res) {
    return false;
  }

  if (!smp_index_allocator.allocate(&res->srv_index)) {
    return false;
  }

  SamplerResource smp_res = {};
  smp_res.handle = smp;
  res->resource = smp_res;

  update_srv_descriptor(res->srv_index, smp_res.handle);

  return true;
}

bool Renderer::update_sampler(const NotNull<const Allocator *> alloc,
                              const Handle h,
                              const Sampler& smp) {
  if (!resource_pool.is_valid(h)) {
    return false;
  }

  if (active_frame) {
    const RenderResource *resource = resource_pool.get(h);
    assert(resource && "Resource should not be null.");
    active_frame->pending_destroys.push_back(alloc, *resource);
  }

  attach_sampler(h, smp);
  return true;
}

RenderResource *Renderer::get_resource(const Handle h) {
  return resource_pool.get(h);
}

void Renderer::free_resource(const NotNull<const Allocator *> alloc, const Handle h) {
  if (resource_pool.is_valid(h)) {
    if (active_frame) {
      const RenderResource *resource = resource_pool.get(h);
      assert(resource && "Resource should not be null.");
      active_frame->pending_destroys.push_back(alloc, *resource);
    }
    resource_pool.free(h);
  }
}

void Renderer::add_state_translation(const Handle h, const ResourceState new_state) {
  if (const RenderResource *res = resource_pool.get(h);
      !res || res->state == new_state) {
    return;
  }

  state_translations[state_translation_count++] = {.handle = h,
                                                   .new_state = new_state};
}

void Renderer::translate_states(CmdBuf cmd) {
  PipelineBarrierBuilder builder = {};

  for (usize i = 0; i < state_translation_count; ++i) {
    const auto &[handle, new_state] = state_translations[i];

    RenderResource *res = resource_pool.get(handle);
    [[unlikely]] if (!res) { continue; }

    std::visit(
        [&]<typename T0>(T0 &&data) {
          using T = std::decay_t<T0>;

          if constexpr (std::is_same_v<T, ImageResource>) {
            const VkImageAspectFlags aspect_mask =
                (is_depth_format(data.handle.format) ||
                 is_depth_stencil_format(data.handle.format))
                    ? VK_IMAGE_ASPECT_DEPTH_BIT
                    : VK_IMAGE_ASPECT_COLOR_BIT;

            const VkImageSubresourceRange subresource_range = {
                .aspectMask = aspect_mask,
                .baseMipLevel = 0u,
                .levelCount = data.handle.level_count,
                .baseArrayLayer = 0u,
                .layerCount = data.handle.layer_count};

            builder.add_image(data.handle, res->state, new_state,
                              subresource_range);
          } else if constexpr (std::is_same_v<T, BufferResource>) {
            builder.add_buffer(data.handle, res->state, new_state,
                               0, VK_WHOLE_SIZE);
          }
          res->state = new_state;
        },
        res->resource);
  }

  cmd.pipeline_barrier(builder);
  state_translation_count = 0;
}

bool Renderer::frame_begin() {
  if (swapchain.is_outdated()) {

    if (direct_queue) {
      direct_queue.wait_idle();
    }

    if (!swapchain.update()) {
      return false;
    }

    if (!swapchain.get_images(swapchain_images)) {
      return false;
    }

    for (usize i = 0; i < swapchain.image_count; ++i) {
      constexpr VkImageSubresourceRange subresource_range = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel = 0u,
          .levelCount = 1u,
          .baseArrayLayer = 0u,
          .layerCount = 1u};

      // Update swapchain resources
      RenderResource *res = resource_pool.get(backbuffer_handles[i]);
      auto *img_res = res->as<ImageResource>();
      img_res->handle = swapchain_images[i];
      img_res->srv.destroy();
      if (!img_res->srv.create(swapchain_images[i], VK_IMAGE_VIEW_TYPE_2D,
                               subresource_range)) {
        return false;
      }
      res->state = ResourceState::Undefined;
    }

    active_frame = nullptr;
    active_image_index = 0;
  }

  RendererFrame &current_frame = frames[frame_number % FRAME_OVERLAP];
  if (!current_frame.begin(this)) {
    return false;
  }

  // Free old resources
  flush_resource_destruction(current_frame);

  acquired_semaphore = current_frame.image_available;

  if (!swapchain.acquire_next_image(1000000000ull, acquired_semaphore,
                                    &active_image_index)) {
    return false;
  }

  active_frame = &current_frame;

  if (frame_number > 0) {
    u64 timestamps[2] = {};
    if (frame_timestamp.get_data(0, timestamps)) {
      u64 elapsed_time = timestamps[1] - timestamps[0];
      if (timestamps[1] <= timestamps[0]) {
        elapsed_time = 0ull;
      }

      gpu_delta_time = static_cast<f64>(elapsed_time) * timestamp_freq / 1000000.0;
    }
  }

  current_frame.cmd.reset_query(frame_timestamp, 0u, 2u);
  current_frame.cmd.write_timestamp(frame_timestamp,
                                    VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0u);

  current_frame.cmd.bind_descriptor(pipeline_layout, descriptor_set,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS);
  current_frame.cmd.bind_descriptor(pipeline_layout, descriptor_set,
                                    VK_PIPELINE_BIND_POINT_COMPUTE);

  return true;
}

bool Renderer::frame_end(const NotNull<const Allocator *> alloc,
                         const VkSemaphoreSubmitInfoKHR &uploader_semaphore) {
  if (!active_frame || !active_frame->is_recording) {
    return false;
  }

  CmdBuf cmd = active_frame->cmd;

  add_state_translation(backbuffer_handles[active_image_index],
                        ResourceState::Present);
  translate_states(cmd);

  if (!write_descriptor_sets.empty()) {
    update_descriptors(write_descriptor_sets.data(),
                       write_descriptor_sets.size());

    write_descriptor_sets.clear();
    image_descriptors.clear();
    buffer_descriptors.clear();
  }

  cmd.write_timestamp(frame_timestamp, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 1u);
  cmd.end();

  VkSemaphoreSubmitInfo wait_semaphores[2] = {};
  wait_semaphores[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  wait_semaphores[0].semaphore = acquired_semaphore;
  wait_semaphores[0].stageMask =
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  wait_semaphores[1] = uploader_semaphore;

  VkSemaphoreSubmitInfo signal_semaphores[2] = {};
  signal_semaphores[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  signal_semaphores[0].semaphore = active_frame->rendering_finished;
  signal_semaphores[0].stageMask =
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

  i32 cmd_buffer_count = 0;
  VkCommandBufferSubmitInfo cmd_buffer_submit_infos[6];
  cmd_buffer_submit_infos[cmd_buffer_count++] = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
      .commandBuffer = cmd};

  const VkSubmitInfo2KHR submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
      .waitSemaphoreInfoCount = uploader_semaphore.semaphore ? 2u : 1u,
      .pWaitSemaphoreInfos = wait_semaphores,
      .commandBufferInfoCount = static_cast<u32>(cmd_buffer_count),
      .pCommandBufferInfos = cmd_buffer_submit_infos,
      .signalSemaphoreInfoCount = 1,
      .pSignalSemaphoreInfos = signal_semaphores};

  if (!direct_queue.submit(active_frame->fence, &submit_info)) {
    active_frame->is_recording = false;
    active_frame = nullptr;
    return false;
  }

  const VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &signal_semaphores[0].semaphore,
      .swapchainCount = 1,
      .pSwapchains = &swapchain.handle,
      .pImageIndices = &active_image_index};

  if (!direct_queue.present(&present_info)) {
    active_frame->is_recording = false;
    active_frame = nullptr;
    return false;
  }

  active_frame->is_recording = false;
  active_frame = nullptr;

  frame_number++;

  return true;
}

void Renderer::image_update_end(const NotNull<const Allocator *> alloc,
                                ImageUpdateInfo &update_info) const {
  const VkCopyBufferToImageInfo2KHR copy_image_info = {
      .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2_KHR,
      .srcBuffer = update_info.buffer_view.buffer,
      .dstImage = update_info.dst_image,
      .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .regionCount = static_cast<u32>(update_info.copy_regions.size()),
      .pRegions = update_info.copy_regions.data()};

  vkCmdCopyBufferToImage2KHR(active_frame->cmd, &copy_image_info);
  update_info.copy_regions.destroy(alloc);
}

void Renderer::buffer_update_end(const NotNull<const Allocator *> alloc,
                                 BufferUpdateInfo &update_info) const {
  const VkCopyBufferInfo2KHR copy_buffer_info = {
      .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2_KHR,
      .srcBuffer = update_info.buffer_view.buffer,
      .dstBuffer = update_info.dst_buffer,
      .regionCount = static_cast<u32>(update_info.copy_regions.size()),
      .pRegions = update_info.copy_regions.data()};

  vkCmdCopyBuffer2KHR(active_frame->cmd, &copy_buffer_info);
  update_info.copy_regions.destroy(alloc);
}

Handle Renderer::get_backbuffer_handle() const {
  return backbuffer_handles[active_image_index];
}

void Renderer::flush_resource_destruction(RendererFrame &frame) {
  for (auto &resource : frame.pending_destroys) {
    std::visit(
        [this, &resource]<typename T0>(T0 &&res) {
          using T = std::decay_t<T0>;
          if constexpr (std::is_same_v<T, ImageResource>) {
            if (resource.srv_index != ~0u) {
              srv_index_allocator.free(resource.srv_index);
            }

            for (usize i = 0; i < res.handle.layer_count; ++i) {
              uav_index_allocator.free(resource.uav_indices[i]);
            }

            res.destroy();
          } else if constexpr (std::is_same_v<T, BufferResource>) {
            res.destroy();
          } else if constexpr (std::is_same_v<T, SamplerResource>) {
            if (resource.srv_index != ~0u) {
              smp_index_allocator.free(resource.srv_index);
            }

            res.destroy();
          }
        },
        resource.resource);
  }
  frame.pending_destroys.clear();
}

void Renderer::update_srv_descriptor(const u32 index, const Sampler &sampler) {
  image_descriptors.push_back({.sampler = sampler});

  const VkWriteDescriptorSet sampler_write = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptor_set,
      .dstBinding = RENDERER_SAMPLER_SLOT,
      .dstArrayElement = index,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
      .pImageInfo = &image_descriptors.back()};
  write_descriptor_sets.push_back(sampler_write);
}

void Renderer::update_srv_descriptor(const u32 index, const ImageView &view) {
  const VkDescriptorImageInfo image_descriptor = {
      .imageView = view, .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR};
  image_descriptors.push_back(image_descriptor);

  const VkWriteDescriptorSet descriptor_write = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptor_set,
      .dstBinding = RENDERER_SRV_SLOT,
      .dstArrayElement = index,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
      .pImageInfo = &image_descriptors.back()};
  write_descriptor_sets.push_back(descriptor_write);
}

void Renderer::update_uav_descriptor(const u32 index, const ImageView &view) {
  const VkDescriptorImageInfo image_descriptor = {
      .imageView = view, .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR};
  image_descriptors.push_back(image_descriptor);

  const VkWriteDescriptorSet descriptor_write = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptor_set,
      .dstBinding = RENDERER_UAV_SLOT,
      .dstArrayElement = index,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      .pImageInfo = &image_descriptors.back()};
  write_descriptor_sets.push_back(descriptor_write);
}
} // namespace edge::gfx