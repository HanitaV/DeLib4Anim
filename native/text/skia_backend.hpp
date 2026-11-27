#pragma once

#include "native/core/types.hpp"

#include <cstdint>

namespace stem::text {

// Optional Skia Ganesh/OpenGL bridge (compiled when -DSTEM_ENABLE_SKIA=ON with MSVC ABI skia.lib).
// On MinGW-W64 toolchains without MSVC Skia ABI compatibility, native FontAtlasCache + SVG Stencil-then-Cover
// handles glyphs and vector paths natively on OpenGL 4.5.
class SkiaGlBridge {
public:
    SkiaGlBridge() = default;
    ~SkiaGlBridge();

    [[nodiscard]] static constexpr bool is_compiled_in() noexcept {
#ifdef STEM_HAS_SKIA
        return true;
#else
        return false;
#endif
    }

    [[nodiscard]] Result<void> attach_to_current_gl(int width, int height, uint32_t target_fbo);
    void reset_gl_state_before_skia() noexcept;
    void flush_and_submit() noexcept;
    void shutdown() noexcept;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace stem::text
