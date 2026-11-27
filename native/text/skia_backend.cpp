#include "native/text/skia_backend.hpp"

#ifdef STEM_HAS_SKIA
#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLTypes.h"
#ifdef _WIN32
#include "include/gpu/ganesh/gl/win/GrGLMakeWinInterface.h"
#endif
#endif

namespace stem::text {

struct SkiaGlBridge::Impl {
#ifdef STEM_HAS_SKIA
    sk_sp<GrDirectContext> gr_ctx;
    sk_sp<SkSurface> surface;
#endif
    int width = 0;
    int height = 0;
    uint32_t fbo = 0;
};

SkiaGlBridge::~SkiaGlBridge() {
    shutdown();
}

Result<void> SkiaGlBridge::attach_to_current_gl(int width, int height, uint32_t target_fbo) {
#ifdef STEM_HAS_SKIA
    shutdown();
#ifdef _WIN32
    auto iface = GrGLInterfaces::MakeWin();
#else
    auto iface = GrGLMakeNativeInterface();
#endif
    if (!iface) {
        return fail("GrGLInterfaces::MakeWin / GrGLMakeNativeInterface failed");
    }
    auto ctx = GrDirectContexts::MakeGL(iface);
    if (!ctx) {
        return fail("GrDirectContexts::MakeGL failed");
    }
    GrGLFramebufferInfo fb_info{};
    fb_info.fFBOID = target_fbo;
    fb_info.fFormat = 0x8058;  // GL_RGBA8
    auto backend_rt = GrBackendRenderTargets::MakeGL(width, height, 0, 8, fb_info);
    auto surf = SkSurfaces::WrapBackendRenderTarget(
        ctx.get(),
        backend_rt,
        kBottomLeft_GrSurfaceOrigin,
        kRGBA_8888_SkColorType,
        nullptr,
        nullptr
    );
    if (!surf) {
        return fail("SkSurfaces::WrapBackendRenderTarget failed");
    }
    impl_ = new Impl{std::move(ctx), std::move(surf), width, height, target_fbo};
    return {};
#else
    (void)width;
    (void)height;
    (void)target_fbo;
    return fail("Skia Ganesh backend not enabled in this build (compile with MSVC + STEM_ENABLE_SKIA=ON)");
#endif
}

void SkiaGlBridge::reset_gl_state_before_skia() noexcept {
#ifdef STEM_HAS_SKIA
    if (impl_ && impl_->gr_ctx) {
        impl_->gr_ctx->resetContext(kAll_GrBackendState);
    }
#endif
}

void SkiaGlBridge::flush_and_submit() noexcept {
#ifdef STEM_HAS_SKIA
    if (impl_ && impl_->gr_ctx) {
        impl_->gr_ctx->flushAndSubmit(impl_->surface.get(), GrSyncCpu::kNo);
    }
#endif
}

void SkiaGlBridge::shutdown() noexcept {
    if (impl_) {
#ifdef STEM_HAS_SKIA
        impl_->surface.reset();
        if (impl_->gr_ctx) {
            impl_->gr_ctx->abandonContext();
            impl_->gr_ctx.reset();
        }
#endif
        delete impl_;
        impl_ = nullptr;
    }
}

}  // namespace stem::text
