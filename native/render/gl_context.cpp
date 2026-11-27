#include "native/render/gl_context.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>

namespace stem::render {

GlContext::~GlContext() {
    shutdown();
}

Result<GlContextInfo> GlContext::initialize(
    int width,
    int height,
    bool visible,
    const char* title
) {
    shutdown();
    prev_key_down_.fill(0);

#if defined(_WIN32)
    // On hybrid/Optimus systems (e.g. AMD iGPU + NVIDIA dGPU), python.exe lacks NvOptimusEnablement.
    // Setting SHIM_MCCOMPAT before opengl32.dll loads routes OpenGL onto the NVIDIA GPU.
    _putenv_s("SHIM_MCCOMPAT", "0x800000001");
    if (visible) {
        if (HMODULE user32 = GetModuleHandleA("user32.dll")) {
            using FnDpiCtx = BOOL(WINAPI*)(void*);
            if (auto fn = reinterpret_cast<FnDpiCtx>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"))) {
                // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == ((DPI_AWARENESS_CONTEXT)-4)
                fn(reinterpret_cast<void*>(static_cast<intptr_t>(-4)));
            }
        }
    }
#endif

    if (!glfwInit()) {
        const char* desc = nullptr;
        glfwGetError(&desc);
        return fail(std::format("glfwInit failed: {}", desc ? desc : "unknown"));
    }

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 0);  // Offscreen MSAA FBO owns sample count

    int win_w = 64;
    int win_h = 64;
    int pos_x = 0;
    int pos_y = 0;
    bool center_on_monitor = false;
    if (visible) {
        win_w = std::max(320, width);
        win_h = std::max(180, height);
        if (GLFWmonitor* mon = glfwGetPrimaryMonitor()) {
            int wx = 0, wy = 0, ww = 0, wh = 0;
            glfwGetMonitorWorkarea(mon, &wx, &wy, &ww, &wh);
            if (ww > 200 && wh > 200) {
                const double max_w = static_cast<double>(ww) * 0.90;
                const double max_h = static_cast<double>(wh) * 0.88;
                const double s = std::min(1.0, std::min(max_w / win_w, max_h / win_h));
                win_w = std::max(320, static_cast<int>(std::lround(win_w * s)));
                win_h = std::max(180, static_cast<int>(std::lround(win_h * s)));
                pos_x = wx + (ww - win_w) / 2;
                pos_y = wy + (wh - win_h) / 2;
                center_on_monitor = true;
            }
        }
    }

    window_ = glfwCreateWindow(win_w, win_h, title ? title : "STEM Engine", nullptr, nullptr);
    if (!window_) {
        const char* desc = nullptr;
        glfwGetError(&desc);
        glfwTerminate();
        return fail(std::format("glfwCreateWindow(4.5 Core) failed: {}", desc ? desc : "unknown"));
    }
    if (visible) {
        if (center_on_monitor) {
            glfwSetWindowPos(window_, pos_x, pos_y);
        }
        glfwShowWindow(window_);
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(visible ? 1 : 0);  // VSync only in preview; unthrottled in batch export

    auto loaded = gl::load_gl45([](const char* name) -> void* {
        return reinterpret_cast<void*>(glfwGetProcAddress(name));
    });
    if (!loaded) {
        shutdown();
        return fail(loaded.error());
    }
    gl_ = *loaded;

    auto str_or = [&](GLenum e) -> std::string {
        const GLubyte* s = gl_.GetString(e);
        return s ? reinterpret_cast<const char*>(s) : "";
    };
    GLint maj = 0;
    GLint min = 0;
    gl_.GetIntegerv(gl::kMajorVersion, &maj);
    gl_.GetIntegerv(gl::kMinorVersion, &min);

    info_ = GlContextInfo{
        .vendor = str_or(GL_VENDOR),
        .renderer = str_or(GL_RENDERER),
        .version = str_or(GL_VERSION),
        .glsl_version = str_or(gl::kShadingLanguageVersion),
        .major = maj,
        .minor = min,
        .loader_version = std::string(gl::kLoaderVersion),
        .visible = visible,
    };

    return info_;
}

void GlContext::make_current() noexcept {
    if (window_) {
        glfwMakeContextCurrent(window_);
    }
}

void GlContext::set_vsync(bool enabled) noexcept {
    if (window_) {
        glfwMakeContextCurrent(window_);
        glfwSwapInterval(enabled ? 1 : 0);
    }
}

void GlContext::swap_buffers() noexcept {
    if (window_) {
        glfwSwapBuffers(window_);
    }
}

void GlContext::poll_events() noexcept {
    glfwPollEvents();
}

void GlContext::set_window_title(const char* title) noexcept {
    if (window_ && title) {
        glfwSetWindowTitle(window_, title);
    }
}

bool GlContext::should_close() const noexcept {
    return !window_ || glfwWindowShouldClose(window_) != 0;
}

bool GlContext::is_key_escape_pressed() const noexcept {
    return window_ && glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS;
}

bool GlContext::is_key_down(int glfw_key) const noexcept {
    return window_ && glfwGetKey(window_, glfw_key) == GLFW_PRESS;
}

bool GlContext::consume_key_press(int glfw_key) noexcept {
    if (!window_ || glfw_key < 0 || static_cast<size_t>(glfw_key) >= prev_key_down_.size()) {
        return false;
    }
    const bool down = glfwGetKey(window_, glfw_key) == GLFW_PRESS;
    const bool was_down = prev_key_down_[static_cast<size_t>(glfw_key)] != 0;
    prev_key_down_[static_cast<size_t>(glfw_key)] = down ? 1u : 0u;
    return down && !was_down;
}

void GlContext::get_framebuffer_size(int& out_w, int& out_h) const noexcept {
    if (window_) {
        glfwGetFramebufferSize(window_, &out_w, &out_h);
    } else {
        out_w = 0;
        out_h = 0;
    }
}

void GlContext::shutdown() noexcept {
    if (window_) {
        glfwMakeContextCurrent(nullptr);
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

}  // namespace stem::render
