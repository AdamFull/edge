#include "gfx_test_pass.h"
#include "gfx_renderer.h"

#include "../../assets/shaders/fullscreen.h"

#define EDGE_LOGGER_SCOPE "TestPass"

namespace edge::gfx {
	auto TestPass::create(Renderer& renderer, uint32_t read_target, Pipeline const* pipeline) -> Owned<TestPass> {
		auto self = std::make_unique<TestPass>();
		self->renderer_ = &renderer;
		self->read_target_ = read_target;
		self->pipeline_ = pipeline;

		self->render_target_ = renderer.create_render_resource();

		ImageCreateInfo image_create_info{};
		image_create_info.extent = vk::Extent3D{ 512u, 512u, 1u };
		image_create_info.face_count = 1u;
		image_create_info.layer_count = 1u;
		image_create_info.level_count = 1u;
		image_create_info.flags = ImageFlag::eWriteColor | ImageFlag::eSample;
		image_create_info.format = vk::Format::eR8G8B8A8Unorm;
		auto image_create_result = Image::create(image_create_info);
		GFX_ASSERT_MSG(image_create_result.has_value(), "Failed to create render target.");

		renderer.setup_render_resource(self->render_target_, std::move(image_create_result.value()), ResourceStateFlag::eUndefined);

		return self;
	}

	auto TestPass::execute(CommandBuffer const& cmd, float delta_time) -> void {
		auto& target_resource = renderer_->get_render_resource(render_target_);
		auto& target_image = target_resource.get_handle<gfx::Image>();
		auto& target_view = target_resource.get_srv_view<ImageView>();
		auto target_extent = target_image.get_extent();

		auto target_state = target_resource.get_state();
		auto required_state = ResourceStateFlag::eRenderTarget;
		if (target_state != required_state) {
			auto src_state = util::get_resource_state(target_state);
			auto dst_state = util::get_resource_state(required_state);

			vk::ImageMemoryBarrier2KHR image_barrier{};
			image_barrier.srcStageMask = src_state.stage_flags;
			image_barrier.srcAccessMask = src_state.access_flags;
			image_barrier.dstStageMask = dst_state.stage_flags;
			image_barrier.dstAccessMask = dst_state.access_flags;
			image_barrier.oldLayout = util::get_image_layout(target_state);
			image_barrier.newLayout = util::get_image_layout(required_state);
			image_barrier.image = target_image.get_handle();
			image_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			image_barrier.subresourceRange.baseMipLevel = 0u;
			image_barrier.subresourceRange.levelCount = target_image.get_level_count();
			image_barrier.subresourceRange.baseArrayLayer = 0u;
			image_barrier.subresourceRange.layerCount = target_image.get_layer_count();
			
			vk::DependencyInfoKHR dependency_info{};
			dependency_info.imageMemoryBarrierCount = 1u;
			dependency_info.pImageMemoryBarriers = &image_barrier;

			cmd->pipelineBarrier2KHR(&dependency_info);

			target_resource.set_state(required_state);
		}

		vk::RenderingAttachmentInfo color_attachment{};
		color_attachment.imageView = target_view.get_handle();
		color_attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
		color_attachment.storeOp = vk::AttachmentStoreOp::eNone;
		color_attachment.clearValue = vk::ClearValue{};

		vk::RenderingInfoKHR rendering_info{};
		rendering_info.renderArea = vk::Rect2D{ { 0u, 0u }, { target_extent.width, target_extent.height } };
		rendering_info.layerCount = 1u;
		rendering_info.colorAttachmentCount = 1u;
		rendering_info.pColorAttachments = &color_attachment;
		cmd->beginRenderingKHR(&rendering_info);

		cmd->bindPipeline(pipeline_->get_bind_point(), pipeline_->get_handle());

		vk::Viewport viewport{ 0.0f, 0.0f, static_cast<float>(target_extent.width), static_cast<float>(target_extent.height) };
		cmd->setViewport(0u, 1u, &viewport);

		vk::Rect2D scissor_rect{ { 0, 0 }, { target_extent.width, target_extent.height } };
		cmd->setScissor(0u, 1u, &scissor_rect);

		auto& resd_resource = renderer_->get_render_resource(read_target_);

		fullscreen::PushConstant push_constant{};
		push_constant.width;
		push_constant.height;
		push_constant.image_id = resd_resource.get_srv_index();

		auto constant_range_ptr = reinterpret_cast<const uint8_t*>(&push_constant);
		renderer_->push_constant_range(cmd,
			vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute,
			{ constant_range_ptr, sizeof(push_constant) });

		cmd->draw(3u, 1u, 0u, 0u);

		cmd->endRenderingKHR();
	}
}

#undef EDGE_LOGGER_SCOPE