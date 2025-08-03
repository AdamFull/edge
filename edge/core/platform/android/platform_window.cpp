#include "platform.h"

namespace edge::platform {
    auto AndroidPlatformWindow::construct(const window::Properties& properties) -> std::unique_ptr<AndroidPlatformWindow> {
        auto window = std::make_unique<AndroidPlatformWindow>();
        window->_construct(properties);
        return window;
    }

    auto AndroidPlatformWindow::create(const window::Properties& props) -> bool {
        return true;
    }

    auto AndroidPlatformWindow::destroy() -> void {

    }

    auto AndroidPlatformWindow::show() -> void {

    }

    auto AndroidPlatformWindow::hide() -> void {

    }

    auto AndroidPlatformWindow::is_visible() const -> bool {
        return true;
    }

    auto AndroidPlatformWindow::poll_events() -> void {

    }

    auto AndroidPlatformWindow::get_dpi_factor() const noexcept -> float {
        return 1.0f;
    }

    auto AndroidPlatformWindow::get_content_scale_factor() const noexcept -> float {
        return 1.0f;
    }

    auto AndroidPlatformWindow::set_title(std::string_view title) -> void {
        properties_.title = title;
    }

    auto AndroidPlatformWindow::_construct(const window::Properties& properties) -> bool {
        properties_ = properties;
        return true;
    }
}