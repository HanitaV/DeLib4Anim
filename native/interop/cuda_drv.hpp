#pragma once

#include "native/core/types.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Include official FFmpeg nv-codec-headers dynamic CUDA Driver API loader before hwcontext_cuda.h.
#include <ffnvcodec/dynlink_loader.h>

namespace stem::interop {

// Extra CUDA Driver API symbols not included in ffnvcodec's CudaFunctions table.
using PFN_cuEventElapsedTime = CUresult(CUDAAPI*)(float* pMilliseconds, CUevent hStart, CUevent hEnd);
using PFN_cuMemGetInfo_v2 = CUresult(CUDAAPI*)(size_t* free, size_t* total);
using PFN_cuCtxSetCurrent = CUresult(CUDAAPI*)(CUcontext ctx);

struct CudaDeviceInfo {
    int device_index = 0;
    std::string name;
    int driver_version = 0;
    int cc_major = 0;
    int cc_minor = 0;
    size_t free_vram_bytes = 0;
    size_t total_vram_bytes = 0;
    bool gl_interop_colocated = false;
};

struct RegisteredGlTexture {
    uint32_t gl_tex_id = 0;
    int width = 0;
    int height = 0;
    CUgraphicsResource resource = nullptr;
    CUtexObject active_tex_obj = 0;
    CUevent conversion_start = nullptr;
    CUevent conversion_done = nullptr;
    bool timing_pending = false;
};

class CudaInteropContext {
public:
    CudaInteropContext() = default;
    ~CudaInteropContext();

    CudaInteropContext(const CudaInteropContext&) = delete;
    CudaInteropContext& operator=(const CudaInteropContext&) = delete;
    CudaInteropContext(CudaInteropContext&&) = delete;
    CudaInteropContext& operator=(CudaInteropContext&&) = delete;

    // Initializes CUDA Driver API on the exact NVIDIA GPU backing the active OpenGL context.
    [[nodiscard]] Result<CudaDeviceInfo> initialize_for_current_gl_context();

    // Standalone probe (can run with or without active GL context for doctor diagnostics).
    [[nodiscard]] static Result<CudaDeviceInfo> probe_driver(bool require_gl_colocation);

    [[nodiscard]] CUcontext cu_context() const noexcept { return cu_ctx_; }
    [[nodiscard]] CUstream cu_stream() const noexcept { return cu_stream_; }
    [[nodiscard]] CUdevice cu_device() const noexcept { return cu_dev_; }
    [[nodiscard]] CudaFunctions* funcs() const noexcept { return cu_fns_; }
    [[nodiscard]] const CudaDeviceInfo& device_info() const noexcept { return info_; }

    [[nodiscard]] Result<void> register_gl_texture(
        uint32_t gl_tex_id,
        int width,
        int height,
        RegisteredGlTexture& out_slot
    );
    void unregister_gl_texture(RegisteredGlTexture& slot) noexcept;

    // Maps GL texture -> creates read-only integer CUtexObject -> launches BT.709 NV12 kernel -> unmaps.
    // Records async CUDA events without blocking host CPU; returns completed GPU ms from previous cycle of this slot.
    [[nodiscard]] Result<float> convert_gl_texture_to_nv12(
        RegisteredGlTexture& slot,
        CUdeviceptr dst_y,
        int pitch_y,
        CUdeviceptr dst_uv,
        int pitch_uv,
        bool flip_y,
        ColorRGBA bg_color,
        bool record_timing
    );

    struct BatchNv12Target {
        RegisteredGlTexture* slot = nullptr;
        CUdeviceptr dst_y = 0;
        int pitch_y = 0;
        CUdeviceptr dst_uv = 0;
        int pitch_uv = 0;
        bool flip_y = true;
        ColorRGBA bg_color{};
    };

    // Batch-amortized GL-CUDA interop: maps all N textures in ONE cuGraphicsMapResources call,
    // launches N conversion kernels on cu_stream_, and unmaps all N in ONE cuGraphicsUnmapResources call.
    [[nodiscard]] Result<void> convert_gl_textures_batch_to_nv12(
        std::span<const BatchNv12Target> items,
        bool record_timing,
        std::vector<float>& out_completed_ms
    );

    [[nodiscard]] Result<void> synchronize_stream();
    [[nodiscard]] Result<void> download_nv12_for_verification(
        CUdeviceptr src_y,
        int pitch_y,
        CUdeviceptr src_uv,
        int pitch_uv,
        int width,
        int height,
        std::vector<uint8_t>& out_y,
        std::vector<uint8_t>& out_uv
    );

    [[nodiscard]] Result<std::pair<size_t, size_t>> query_vram_bytes() const;
    [[nodiscard]] std::string format_cu_error(CUresult res, std::string_view where) const;

    void shutdown() noexcept;

private:
    CudaFunctions* cu_fns_ = nullptr;
    void* nvcuda_module_ = nullptr;
    PFN_cuEventElapsedTime fn_event_elapsed_time_ = nullptr;
    PFN_cuMemGetInfo_v2 fn_mem_get_info_ = nullptr;
    PFN_cuCtxSetCurrent fn_ctx_set_current_ = nullptr;

    CUdevice cu_dev_ = 0;
    CUcontext cu_ctx_ = nullptr;
    bool retained_primary_ctx_ = false;
    CUstream cu_stream_ = nullptr;
    CUmodule cu_module_ = nullptr;
    CUfunction fn_rgba_to_nv12_ = nullptr;
    CudaDeviceInfo info_{};
};

}  // namespace stem::interop
