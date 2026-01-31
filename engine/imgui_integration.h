#ifndef IMGUI_INTEGRATION_H
#define IMGUI_INTEGRATION_H

#include <imgui.h>
#include <handle_pool.hpp>

namespace edge {
	struct ImTextureBinding {
		Handle image = HANDLE_INVALID;
		Handle sampler = HANDLE_INVALID;

		ImTextureBinding(Handle img, Handle samp)
			: image{ img }, sampler{ samp } {
		}

		explicit operator ImTextureID() const {
			return (static_cast<ImTextureID>(static_cast<u32>(sampler)) << 32) | static_cast<ImTextureID>(static_cast<u32>(image));
		}

		explicit operator ImTextureRef() const {
			return ImTextureRef{ static_cast<ImTextureID>(*this) };
		}

		static ImTextureBinding from_texture_id(ImTextureID tex_ref) {
			return ImTextureBinding(Handle(static_cast<u32>(tex_ref & 0xFFFFFFFFu)), Handle(static_cast<u32>((tex_ref >> 32) & 0xFFFFFFFFu)));
		}

		bool operator==(const ImTextureBinding& other) const { return image == other.image && sampler == other.sampler; }
		bool operator!=(const ImTextureBinding& other) const { return !(*this == other); }
	};
}

#endif