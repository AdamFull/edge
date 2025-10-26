#pragma once

#include "gfx_context.h"
#include "gfx_shader_effect.h"

namespace edge::gfx {
	struct ShaderLibraryInfo {
		PipelineLayout const* pipeline_layout{ nullptr };
		mi::U8String pipeline_cache_path{};
		mi::U8String library_path{};
		vk::Format backbuffer_format{};
	};

	class ShaderLibrary : public NonCopyable {
	public:
		ShaderLibrary() = default;
		~ShaderLibrary();

		ShaderLibrary(ShaderLibrary&& other)
			: pipeline_cache_{ std::move(other.pipeline_cache_) }
			, pipeline_cache_path_{ std::move(other.pipeline_cache_path_) }
			, pipeline_layout_{ std::exchange(other.pipeline_layout_, nullptr) }
			, backbuffer_format_{ std::exchange(other.backbuffer_format_, vk::Format::eUndefined) }
			, pipelines_{ std::exchange(other.pipelines_, {}) } {
		}

		auto operator=(ShaderLibrary&& other) -> ShaderLibrary& {
			if (this != &other) {
				pipeline_cache_ = std::move(other.pipeline_cache_);
				pipeline_cache_path_ = std::move(other.pipeline_cache_path_);
				pipeline_layout_ = std::exchange(other.pipeline_layout_, nullptr);
				backbuffer_format_ = std::exchange(other.backbuffer_format_, vk::Format::eUndefined);
				pipelines_ = std::exchange(other.pipelines_, {});
			}
			return *this;
		}

		static auto construct(const ShaderLibraryInfo& info) -> Result<ShaderLibrary>;
	private:
		auto _construct(const ShaderLibraryInfo& info) -> vk::Result;

		PipelineCache pipeline_cache_;
		mi::U8String pipeline_cache_path_;

		PipelineLayout const* pipeline_layout_{ nullptr };
		vk::Format backbuffer_format_{};

		mi::HashMap<mi::String, Pipeline> pipelines_;
	};
}