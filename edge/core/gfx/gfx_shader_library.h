#pragma once

#include "gfx_context.h"
#include "gfx_shader_effect.h"

namespace edge::gfx {
	class ShaderLibrary : public NonCopyable {
	public:
		ShaderLibrary() = default;

		ShaderLibrary(Context const& ctx);
		~ShaderLibrary();

		ShaderLibrary(ShaderLibrary&& other)
			: ctx_{ std::exchange(other.ctx_, nullptr) }
			, pipeline_cache_{ std::exchange(other.pipeline_cache_, VK_NULL_HANDLE) }
			, pipeline_cache_path_{ std::exchange(other.pipeline_cache_path_, {}) }
			, pipelines_{ std::exchange(other.pipelines_, {}) } {
		}

		auto operator=(ShaderLibrary&& other) -> ShaderLibrary& {
			ctx_ = std::exchange(other.ctx_, nullptr);
			pipeline_cache_ = std::exchange(other.pipeline_cache_, VK_NULL_HANDLE);
			pipeline_cache_path_ = std::exchange(other.pipeline_cache_path_, {});
			pipelines_ = std::exchange(other.pipelines_, {});
			return *this;
		}

		static auto construct(Context const& ctx, PipelineLayout const& pipeline_layout, mi::WString const& pipeline_cache_path, mi::WString const& shaders_path) -> Result<ShaderLibrary>;
	private:
		auto _construct(PipelineLayout const& pipeline_layout, mi::WString const& shaders_path) -> vk::Result;

		Context const* ctx_{ nullptr };
		PipelineCache pipeline_cache_;
		mi::WString pipeline_cache_path_;
		mi::HashMap<mi::String, Pipeline> pipelines_;
	};
}