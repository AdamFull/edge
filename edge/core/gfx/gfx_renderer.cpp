#include "gfx_renderer.h"

namespace edge::gfx {

#define EDGE_LOGGER_SCOPE "gfx::Frame"

	Frame::Frame(const Context& context, Image&& image)
		: image_{ std::move(image) } {

		vk::ImageSubresourceRange subresource_range{};
		subresource_range.aspectMask = vk::ImageAspectFlagBits::eColor;
		subresource_range.baseArrayLayer = 0u;
		subresource_range.layerCount = 1u;
		subresource_range.baseMipLevel = 0u;
		subresource_range.levelCount = 1u;
		auto image_view_result = context.create_image_view(image_, subresource_range, vk::ImageViewType::e2D);
	}

	Frame::~Frame() {

	}

#undef EDGE_LOGGER_SCOPE // Frames

#define EDGE_LOGGER_SCOPE "gfx::Renderer"

	auto Renderer::construct(Context&& context, const RendererCreateInfo& create_info) -> Result<std::unique_ptr<Renderer>> {
		auto self = std::make_unique<Renderer>();
		self->context_ = std::move(context);
		if (auto result = self->_construct(create_info); result != vk::Result::eSuccess) {
			context = std::move(self->context_);
			return std::unexpected(result);
		}
		return self;
	}

	auto Renderer::_construct(const RendererCreateInfo& create_info) -> vk::Result {
		auto swapchain_creation_result = SwapchainBuilder{ context_.get_adapter(), context_.get_device(), context_.get_surface() }
			.set_image_extent(create_info.extent)
			.set_image_format(create_info.preferred_format)
			.set_color_space(create_info.preferred_color_space)
			.enable_hdr(create_info.enable_hdr)
			.enable_vsync(create_info.enable_vsync)
			.build();

		if (!swapchain_creation_result) {
			EDGE_SLOGE("Failed to create swapchain. Reason: {}.", vk::to_string(swapchain_creation_result.error()));
			return swapchain_creation_result.error();
		}

		swapchain_ = std::move(swapchain_creation_result.value());

		auto swapchain_images = swapchain_.get_images();

		return vk::Result::eSuccess;
	}

#undef EDGE_LOGGER_SCOPE // Renderer

}