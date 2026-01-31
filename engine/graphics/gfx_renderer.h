#ifndef GFX_RENDERER_H
#define GFX_RENDERER_H

#include "gfx_context.h"

#include <scheduler.hpp>
#include <handle_pool.hpp>
#include <free_index_list.hpp>

namespace edge::gfx {
	struct Renderer;

	constexpr usize RENDERER_UAV_MAX = 16;

	constexpr usize RENDERER_SAMPLER_SLOT = 0;
	constexpr usize RENDERER_SRV_SLOT = 1;
	constexpr usize RENDERER_UAV_SLOT = 2;

	constexpr usize RENDERER_HANDLE_MAX = 65535;

	constexpr usize RENDERER_UPDATE_STAGING_ARENA_SIZE = 1048576;

	struct RendererCreateInfo {
		Queue main_queue = {};
	};

	enum class RenderResourceType {
		None = 0,
		Image, 
		Buffer,
		Sampler
	};

	struct RenderResource {
		RenderResourceType type = RenderResourceType::None;
		union {
			struct {
				Handle handle;
				Handle srv_handle;
				Handle uav_handles[16];
			} image = {};

			struct {
				Handle handle;
			} buffer;

			struct {
				Handle handle;
			} sampler;
		};

		void cleanup(NotNull<Renderer*> renderer);

		Handle get_handle();
		Handle get_srv_handle();
		Handle get_uav_handle(usize index);
	};

	struct RendererFrame {
		Buffer staging_memory = {};
		u64 staging_offset = 0;

		Array<Buffer> temp_staging_memory = {};

		Semaphore image_available = {};
		Semaphore rendering_finished = {};
		Fence fence = {};

		CmdBuf cmd = {};
		bool is_recording = false;

		Array<RenderResource> free_resources = {};

		bool create(NotNull<const Allocator*> alloc, CmdPool cmd_pool);
		void destroy(NotNull<const Allocator*> alloc, NotNull<Renderer*> renderer);
		void release_resources(NotNull<Renderer*> renderer);

		bool begin(NotNull<Renderer*> renderer);

		BufferView try_allocate_staging_memory(NotNull<const Allocator*> alloc, VkDeviceSize required_memory, VkDeviceSize required_alignment);
	};

	struct BufferUpdateInfo {
		Buffer dst_buffer = {};
		BufferView buffer_view = {};
		Array<VkBufferCopy2KHR> copy_regions = {};
		VkDeviceSize offset = 0;

		bool write(NotNull<const Allocator*> alloc, Span<const u8> data, VkDeviceSize dst_offset);
	};

	struct ImageSubresourceData {
		Span<const u8> data = {};
		VkOffset3D offset = {};
		VkExtent3D extent = {};
		u32 mip_level = 0;
		u32 array_layer = 0;
		u32 layer_count = 1;
	};

	struct ImageUpdateInfo {
		Image dst_image = {};
		BufferView buffer_view = {};
		Array<VkBufferImageCopy2KHR> copy_regions = {};
		VkDeviceSize offset = 0;

		bool write(NotNull<const Allocator*> alloc, const ImageSubresourceData& subresource_info);
	};

	struct Renderer {
		Queue direct_queue = {};

		CmdPool cmd_pool = {};

		QueryPool frame_timestamp = {};
		f64 timestamp_freq = 0.0;
		f64 gpu_delta_time = 0.0;

		DescriptorSetLayout descriptor_layout = {};
		DescriptorPool descriptor_pool = {};
		DescriptorSet descriptor_set = {};
		PipelineLayout pipeline_layout = {};

		Swapchain swapchain = {};
		Image swapchain_images[8] = {};
		ImageView swapchain_image_views[8] = {};
		u32 active_image_index = 0u;

		RendererFrame frames[FRAME_OVERLAP] = {};
		RendererFrame* active_frame = nullptr;
		u32 frame_number = 0u;

		HandlePool<RenderResource> resource_handle_pool = {};
		HandlePool<Image> image_handle_pool = {};
		HandlePool<ImageView> image_srv_handle_pool = {};
		HandlePool<ImageView> image_uav_handle_pool = {};
		HandlePool<Sampler> sampler_handle_pool = {};
		HandlePool<Buffer> buffer_handle_pool = {};

		Handle backbuffer_handle = HANDLE_INVALID;

		PipelineBarrierBuilder barrier_builder = {};

		Semaphore acquired_semaphore = {};

		Array<VkWriteDescriptorSet> write_descriptor_sets = {};
		Array<VkDescriptorImageInfo> image_descriptors = {};
		Array<VkDescriptorBufferInfo> buffer_descriptors = {};

		bool create(NotNull<const Allocator*> alloc, RendererCreateInfo create_info);
		void destroy(NotNull<const Allocator*> alloc);

		Handle add_resource();
		bool attach_resource(Handle handle, Image image);
		bool attach_resource(Handle handle, Buffer buffer);
		bool attach_resource(Handle handle, Sampler sampler);

		void update_resource(NotNull<const Allocator*> alloc, Handle handle, Image image);
		void update_resource(NotNull<const Allocator*> alloc, Handle handle, Buffer buffer);

		RenderResource* get_resource(Handle handle);
		void free_resource(NotNull<const Allocator*> alloc, Handle handle);

		bool frame_begin();
		bool frame_end(NotNull<const Allocator*> alloc, VkSemaphoreSubmitInfoKHR uploader_semaphore);

		void image_update_end(NotNull<const Allocator*> alloc, ImageUpdateInfo& update_info);
		void buffer_update_end(NotNull<const Allocator*> alloc, BufferUpdateInfo& update_info);

		template<typename T>
		void push_constants(VkShaderStageFlags, T data) {
			active_frame->cmd.push_constants(pipeline_layout, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(T), &data);
		}
	};
}

#endif // GFX_RENDERER_H