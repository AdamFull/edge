#pragma once

#include "gfx_context.h"
#include "gfx_shader_effect.h"

namespace edge::gfx {
	class ShaderLibrary : public NonCopyable {
	public:
		ShaderLibrary() = default;
		~ShaderLibrary();

		ShaderLibrary(ShaderLibrary&& other)
			: pipeline_cache_{ std::exchange(other.pipeline_cache_, VK_NULL_HANDLE) }
			, pipeline_cache_path_{ std::exchange(other.pipeline_cache_path_, {}) }
			, pipelines_{ std::exchange(other.pipelines_, {}) } {
		}

		auto operator=(ShaderLibrary&& other) -> ShaderLibrary& {
			pipeline_cache_ = std::exchange(other.pipeline_cache_, VK_NULL_HANDLE);
			pipeline_cache_path_ = std::exchange(other.pipeline_cache_path_, {});
			pipelines_ = std::exchange(other.pipelines_, {});
			return *this;
		}

		static auto construct(PipelineLayout const& pipeline_layout, std::u8string_view pipeline_cache_path, std::u8string_view shaders_path) -> Result<ShaderLibrary>;
	private:
		auto _construct(PipelineLayout const& pipeline_layout, std::u8string_view shaders_path) -> vk::Result;

		PipelineCache pipeline_cache_;
		mi::U8String pipeline_cache_path_;
		mi::HashMap<mi::String, Pipeline> pipelines_;
	};
}