#include "imgui_layer.h"
#include "core/platform/platform.h"
#include "core/gfx/gfx_renderer.h"
#include "core/gfx/gfx_resource_updater.h"
#include "core/gfx/gfx_resource_uploader.h"

#include "../assets/shaders/imgui.h"

#include <imgui.h>

#define EDGE_LOGGER_SCOPE "ImGuiLayer"

namespace edge {
	inline constexpr auto translate_key_code(KeyboardKeyCode code) -> ImGuiKey {
		switch (code)
		{
		case KeyboardKeyCode::eUnknown: return ImGuiKey_None;
		case KeyboardKeyCode::eSpace: return ImGuiKey_Space;
		case KeyboardKeyCode::eApostrophe: return ImGuiKey_Apostrophe;
		case KeyboardKeyCode::eComma: return ImGuiKey_Comma;
		case KeyboardKeyCode::eMinus: return ImGuiKey_Minus;
		case KeyboardKeyCode::ePeriod: return ImGuiKey_Period;
		case KeyboardKeyCode::eSlash: return ImGuiKey_Slash;
		case KeyboardKeyCode::e0: return ImGuiKey_0;
		case KeyboardKeyCode::e1: return ImGuiKey_1;
		case KeyboardKeyCode::e2: return ImGuiKey_2;
		case KeyboardKeyCode::e3: return ImGuiKey_3;
		case KeyboardKeyCode::e4: return ImGuiKey_4;
		case KeyboardKeyCode::e5: return ImGuiKey_5;
		case KeyboardKeyCode::e6: return ImGuiKey_6;
		case KeyboardKeyCode::e7: return ImGuiKey_7;
		case KeyboardKeyCode::e8: return ImGuiKey_8;
		case KeyboardKeyCode::e9: return ImGuiKey_9;
		case KeyboardKeyCode::eSemicolon: return ImGuiKey_Semicolon;
		case KeyboardKeyCode::eEq: return ImGuiKey_Equal;
		case KeyboardKeyCode::eA: return ImGuiKey_A;
		case KeyboardKeyCode::eB: return ImGuiKey_B;
		case KeyboardKeyCode::eC: return ImGuiKey_C;
		case KeyboardKeyCode::eD: return ImGuiKey_D;
		case KeyboardKeyCode::eE: return ImGuiKey_E;
		case KeyboardKeyCode::eF: return ImGuiKey_F;
		case KeyboardKeyCode::eG: return ImGuiKey_G;
		case KeyboardKeyCode::eH: return ImGuiKey_H;
		case KeyboardKeyCode::eI: return ImGuiKey_I;
		case KeyboardKeyCode::eJ: return ImGuiKey_J;
		case KeyboardKeyCode::eK: return ImGuiKey_K;
		case KeyboardKeyCode::eL: return ImGuiKey_L;
		case KeyboardKeyCode::eM: return ImGuiKey_M;
		case KeyboardKeyCode::eN: return ImGuiKey_N;
		case KeyboardKeyCode::eO: return ImGuiKey_O;
		case KeyboardKeyCode::eP: return ImGuiKey_P;
		case KeyboardKeyCode::eQ: return ImGuiKey_Q;
		case KeyboardKeyCode::eR: return ImGuiKey_R;
		case KeyboardKeyCode::eS: return ImGuiKey_S;
		case KeyboardKeyCode::eT: return ImGuiKey_T;
		case KeyboardKeyCode::eU: return ImGuiKey_U;
		case KeyboardKeyCode::eV: return ImGuiKey_V;
		case KeyboardKeyCode::eW: return ImGuiKey_W;
		case KeyboardKeyCode::eX: return ImGuiKey_X;
		case KeyboardKeyCode::eY: return ImGuiKey_Y;
		case KeyboardKeyCode::eZ: return ImGuiKey_Z;
		case KeyboardKeyCode::eLeftBracket: return ImGuiKey_LeftBracket;
		case KeyboardKeyCode::eBackslash: return ImGuiKey_Backslash;
		case KeyboardKeyCode::eRightBracket: return ImGuiKey_RightBracket;
		case KeyboardKeyCode::eGraveAccent: return ImGuiKey_GraveAccent;
		case KeyboardKeyCode::eEsc: return ImGuiKey_Escape;
		case KeyboardKeyCode::eEnter: return ImGuiKey_Enter;
		case KeyboardKeyCode::eTab: return ImGuiKey_Tab;
		case KeyboardKeyCode::eBackspace: return ImGuiKey_Backspace;
		case KeyboardKeyCode::eInsert: return ImGuiKey_Insert;
		case KeyboardKeyCode::eDel: return ImGuiKey_Delete;
		case KeyboardKeyCode::eRight: return ImGuiKey_RightArrow;
		case KeyboardKeyCode::eLeft: return ImGuiKey_LeftArrow;
		case KeyboardKeyCode::eDown: return ImGuiKey_DownArrow;
		case KeyboardKeyCode::eUp: return ImGuiKey_UpArrow;
		case KeyboardKeyCode::ePageUp: return ImGuiKey_PageUp;
		case KeyboardKeyCode::ePageDown: return ImGuiKey_PageDown;
		case KeyboardKeyCode::eHome: return ImGuiKey_Home;
		case KeyboardKeyCode::eEnd: return ImGuiKey_End;
		case KeyboardKeyCode::eCapsLock: return ImGuiKey_CapsLock;
		case KeyboardKeyCode::eScrollLock: return ImGuiKey_ScrollLock;
		case KeyboardKeyCode::eNumLock: return ImGuiKey_NumLock;
		case KeyboardKeyCode::ePrintScreen: return ImGuiKey_PrintScreen;
		case KeyboardKeyCode::ePause: return ImGuiKey_Pause;
		case KeyboardKeyCode::eF1: return ImGuiKey_F1;
		case KeyboardKeyCode::eF2: return ImGuiKey_F2;
		case KeyboardKeyCode::eF3: return ImGuiKey_F3;
		case KeyboardKeyCode::eF4: return ImGuiKey_F4;
		case KeyboardKeyCode::eF5: return ImGuiKey_F5;
		case KeyboardKeyCode::eF6: return ImGuiKey_F6;
		case KeyboardKeyCode::eF7: return ImGuiKey_F7;
		case KeyboardKeyCode::eF8: return ImGuiKey_F8;
		case KeyboardKeyCode::eF9: return ImGuiKey_F9;
		case KeyboardKeyCode::eF10: return ImGuiKey_F10;
		case KeyboardKeyCode::eF11: return ImGuiKey_F11;
		case KeyboardKeyCode::eF12: return ImGuiKey_F12;
		case KeyboardKeyCode::eF13: return ImGuiKey_F13;
		case KeyboardKeyCode::eF14: return ImGuiKey_F14;
		case KeyboardKeyCode::eF15: return ImGuiKey_F15;
		case KeyboardKeyCode::eF16: return ImGuiKey_F16;
		case KeyboardKeyCode::eF17: return ImGuiKey_F17;
		case KeyboardKeyCode::eF18: return ImGuiKey_F18;
		case KeyboardKeyCode::eF19: return ImGuiKey_F19;
		case KeyboardKeyCode::eF20: return ImGuiKey_F20;
		case KeyboardKeyCode::eF21: return ImGuiKey_F21;
		case KeyboardKeyCode::eF22: return ImGuiKey_F22;
		case KeyboardKeyCode::eF23: return ImGuiKey_F23;
		case KeyboardKeyCode::eF24: return ImGuiKey_F24;
		case KeyboardKeyCode::eKp0: return ImGuiKey_Keypad0;
		case KeyboardKeyCode::eKp1: return ImGuiKey_Keypad1;
		case KeyboardKeyCode::eKp2: return ImGuiKey_Keypad2;
		case KeyboardKeyCode::eKp3: return ImGuiKey_Keypad3;
		case KeyboardKeyCode::eKp4: return ImGuiKey_Keypad4;
		case KeyboardKeyCode::eKp5: return ImGuiKey_Keypad5;
		case KeyboardKeyCode::eKp6: return ImGuiKey_Keypad6;
		case KeyboardKeyCode::eKp7: return ImGuiKey_Keypad7;
		case KeyboardKeyCode::eKp8: return ImGuiKey_Keypad8;
		case KeyboardKeyCode::eKp9: return ImGuiKey_Keypad9;
		case KeyboardKeyCode::eKpDec: return ImGuiKey_KeypadDecimal;
		case KeyboardKeyCode::eKpDiv: return ImGuiKey_KeypadDivide;
		case KeyboardKeyCode::eKpMul: return ImGuiKey_KeypadMultiply;
		case KeyboardKeyCode::eKpSub: return ImGuiKey_KeypadSubtract;
		case KeyboardKeyCode::eKpAdd: return ImGuiKey_KeypadAdd;
		case KeyboardKeyCode::eKpEnter: return ImGuiKey_KeypadEnter;
		case KeyboardKeyCode::eKpEq: return ImGuiKey_KeypadEqual;
		case KeyboardKeyCode::eLeftShift: return ImGuiKey_LeftShift;
		case KeyboardKeyCode::eLeftControl: return ImGuiKey_LeftCtrl;
		case KeyboardKeyCode::eLeftAlt: return ImGuiKey_LeftAlt;
		case KeyboardKeyCode::eLeftSuper: return ImGuiKey_LeftSuper;
		case KeyboardKeyCode::eRightShift: return ImGuiKey_RightShift;
		case KeyboardKeyCode::eRightControl: return ImGuiKey_RightCtrl;
		case KeyboardKeyCode::eRightAlt: return ImGuiKey_RightAlt;
		case KeyboardKeyCode::eRightSuper: return ImGuiKey_RightSuper;
		case KeyboardKeyCode::eMenu: return ImGuiKey_Menu;
		default: return ImGuiKey_None;
		}
	}

