#ifndef GFX_RENDERER_H
#define GFX_RENDERER_H

#include "gfx_context.h"
#include <handle_pool.hpp>
#include <free_index_list.hpp>

namespace edge::gfx {
	struct Renderer;

	constexpr usize RENDERER_FRAME_OVERLAP = 3;

	constexpr usize RENDERER_UAV_MAX = 16;

	constexpr usize RENDERER_SAMPLER_SLOT = 0;
	constexpr usize RENDERER_SRV_SLOT = 1;
	constexpr usize RENDERER_UAV_SLOT = 2;

	constexpr usize RENDERER_HANDLE_MAX = 65535;

	constexpr usize RENDERER_UPDATE_STAGING_ARENA_SIZE = 1048576;

	enum class ResourceType {
		Unknown = -1,
		Image,
		Buffer
	};

	struct RendererCreateInfo {
		const Allocator* alloc = nullptr;
		Queue main_queue = {};
	};

	struct Resource {
		ResourceType type = ResourceType::Unknown;
		union {
			Image image = {};
			Buffer buffer;
		};

		ImageView srv = {};
		u32 srv_index = 0u;

		ImageView uav[RENDERER_UAV_MAX] = {};
		u32 uav_index = 0u;
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

		Array<Resource> free_resources = {};

		bool create(NotNull<const Allocator*> alloc, CmdPool cmd_pool) noexcept;
		void destroy(NotNull<const Allocator*> alloc, NotNull<Renderer*> renderer) noexcept;
		void release_resources(NotNull<Renderer*> renderer) noexcept;

		bool begin(NotNull<Renderer*> renderer) noexcept;

		BufferView try_allocate_staging_memory(NotNull<const Allocator*> alloc, VkDeviceSize required_memory, VkDeviceSize required_alignment) noexcept;
	};

	struct BufferUpdateInfo {
		Buffer dst_buffer = {};
		BufferView buffer_view = {};
		Array<VkBufferCopy2KHR> copy_regions = {};
		VkDeviceSize offset = 0;

		bool write(NotNull<const Allocator*> alloc, Span<const u8> data, VkDeviceSize dst_offset) noexcept;
	};

	struct ImageSubresourceData {
		Span<const u8> data = {};
		uint32_t mip_level = 0;
		uint32_t array_layer = 0;
		VkOffset3D offset = {};
		VkExtent3D extent = {};
	};

	struct ImageUpdateInfo {
		Image dst_image = {};
		BufferView buffer_view = {};
		Array<VkBufferImageCopy2KHR> copy_regions = {};
		VkDeviceSize offset = 0;

		bool write(NotNull<const Allocator*> alloc, const ImageSubresourceData& subresource_info) noexcept;
	};

	struct Renderer {
		Queue direct_queue = {};

		CmdPool cmd_pool = {};

		QueryPool frame_timestamp = {};
		double timestamp_freq = 0.0;
		double gpu_delta_time = 0.0;

		DescriptorSetLayout descriptor_layout = {};
		DescriptorPool descriptor_pool = {};
		DescriptorSet descriptor_set = {};
		PipelineLayout pipeline_layout = {};

		Swapchain swapchain = {};
		Image swapchain_images[8] = {};
		ImageView swapchain_image_views[8] = {};
		u32 active_image_index = 0u;

		RendererFrame frames[RENDERER_FRAME_OVERLAP] = {};
		RendererFrame* active_frame = nullptr;
		u32 frame_number = 0u;

		HandlePool<Resource> resource_handle_pool = {};
		Handle backbuffer_handle = 0;

		FreeIndexList sampler_indices_list = {};
		FreeIndexList srv_indices_list = {};
		FreeIndexList uav_indices_list = {};

		PipelineBarrierBuilder barrier_builder = {};

		Semaphore acquired_semaphore = {};

		Array<VkWriteDescriptorSet> write_descriptor_sets = {};
		Array<VkDescriptorImageInfo> image_descriptors = {};
		Array<VkDescriptorBufferInfo> buffer_descriptors = {};

		Sampler default_sampler = {};

		bool create(RendererCreateInfo create_info) noexcept;
		void destroy(NotNull<const Allocator*> alloc) noexcept;

		Handle add_resource() noexcept;
		bool setup_resource(NotNull<const Allocator*> alloc, Handle handle, Image image) noexcept; // TODO: Maybe attach_resource sounds better
		bool setup_resource(Handle handle, Buffer buffer) noexcept; // TODO: Maybe attach_resource sounds better
		void update_resource(NotNull<const Allocator*> alloc, Handle handle, Image image) noexcept;
		void update_resource(NotNull<const Allocator*> alloc, Handle handle, Buffer buffer) noexcept;
		Resource* get_resource(Handle handle) noexcept;
		void free_resource(NotNull<const Allocator*> alloc, Handle handle) noexcept;

		bool frame_begin() noexcept;
		bool frame_end() noexcept;

		void image_update_end(NotNull<const Allocator*> alloc, ImageUpdateInfo& update_info) noexcept;
		void buffer_update_end(NotNull<const Allocator*> alloc, BufferUpdateInfo& update_info);

		template<typename T>
		void push_constants(VkShaderStageFlags, T data) {
			active_frame->cmd.push_constants(pipeline_layout, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(T), &data);
		}
	};
}

#endif // GFX_RENDERER_H