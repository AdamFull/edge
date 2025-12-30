#ifndef GFX_CONTEXT_H
#define GFX_CONTEXT_H

#include "gfx_interface.h"

namespace edge::gfx {
	struct ContextCreateInfo {
		const Allocator* alloc = nullptr;
		PlatformContext* platform_context = nullptr;
	};

	bool context_init(const ContextCreateInfo* cteate_info);
	void context_shutdown();

	bool context_is_extension_enabled(const char* name);

	const VkPhysicalDeviceProperties* get_adapter_props();

	void updete_descriptors(const VkWriteDescriptorSet* writes, u32 count);
}

#endif // GFX_CONTEXT_H