	inline constexpr auto translate_mouse_code(MouseKeyCode code) -> ImGuiMouseButton {
		switch (code)
		{
		case edge::MouseKeyCode::eMouseLeft: return ImGuiMouseButton_Left;
		case edge::MouseKeyCode::eMouseRight: return ImGuiMouseButton_Right;
		case edge::MouseKeyCode::eMouseMiddle: return ImGuiMouseButton_Middle;
		default: return ImGuiMouseButton_COUNT;
		}
	}

	auto ImGuiLayer::create(platform::IPlatformContext& context, gfx::Renderer& renderer, gfx::ResourceUploader& uploader, gfx::ResourceUpdater& updater) -> Owned<ImGuiLayer> {
		Owned<ImGuiLayer> self = std::make_unique<ImGuiLayer>();
		self->dispatcher_ = &context.get_event_dispatcher();
		self->renderer_ = &renderer;
		self->resource_uploader_ = &uploader;
		self->resource_updater_ = &updater;

		ImGui::SetAllocatorFunctions(
			[](size_t size, void* user_data) -> void* {
				return mi_malloc(size);
			},
			[](void* ptr, void* user_data) -> void {
				mi_free(ptr);
			}, nullptr);

		return self;
	}

	auto ImGuiLayer::attach() -> void {
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		IMGUI_CHECKVERSION();
		IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

		io.BackendRendererUserData = (void*)this;
		io.BackendRendererName = "edge";
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
		io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;  // We can honor ImGuiPlatformIO::Textures[] requests during render.

		io.Fonts->Build();

		uint8_t* font_pixels{ nullptr };
		int font_width, font_height;

		io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);

