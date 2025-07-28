#include "platform.h"

namespace edge::platform {
    auto AndroidPlatformWindow::construct(const window::Properties& properties) -> std::unique_ptr<AndroidPlatformWindow> {
        auto window = std::make_unique<AndroidPlatformWindow>();
        window->_construct(properties);
        return window;
    }

    auto AndroidPlatformWindow::create() -> bool {
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

    auto AndroidPlatformWindow::set_title(std::string_view title) -> void {
        properties_.title = title;
    }

    auto AndroidPlatformWindow::get_title() const->std::string_view {
        return properties_.title;
    }

    auto AndroidPlatformWindow::_construct(const window::Properties& properties) -> bool {
        properties_ = properties;
        return true;
    }
}