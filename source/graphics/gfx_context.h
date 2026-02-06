#ifndef GFX_CONTEXT_H
#define GFX_CONTEXT_H

#include <stdio.h>

#include "gfx_interface.h"

namespace edge::gfx {
struct PipelineBarrierBuilder;
struct DescriptorLayoutBuilder;
struct PipelineLayoutBuilder;

void context_set_object_name(const char *name, VkObjectType type, u64 handle);

template <typename T> struct VulkanHandle {
  T handle = VK_NULL_HANDLE;

  VulkanHandle() = default;
  VulkanHandle(std::nullptr_t) : handle{VK_NULL_HANDLE} {}

  VulkanHandle &operator=(std::nullptr_t) {
    handle = VK_NULL_HANDLE;
    return *this;
  }

  void set_name(const char *name) const {
    context_set_object_name(name, VkObjectTraits<T>::object_type, reinterpret_cast<u64>(handle));
  }

  template <typename... Args>
  void set_name(const char *format, Args &&...args) const {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), format, std::forward<Args>(args)...);
    context_set_object_name(buffer, VkObjectTraits<T>::object_type,
                            reinterpret_cast<u64>(handle));
  }

  explicit operator bool() const { return handle != VK_NULL_HANDLE; }
  operator T() const { return handle; }
};

struct Fence : VulkanHandle<VkFence> {
  using VulkanHandle::VulkanHandle;

  bool create(VkFenceCreateFlags flags);
  void destroy();
  bool wait(u64 timeout) const;
  void reset() const;
};

struct Semaphore : VulkanHandle<VkSemaphore> {
  using VulkanHandle::VulkanHandle;

  VkSemaphoreType sem_type = {};
  u64 sem_value = 0ull;

  bool create(VkSemaphoreType type, u64 value);
  void destroy();
};

// Primitives
struct Queue {
  i32 family_index = -1;
  i32 queue_index = -1;

  explicit operator bool() const {
    return family_index != -1 && queue_index != -1;
  }

  bool request(QueueRequest create_info);
  void release();

  VkQueue get_handle() const;
  bool submit(Fence fence, const VkSubmitInfo2KHR *submit_info) const;
  bool present(const VkPresentInfoKHR *present_info) const;
  void wait_idle() const;
};

struct QueryPool : VulkanHandle<VkQueryPool> {
  using VulkanHandle::VulkanHandle;

  VkQueryType query_type = {};
  u32 max_query = 0u;
  bool host_reset_enabled = false;

  bool create(VkQueryType type, u32 count);
  void destroy();
  void reset() const;
  bool get_data(u32 first_query, void *out_data) const;
};

struct PipelineLayout : VulkanHandle<VkPipelineLayout> {
  using VulkanHandle::VulkanHandle;

  bool create(const PipelineLayoutBuilder &builder);
  void destroy();
};

struct DescriptorSetLayout : VulkanHandle<VkDescriptorSetLayout> {
  using VulkanHandle::VulkanHandle;

  u32 descriptor_sizes[DESCRIPTOR_SIZES_COUNT] = {};

  bool create(const DescriptorLayoutBuilder &builder);
  void destroy();
};

struct DescriptorPool : VulkanHandle<VkDescriptorPool> {
  using VulkanHandle::VulkanHandle;

  u32 descriptor_sizes[DESCRIPTOR_SIZES_COUNT]{};

  bool create(Span<u32> descriptor_sizes_span);
  void destroy();
};

struct DescriptorSet : VulkanHandle<VkDescriptorSet> {
  using VulkanHandle::VulkanHandle;

  DescriptorPool pool = {};

  bool create(const DescriptorPool &descriptor_pool, const DescriptorSetLayout *layouts);
  void destroy();
};

struct PipelineCache : VulkanHandle<VkPipelineCache> {
  using VulkanHandle::VulkanHandle;

  bool create(const u8 *data, usize data_size);
  void destroy();
};

struct ShaderModule : VulkanHandle<VkShaderModule> {
  using VulkanHandle::VulkanHandle;

  bool create(const u32 *code, usize size);
  void destroy();
};

struct ComputePipelineCreateInfo {
  ShaderModule shader_module = {};
  PipelineLayout layout = {};
  PipelineCache cache = {};
};

struct Pipeline : VulkanHandle<VkPipeline> {
  using VulkanHandle::VulkanHandle;

  VkPipelineBindPoint bind_point = {};

  bool create(const VkGraphicsPipelineCreateInfo *create_info);
  bool create(const ComputePipelineCreateInfo &create_info);
  void destroy();
};

struct Sampler : VulkanHandle<VkSampler> {
  using VulkanHandle::VulkanHandle;

  bool create(const VkSamplerCreateInfo &create_info);
  void destroy();
};

struct DeviceMemory : VulkanHandle<VmaAllocation> {
  using VulkanHandle::VulkanHandle;

  VkDeviceSize size = 0;
  void *mapped = nullptr;

  bool coherent = false;
  bool persistent = false;

  void setup();
  bool is_mapped() const;
  void *map();
  void unmap();
  void flush(VkDeviceSize offset, VkDeviceSize size) const;
  void update(const void *data, VkDeviceSize size, VkDeviceSize offset);
};

// TODO: COMPACT DATA
struct Image : VulkanHandle<VkImage> {
  using VulkanHandle::VulkanHandle;

  DeviceMemory memory = {};

  VkExtent3D extent = {1u, 1u, 1u};
  u32 level_count = 1u;
  u32 layer_count = 1u;
  u32 face_count = 1u;
  VkImageUsageFlags usage_flags = 0;
  VkFormat format = VK_FORMAT_UNDEFINED;

  bool create(const ImageCreateInfo &create_info);
  void destroy();
};