		// Create vertex buffer
		vertex_buffer_id_ = renderer_->create_render_resource();
		create_or_update_vertex_buffer(kInitialVertexCount);

		// Create index buffer
		index_buffer_id_ = renderer_->create_render_resource();
		create_or_update_index_buffer(kInitialIndexCount);

		// Register event listener
		listener_id_ = dispatcher_->add_listener<events::EventTag::eWindow, events::EventTag::eRawInput>(
			[](const events::Dispatcher::event_variant_t& e, void* user_ptr) {
				auto* self = static_cast<ImGuiLayer*>(user_ptr);

				std::visit([self](const auto& e) {
					ImGuiIO& io = ImGui::GetIO();
					using EventType = std::decay_t<decltype(e)>;
					if constexpr (std::same_as<EventType, events::KeyEvent>) {
						io.AddKeyEvent(translate_key_code(e.key_code), e.state);
					}
					else if constexpr (std::same_as<EventType, events::MousePositionEvent>) {
						io.AddMousePosEvent(static_cast<float>(e.x), static_cast<float>(e.y));
					}
					else if constexpr (std::same_as<EventType, events::MouseKeyEvent>) {
						io.AddMouseButtonEvent(translate_mouse_code(e.key_code), e.state);
					}
					else if constexpr (std::same_as<EventType, events::MouseScrollEvent>) {
						io.AddMouseWheelEvent(static_cast<float>(e.offset_x), static_cast<float>(e.offset_y));
					}
					else if constexpr (std::same_as<EventType, events::CharacterInputEvent>) {
						io.AddInputCharacter(e.charcode);
					}
					else if constexpr (std::same_as<EventType, events::WindowFocusChangedEvent>) {
						io.AddFocusEvent(e.focused);
					}
					}, e);

			}, this);
	}

	auto ImGuiLayer::detach() -> void {
		ImGui::EndFrame();
		ImGui::DestroyContext();

		dispatcher_->remove_listener(listener_id_);
	}

	auto ImGuiLayer::update(float delta_time) -> void {
		renderer_->add_shader_pass({
			.name = u8"ImGui",
			.pipeline_name = "imgui",
			.setup_cb = [this](gfx::Renderer& pass, gfx::CommandBuffer const& cmd) -> void {
				auto backbuffer_id = pass.get_backbuffer_resource_id();
				auto& backbuffer = pass.get_render_resource(backbuffer_id);
				auto& backbuffer_image = backbuffer.get_handle<gfx::Image>();
				auto extent = backbuffer_image.get_extent();

				pass.add_color_attachment(backbuffer_id);
				pass.set_render_area({ {}, {extent.width, extent.height } });

				auto& vertex_buffer_resource = pass.get_render_resource(vertex_buffer_id_);
				vertex_buffer_resource.transfer_state(cmd, gfx::ResourceStateFlag::eVertexRead);
				auto& index_buffer_resource = pass.get_render_resource(index_buffer_id_);
				index_buffer_resource.transfer_state(cmd, gfx::ResourceStateFlag::eIndexRead);

				if (!ImGui::GetCurrentContext()) {
					return;
				}

				ImGui::Render();

				ImDrawData* draw_data = ImGui::GetDrawData();
				if (!draw_data || draw_data->TotalVtxCount == 0 || draw_data->TotalIdxCount == 0) {
					return;
				}

				if (draw_data->Textures != nullptr) {
					for (ImTextureData* tex : *draw_data->Textures) {
						if (tex->Status == ImTextureStatus_OK) {
							continue;
						}

						if (tex->Status == ImTextureStatus_WantCreate) {
							gfx::ImageImportInfo import_info{};
							import_info.raw.width = tex->Width;
							import_info.raw.height = tex->Height;
							import_info.raw.data.resize(tex->Width * tex->Height * tex->BytesPerPixel);
							std::memcpy(import_info.raw.data.data(), tex->Pixels, import_info.raw.data.size());
							auto uploading_task_id = resource_uploader_->load_image(std::move(import_info));
							resource_uploader_->wait_for_task(uploading_task_id);

							auto new_texture = pass.create_render_resource();

							auto uploading_result = resource_uploader_->get_task_result(uploading_task_id);
							if (uploading_result) {
								renderer_->setup_render_resource(new_texture, std::move(std::get<gfx::Image>(uploading_result->data)), uploading_result->state);
							}

							// TODO: add internal tracking of resource uploads
							tex->SetTexID((ImTextureID)new_texture);
							tex->SetStatus(ImTextureStatus_OK);
						}
						else if (tex->Status == ImTextureStatus_WantUpdates) {
							auto resource_id = tex->GetTexID();
							auto& render_resource = pass.get_render_resource(resource_id);
							auto& image = render_resource.get_handle<gfx::Image>();

							auto pitch = tex->UpdateRect.w * tex->BytesPerPixel;

							// Create staging memory for update data
							gfx::BufferCreateInfo buffer_create_info{};
							buffer_create_info.size = pitch * tex->UpdateRect.h;
							buffer_create_info.count = 1u;
							buffer_create_info.minimal_alignment = 4u;
							buffer_create_info.flags = gfx::kStagingBuffer;
							auto buffer_create_result = gfx::Buffer::create(buffer_create_info);
							GFX_ASSERT_MSG(buffer_create_result.has_value(), "Failed to create staging buffer for texture update.");
							auto staging_buffer = std::move(buffer_create_result.value());

							auto mapping_result = staging_buffer.map();
							GFX_ASSERT_MSG(mapping_result.has_value(), "Failed to map staging memory.");
							auto mapped_range = std::move(mapping_result.value());

							for (int y = 0; y < tex->UpdateRect.h; y++) {
								std::memcpy(mapped_range.data() + pitch * y, tex->GetPixelsAt(tex->UpdateRect.x, tex->UpdateRect.y + y), pitch);
							}

							render_resource.transfer_state(cmd, gfx::ResourceStateFlag::eCopyDst);

							mi::Vector<vk::BufferImageCopy2KHR> copy_regions{};
							for (auto& update_region : tex->Updates) {
								vk::BufferImageCopy2KHR image_copy_region{};
								image_copy_region.bufferOffset; // TODO: setup buffer offset
								image_copy_region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
								image_copy_region.imageSubresource.mipLevel = 0u;
								image_copy_region.imageSubresource.baseArrayLayer = 0u;
								image_copy_region.imageSubresource.layerCount = 1u;
								image_copy_region.imageOffset = vk::Offset3D{ update_region.x, update_region.y, 0 };
								image_copy_region.imageExtent = vk::Extent3D{ update_region.w, update_region.h, 1u };
								copy_regions.push_back(image_copy_region);
							}

							vk::CopyBufferToImageInfo2KHR copy_info{};
							//copy_info.srcBuffer = upload_info.memory_range.get_buffer();
							copy_info.dstImage = image.get_handle();
							copy_info.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
							copy_info.regionCount = static_cast<uint32_t>(copy_regions.size());
							copy_info.pRegions = copy_regions.data();
							cmd->copyBufferToImage2KHR(&copy_info);

							render_resource.transfer_state(cmd, gfx::ResourceStateFlag::eShaderResource);

							// TODO: Uploader is not support partial update yet, because of that trying to update manually
						}
						else if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames >= 256) {

						}
					}
				}

				},
			.execute_cb = [this](gfx::Renderer& pass, gfx::CommandBuffer const& cmd, float delta_time) -> void {
				if (!ImGui::GetCurrentContext()) {
					return;
				}

				ImDrawData* draw_data = ImGui::GetDrawData();
				if (!draw_data || draw_data->TotalVtxCount == 0 || draw_data->TotalIdxCount == 0) {
					return;
				}
				

				if (draw_data->TotalVtxCount > static_cast<int>(current_vertex_capacity_)) {
					EDGE_LOGW("ImGui vertex buffer too small ({} < {}), need to resize", current_vertex_capacity_, draw_data->TotalVtxCount);

					uint32_t new_size = current_index_capacity_;
					while (new_size < draw_data->TotalVtxCount) {
						new_size *= 2u;
					}

					create_or_update_vertex_buffer(new_size);
				}

				if (draw_data->TotalIdxCount > static_cast<int>(current_index_capacity_)) {
					EDGE_LOGW("ImGui index buffer too small ({} < {}), need to resize", current_index_capacity_, draw_data->TotalIdxCount);
					
					uint32_t new_size = current_index_capacity_;
					while (new_size < draw_data->TotalIdxCount) {
						new_size *= 2u;
					}

					create_or_update_index_buffer(new_size);
				}

				auto& vertex_buffer_resource = pass.get_render_resource(vertex_buffer_id_);
				auto& vertex_buffer = vertex_buffer_resource.get_handle<gfx::Buffer>();
				auto mapped_vertex_range = vertex_buffer.map().value();

				auto& index_buffer_resource = pass.get_render_resource(index_buffer_id_);
				auto& index_buffer = index_buffer_resource.get_handle<gfx::Buffer>();
				auto mapped_index_range = index_buffer.map().value();

				gfx::imgui::PushConstant push_constant{};
				push_constant.vertices = vertex_buffer.get_device_address();
				push_constant.scale.x = 2.f / draw_data->DisplaySize.x;
				push_constant.scale.y = 2.f / draw_data->DisplaySize.y;
				push_constant.translate.x = -1.0f - draw_data->DisplayPos.x * push_constant.scale.x;
				push_constant.translate.y = -1.0f - draw_data->DisplayPos.y * push_constant.scale.y;
				push_constant.sampler_index = 0u;
				// TODO: Push constants will be needed in any time when i update image binding id
				//auto constant_range_ptr = reinterpret_cast<const uint8_t*>(&push_constant);
				//pass.push_constant_range(cmd, vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute, { constant_range_ptr, sizeof(push_constant) });

				cmd->bindIndexBuffer(index_buffer.get_handle(), 0ull, sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32);

				ImGuiIO& io = ImGui::GetIO();
				vk::Viewport viewport{ 0.f, 0.f, io.DisplaySize.x, io.DisplaySize.y, 0.f, 1.f };
				cmd->setViewport(0u, 1u, &viewport);

				// Will project scissor/clipping rectangles into framebuffer space
				ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
				ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

				int32_t global_vtx_offset = 0;
				int32_t global_idx_offset = 0;

				for (int32_t n = 0; n < draw_data->CmdListsCount; n++) {
					const ImDrawList* im_cmd_list = draw_data->CmdLists[n];

					auto vertex_byte_size = im_cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
					std::memcpy(mapped_vertex_range.data() + global_vtx_offset,
						im_cmd_list->VtxBuffer.Data, vertex_byte_size);

					// FIX: Was using vertex_byte_size instead of index_byte_size
					auto index_byte_size = im_cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
					std::memcpy(mapped_index_range.data() + global_idx_offset,
						im_cmd_list->IdxBuffer.Data, index_byte_size);

					global_vtx_offset += vertex_byte_size;
					global_idx_offset += index_byte_size;
				}

				// Reset offsets for drawing
				global_vtx_offset = 0;
				global_idx_offset = 0;

				uint32_t last_image_index = ~0u;

				for (int32_t n = 0; n < draw_data->CmdListsCount; n++) {
					const ImDrawList* im_cmd_list = draw_data->CmdLists[n];
					for (int32_t cmd_i = 0; cmd_i < im_cmd_list->CmdBuffer.Size; cmd_i++) {
						const ImDrawCmd* pcmd = &im_cmd_list->CmdBuffer[cmd_i];
						
						// Project scissor/clipping rectangles into framebuffer space
						ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
						ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

						// Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
						if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
						if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
						if (clip_max.x > io.DisplaySize.x) { clip_max.x = io.DisplaySize.x; }
						if (clip_max.y > io.DisplaySize.y) { clip_max.y = io.DisplaySize.y; }
						if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
							continue;

						// Apply scissor/clipping rectangle
						vk::Rect2D scissor_rect{ { (int32_t)(clip_min.x), (int32_t)(clip_min.y) }, { (uint32_t)(clip_max.x - clip_min.x), (uint32_t)(clip_max.y - clip_min.y) } };
						cmd->setScissor(0u, 1u, &scissor_rect);
						
						uint32_t new_image_index = pcmd->GetTexID();
						if (new_image_index != last_image_index) {
							auto& render_resource = pass.get_render_resource(new_image_index);
							push_constant.image_index = render_resource.get_srv_index();
							auto constant_range_ptr = reinterpret_cast<const uint8_t*>(&push_constant);
							pass.push_constant_range(cmd,
								vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute,
								{ constant_range_ptr, sizeof(push_constant) });
							last_image_index = new_image_index;
						}

						cmd->drawIndexed(pcmd->ElemCount, 1u, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0u);
					}

					global_idx_offset += im_cmd_list->IdxBuffer.Size;
					global_vtx_offset += im_cmd_list->VtxBuffer.Size;
				}
				}
			});

		auto& backbuffer = renderer_->get_backbuffer_resource();
		auto& backbuffer_image = backbuffer.get_handle<gfx::Image>();
		auto backbuffer_extent = backbuffer_image.get_extent();

		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2(static_cast<float>(backbuffer_extent.width), static_cast<float>(backbuffer_extent.height));
		io.DeltaTime = delta_time;
		
		ImGui::NewFrame();
		ImGui::ShowDemoWindow();
	}

	auto ImGuiLayer::fixed_update(float delta_time) -> void {

	}

	auto ImGuiLayer::create_or_update_vertex_buffer(uint32_t requested_capacity) -> void {
		gfx::BufferCreateInfo buffer_create_info{};
		buffer_create_info.size = sizeof(ImDrawVert);
		buffer_create_info.count = requested_capacity;
		buffer_create_info.flags = gfx::kDynamicVertexBuffer;
		buffer_create_info.minimal_alignment = buffer_create_info.size;
		auto buffer_create_result = gfx::Buffer::create(buffer_create_info);
		GFX_ASSERT_MSG(buffer_create_result.has_value(), "Failed to create vertex buffer.");
		auto& render_resource = renderer_->get_render_resource(vertex_buffer_id_);
		render_resource.update(std::move(buffer_create_result.value()), gfx::ResourceStateFlag::eUndefined);

		current_vertex_capacity_ = requested_capacity;
	}

	auto ImGuiLayer::create_or_update_index_buffer(uint32_t requested_capacity) -> void {
		gfx::BufferCreateInfo buffer_create_info{};
		buffer_create_info.size = sizeof(ImDrawIdx);
		buffer_create_info.count = requested_capacity;
		buffer_create_info.flags = gfx::kDynamicIndexBuffer;
		buffer_create_info.minimal_alignment = buffer_create_info.size;
		auto buffer_create_result = gfx::Buffer::create(buffer_create_info);
		GFX_ASSERT_MSG(buffer_create_result.has_value(), "Failed to create index buffer.");
		auto& render_resource = renderer_->get_render_resource(index_buffer_id_);
		render_resource.update(std::move(buffer_create_result.value()), gfx::ResourceStateFlag::eUndefined);

		current_index_capacity_ = requested_capacity;
	}
}

#undef EDGE_LOGGER_SCOPE