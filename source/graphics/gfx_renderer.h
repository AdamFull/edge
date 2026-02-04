#ifndef GFX_RENDERER_H
#define GFX_RENDERER_H

#include "gfx_context.h"

#include <scheduler.hpp>
#include <handle_pool.hpp>
#include <free_index_list.hpp>

#include <variant>

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

	struct ImageResource {
		Image handle;
		ImageView srv;
		ImageView uavs[RENDERER_UAV_MAX];

		void destroy();
	};

	struct BufferResource {
		Buffer handle;

		void destroy();
	};

	struct SamplerResource {
		Sampler handle;

		void destroy();
	};

	struct RenderResource {
		using ResourceType = std::variant<std::monostate, ImageResource, BufferResource, SamplerResource>;

		ResourceType resource;
		ResourceState state = ResourceState::Undefined;

		u32 srv_index = ~0u;
		u32 uav_indices[RENDERER_UAV_MAX] = { 
			~0u, ~0u, ~0u, ~0u, 
			~0u, ~0u, ~0u, ~0u, 
			~0u, ~0u, ~0u, ~0u, 
			~0u, ~0u, ~0u, ~0u 
		};

		void destroy();

		bool is_valid() const { return !std::holds_alternative<std::monostate>(resource); }
		bool is_image() const { return std::holds_alternative<ImageResource>(resource); }
		bool is_buffer() const { return std::holds_alternative<BufferResource>(resource); }
		bool is_sampler() const { return std::holds_alternative<SamplerResource>(resource); }

		u32 get_srv_index() const { return srv_index; }
		u32 get_uav_index(usize mip = 0) const {
			if (is_image()) {
				if (auto* img = std::get_if<ImageResource>(&resource)) {
					if (mip >= img->handle.level_count) {
						return ~0u;
					}
					return uav_indices[mip];
				}
			}
			else if (is_buffer()) {
				return uav_indices[0];
			}
			
			return ~0u;
		}

		template<typename T>
		T* as() {
			return std::get_if<T>(&resource);
		}

		Image* as_image() {
			if (auto* img = std::get_if<ImageResource>(&resource)) {
				return &img->handle;
			}
			return nullptr;
		}

		Buffer* as_buffer() {
			if (auto* buf = std::get_if<BufferResource>(&resource)) {
				return &buf->handle;
			}
			return nullptr;
		}

		Sampler* as_sampler() {
			if (auto* smp = std::get_if<SamplerResource>(&resource)) {
				return &smp->handle;
			}
			return nullptr;
		}
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

		Array<RenderResource> pending_destroys = {};

		bool create(NotNull<const Allocator*> alloc, CmdPool cmd_pool);
		void destroy(NotNull<const Allocator*> alloc, NotNull<Renderer*> renderer);

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

	struct StateTranslation {
		Handle handle = HANDLE_INVALID;
		ResourceState new_state = ResourceState::Undefined;
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
		Handle backbuffer_handles[8] = {};
		u32 active_image_index = 0u;

		RendererFrame frames[FRAME_OVERLAP] = {};
		RendererFrame* active_frame = nullptr;
		u32 frame_number = 0u;

		StateTranslation state_translations[IMAGE_BARRIERS_MAX + BUFFER_BARRIERS_MAX] = {};
		usize state_translation_count = 0;

		Semaphore acquired_semaphore = {};

		Array<VkWriteDescriptorSet> write_descriptor_sets = {};
		Array<VkDescriptorImageInfo> image_descriptors = {};
		Array<VkDescriptorBufferInfo> buffer_descriptors = {};

		bool create(NotNull<const Allocator*> alloc, RendererCreateInfo create_info);
		void destroy(NotNull<const Allocator*> alloc);

		Handle create_empty();

		Handle create_image(const ImageCreateInfo& create_info);
		bool attach_image(Handle h, Image image);
		bool update_image(NotNull<const Allocator*> alloc, Handle h, Image img);

		Handle create_buffer(const BufferCreateInfo& create_info);
		bool attach_buffer(Handle h, Buffer buf);
		bool update_buffer(NotNull<const Allocator*> alloc, Handle h, Buffer buf);

		Handle create_sampler(const VkSamplerCreateInfo& create_info);
		bool attach_sampler(Handle handle, Sampler smp);
		bool update_sampler(NotNull<const Allocator*> alloc, Handle h, Sampler smp);

		RenderResource* get_resource(Handle h);
		void free_resource(NotNull<const Allocator*> alloc, Handle h);

		void add_state_translation(Handle h, ResourceState new_state);
		void translate_states(CmdBuf cmd);

		bool frame_begin();
		bool frame_end(NotNull<const Allocator*> alloc, VkSemaphoreSubmitInfoKHR uploader_semaphore);

		void image_update_end(NotNull<const Allocator*> alloc, ImageUpdateInfo& update_info);
		void buffer_update_end(NotNull<const Allocator*> alloc, BufferUpdateInfo& update_info);

		template<typename T>
		void push_constants(VkShaderStageFlags, T data) {
			active_frame->cmd.push_constants(pipeline_layout, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(T), &data);
		}

		Handle get_backbuffer_handle() const;

	private:
		void flush_resource_destruction(RendererFrame& frame);

		void update_srv_descriptor(u32 index, Sampler sampler);
		void update_srv_descriptor(u32 index, ImageView view);
		void update_uav_descriptor(u32 index, ImageView view);

		HandlePool<RenderResource> resource_pool = {};

		FreeIndexList smp_index_allocator = {};
		FreeIndexList srv_index_allocator = {};
		FreeIndexList uav_index_allocator = {};
	};
}

#endif // GFX_RENDERER_H