struct ImageView : VulkanHandle<VkImageView> {
  using VulkanHandle::VulkanHandle;

  VkImageViewType view_type = {};
  VkImageSubresourceRange range = {};

  bool create(const Image &image, VkImageViewType type,
              const VkImageSubresourceRange &subresource_range);
  void destroy();
};

struct Buffer : VulkanHandle<VkBuffer> {
  using VulkanHandle::VulkanHandle;

  DeviceMemory memory = {};

  BufferFlags flags = 0;
  VkDeviceAddress address = 0;

  bool create(const BufferCreateInfo &create_info);
  void destroy();
};

struct BufferView {
  Buffer buffer = {};
  VkDeviceSize local_offset = 0;
  VkDeviceSize size = 0;

  explicit operator bool() const { return buffer && size != 0; }

  void write(Span<const u8> data, VkDeviceSize offset);
};

struct Swapchain : VulkanHandle<VkSwapchainKHR> {
  using VulkanHandle::VulkanHandle;

  VkFormat format = VK_FORMAT_UNDEFINED;
  VkColorSpaceKHR color_space = {};
  u32 image_count = 1u;
  VkExtent2D extent = {1u, 1u};
  VkPresentModeKHR present_mode = {};
  VkCompositeAlphaFlagBitsKHR composite_alpha = {};

  bool create(SwapchainCreateInfo create_info);
  void destroy();

  bool update();
  bool is_outdated() const;
  bool get_images(Image *image_out) const;
  bool acquire_next_image(u64 timeout, const Semaphore &semaphore,
                          u32 *next_image_idx) const;
};

struct CmdPool : VulkanHandle<VkCommandPool> {
  using VulkanHandle::VulkanHandle;

  bool create(Queue queue);
  void destroy();
};

struct CmdBuf : VulkanHandle<VkCommandBuffer> {
  using VulkanHandle::VulkanHandle;

  CmdPool pool = {};

  bool create(CmdPool cmd_pool);
  void destroy();

  bool begin() const;
  bool end() const;
  void begin_marker(const char *name, u32 color) const;
  void end_marker() const;
  bool reset() const;
  static void reset_query(QueryPool query, u32 first_query, u32 query_count);
  void write_timestamp(const QueryPool &query, VkPipelineStageFlagBits2 stage,
                       u32 query_index) const;
  void bind_descriptor(const PipelineLayout &layout, const DescriptorSet &descriptor,
                       VkPipelineBindPoint bind_point) const;
  void pipeline_barrier(const PipelineBarrierBuilder &builder) const;
  void begin_rendering(const VkRenderingInfoKHR &rendering_info) const;
  void end_rendering() const;
  void bind_index_buffer(const Buffer &buffer, VkIndexType type) const;
  void bind_pipeline(const Pipeline &pipeline) const;
  void set_viewport(const VkViewport &viewport) const;
  void set_viewport(f32 x, f32 y, f32 width, f32 height, f32 depth_min = 0.0f,
                    f32 depth_max = 1.0f) const;
  void set_scissor(VkRect2D rect) const;
  void set_scissor(i32 off_x, i32 off_y, u32 width, u32 height) const;
  void push_constants(PipelineLayout layout, VkShaderStageFlags flags,
                      u32 offset, u32 size, const void *data) const;
  void draw_indexed(u32 idx_cnt, u32 inst_cnt, u32 first_idx, i32 vtx_offset,
                    u32 first_inst) const;
};

struct DescriptorLayoutBuilder {
  VkDescriptorSetLayoutBinding bindings[MAX_BINDING_COUNT] = {};
  VkDescriptorBindingFlagsEXT binding_flags[MAX_BINDING_COUNT] = {};
  u32 binding_count = 0u;

  void add_binding(const VkDescriptorSetLayoutBinding &binding,
                   VkDescriptorBindingFlags flags);
};

struct PipelineBarrierBuilder {
  VkMemoryBarrier2 memory_barriers[MEMORY_BARRIERS_MAX] = {};
  VkBufferMemoryBarrier2 buffer_barriers[BUFFER_BARRIERS_MAX] = {};
  VkImageMemoryBarrier2 image_barriers[IMAGE_BARRIERS_MAX] = {};

  u32 memory_barrier_count = 0u;
  u32 buffer_barrier_count = 0u;
  u32 image_barrier_count = 0u;

  VkDependencyFlags dependency_flags = 0;

  bool add_memory(VkPipelineStageFlags2 src_stage_mask,
                  VkAccessFlags2 src_access_mask,
                  VkPipelineStageFlags2 dst_stage_mask,
                  VkAccessFlags2 dst_access_mask);
  bool add_buffer(const Buffer &buffer, ResourceState old_state,
                  ResourceState new_state, VkDeviceSize offset,
                  VkDeviceSize size);
  bool add_image(const Image &image, ResourceState old_state, ResourceState new_state,
                 const VkImageSubresourceRange &subresource_range);
  void reset();
};

struct PipelineLayoutBuilder {
  VkPushConstantRange constant_ranges[8] = {};
  u32 constant_range_count = 0u;
  VkDescriptorSetLayout descriptor_layouts[MAX_BINDING_COUNT] = {};
  u32 descriptor_layout_count = 0u;

  void add_range(VkShaderStageFlags flags, u32 offset, u32 size);
  void add_layout(const DescriptorSetLayout &layout);
};

struct ContextCreateInfo {
  const Allocator *alloc = nullptr;
  IRuntime *runtime = nullptr;
};

bool context_init(const ContextCreateInfo *create_info);
void context_shutdown();

bool context_is_extension_enabled(const char *name);

const VkPhysicalDeviceProperties *get_adapter_props();

void update_descriptors(const VkWriteDescriptorSet *writes, u32 count);
} // namespace edge::gfx

#endif // GFX_CONTEXT_H