#pragma once

#include "native/core/types.hpp"
#include "native/render/gl_loader.hpp"

#include <array>
#include <cstdint>
#include <string>

struct GLFWwindow;

namespace stem::render {

struct GlContextInfo {
    std::string vendor;
    std::string renderer;
    std::string version;
    std::string glsl_version;
    int major = 0;
    int minor = 0;
    std::string loader_version;
    bool visible = false;
};

class GlContext {
public:
    GlContext() = default;
    ~GlContext();

    GlContext(const GlContext&) = delete;
    GlContext& operator=(const GlContext&) = delete;
    GlContext(GlContext&&) = delete;
    GlContext& operator=(GlContext&&) = delete;

    [[nodiscard]] Result<GlContextInfo> initialize(
        int width,
        int height,
        bool visible,
        const char* title = "STEM Engine Native Preview"
    );

    void make_current() noexcept;
    void set_vsync(bool enabled) noexcept;
    void swap_buffers() noexcept;
    void poll_events() noexcept;
    void set_window_title(const char* title) noexcept;
    [[nodiscard]] bool should_close() const noexcept;
    [[nodiscard]] bool is_key_escape_pressed() const noexcept;
    [[nodiscard]] bool is_key_down(int glfw_key) const noexcept;
    [[nodiscard]] bool consume_key_press(int glfw_key) noexcept;
    void get_framebuffer_size(int& out_w, int& out_h) const noexcept;

    [[nodiscard]] const gl::GlDispatch& gl() const noexcept { return gl_; }
    [[nodiscard]] const GlContextInfo& info() const noexcept { return info_; }
    [[nodiscard]] GLFWwindow* window() const noexcept { return window_; }

    void shutdown() noexcept;

private:
    GLFWwindow* window_ = nullptr;
    gl::GlDispatch gl_{};
    GlContextInfo info_{};
    std::array<uint8_t, 512> prev_key_down_{};
};

}  // namespace stem::render
