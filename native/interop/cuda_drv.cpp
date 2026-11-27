#include "native/interop/cuda_drv.hpp"
#include "color_convert_ptx.hpp"

#include <cstring>
#include <format>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace stem::interop {

namespace {

constexpr uint32_t kGlTexture2D = 0x0DE1u;  // GL_TEXTURE_2D

void* open_cuda_driver_lib() noexcept {
#if defined(_WIN32)
    return reinterpret_cast<void*>(LoadLibraryA("nvcuda.dll"));
#else
    return dlopen("libcuda.so.1", RTLD_LAZY);
#endif
}

void close_cuda_driver_lib(void* handle) noexcept {
    if (!handle) return;
#if defined(_WIN32)
    FreeLibrary(reinterpret_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

void* get_cuda_proc(void* handle, const char* name) noexcept {
    if (!handle) return nullptr;
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(handle), name));
#else
    return dlsym(handle, name);
#endif
}

}  // namespace

CudaInteropContext::~CudaInteropContext() {
    shutdown();
}

std::string CudaInteropContext::format_cu_error(CUresult res, std::string_view where) const {
    const char* err_name = "UNKNOWN";
    const char* err_str = "Unknown CUDA driver error";
    if (cu_fns_) {
        if (cu_fns_->cuGetErrorName) cu_fns_->cuGetErrorName(res, &err_name);
        if (cu_fns_->cuGetErrorString) cu_fns_->cuGetErrorString(res, &err_str);
    }
    return std::format("{} failed: {} ({}) [code={}]", where, err_name ? err_name : "?", err_str ? err_str : "?", static_cast<int>(res));
}

Result<CudaDeviceInfo> CudaInteropContext::probe_driver(bool require_gl_colocation) {
    CudaInteropContext tmp;
    if (require_gl_colocation) {
        return tmp.initialize_for_current_gl_context();
    }

    if (cuda_load_functions(&tmp.cu_fns_, nullptr) < 0 || !tmp.cu_fns_) {
        return fail("CUDA driver (nvcuda.dll / libcuda.so.1) not available");
    }
    tmp.nvcuda_module_ = open_cuda_driver_lib();
    tmp.fn_mem_get_info_ = reinterpret_cast<PFN_cuMemGetInfo_v2>(
        get_cuda_proc(tmp.nvcuda_module_, "cuMemGetInfo_v2")
    );
    tmp.fn_ctx_set_current_ = reinterpret_cast<PFN_cuCtxSetCurrent>(
        get_cuda_proc(tmp.nvcuda_module_, "cuCtxSetCurrent")
    );

    CUresult r = tmp.cu_fns_->cuInit(0);
    if (r != CUDA_SUCCESS) {
        return fail(tmp.format_cu_error(r, "cuInit"));
    }
    int count = 0;
    r = tmp.cu_fns_->cuDeviceGetCount(&count);
    if (r != CUDA_SUCCESS || count <= 0) {
        return fail("No CUDA-capable NVIDIA GPU detected");
    }

    CUdevice dev = 0;
    tmp.cu_fns_->cuDeviceGet(&dev, 0);
    char name[256] = {};
    tmp.cu_fns_->cuDeviceGetName(name, sizeof(name), dev);
    int drv_ver = 0;
    tmp.cu_fns_->cuDriverGetVersion(&drv_ver);
    int major = 0;
    int minor = 0;
    tmp.cu_fns_->cuDeviceComputeCapability(&major, &minor, dev);

    CudaDeviceInfo info{
        .device_index = 0,
        .name = name,
        .driver_version = drv_ver,
        .cc_major = major,
        .cc_minor = minor,
        .gl_interop_colocated = false,
    };

    CUcontext ctx = nullptr;
    if (tmp.cu_fns_->cuDevicePrimaryCtxRetain(&ctx, dev) == CUDA_SUCCESS) {
        if (tmp.fn_ctx_set_current_) tmp.fn_ctx_set_current_(ctx);
        if (tmp.fn_mem_get_info_) {
            tmp.fn_mem_get_info_(&info.free_vram_bytes, &info.total_vram_bytes);
        }
        tmp.cu_fns_->cuDevicePrimaryCtxRelease(dev);
    }
    return info;
}

Result<CudaDeviceInfo> CudaInteropContext::initialize_for_current_gl_context() {
    shutdown();

    if (cuda_load_functions(&cu_fns_, nullptr) < 0 || !cu_fns_) {
        return fail("Failed to load CUDA Driver API functions via ffnvcodec (nvcuda.dll missing)");
    }

    nvcuda_module_ = open_cuda_driver_lib();
    fn_event_elapsed_time_ = reinterpret_cast<PFN_cuEventElapsedTime>(
        get_cuda_proc(nvcuda_module_, "cuEventElapsedTime")
    );
    fn_mem_get_info_ = reinterpret_cast<PFN_cuMemGetInfo_v2>(
        get_cuda_proc(nvcuda_module_, "cuMemGetInfo_v2")
    );
    fn_ctx_set_current_ = reinterpret_cast<PFN_cuCtxSetCurrent>(
        get_cuda_proc(nvcuda_module_, "cuCtxSetCurrent")
    );

    CUresult res = cu_fns_->cuInit(0);
    if (res != CUDA_SUCCESS) {
        return fail(format_cu_error(res, "cuInit"));
    }

    if (!cu_fns_->cuGLGetDevices) {
        return fail("cuGLGetDevices symbol missing from CUDA driver");
    }

    unsigned int gl_dev_count = 0;
    CUdevice gl_dev = 0;
    res = cu_fns_->cuGLGetDevices(&gl_dev_count, &gl_dev, 1, CU_GL_DEVICE_LIST_ALL);
    if (res != CUDA_SUCCESS || gl_dev_count == 0) {
        return fail(format_cu_error(
            res,
            "cuGLGetDevices (Active OpenGL context is NOT running on an NVIDIA CUDA GPU)"
        ));
    }
    cu_dev_ = gl_dev;

    res = cu_fns_->cuDevicePrimaryCtxRetain(&cu_ctx_, cu_dev_);
    if (res != CUDA_SUCCESS) {
        return fail(format_cu_error(res, "cuDevicePrimaryCtxRetain"));
    }
    retained_primary_ctx_ = true;

    if (fn_ctx_set_current_) {
        res = fn_ctx_set_current_(cu_ctx_);
    } else {
        res = cu_fns_->cuCtxPushCurrent(cu_ctx_);
    }
    if (res != CUDA_SUCCESS) {
        return fail(format_cu_error(res, "cuCtxSetCurrent"));
    }

    res = cu_fns_->cuStreamCreate(&cu_stream_, CU_STREAM_NON_BLOCKING);
    if (res != CUDA_SUCCESS) {
        return fail(format_cu_error(res, "cuStreamCreate"));
    }

    // Load compiled PTX module for RGBA8->NV12 BT.709 color conversion
    const std::string ptx_null_terminated(kColorConvertPtx);
    res = cu_fns_->cuModuleLoadData(&cu_module_, ptx_null_terminated.c_str());
    if (res != CUDA_SUCCESS) {
        return fail(format_cu_error(res, "cuModuleLoadData(color_convert_ptx)"));
    }

    res = cu_fns_->cuModuleGetFunction(&fn_rgba_to_nv12_, cu_module_, "rgba8_gl_to_nv12_bt709_kernel");
    if (res != CUDA_SUCCESS) {
        return fail(format_cu_error(res, "cuModuleGetFunction(rgba8_gl_to_nv12_bt709_kernel)"));
    }

    char name[256] = {};
    cu_fns_->cuDeviceGetName(name, sizeof(name), cu_dev_);
    int drv_ver = 0;
    cu_fns_->cuDriverGetVersion(&drv_ver);
    int major = 0;
    int minor = 0;
    cu_fns_->cuDeviceComputeCapability(&major, &minor, cu_dev_);
    size_t free_b = 0;
    size_t total_b = 0;
    if (fn_mem_get_info_) {
        fn_mem_get_info_(&free_b, &total_b);
    }

    info_ = CudaDeviceInfo{
        .device_index = static_cast<int>(cu_dev_),
        .name = name,
        .driver_version = drv_ver,
        .cc_major = major,
        .cc_minor = minor,
        .free_vram_bytes = free_b,
        .total_vram_bytes = total_b,
        .gl_interop_colocated = true,
    };
    return info_;
}

Result<void> CudaInteropContext::register_gl_texture(
    uint32_t gl_tex_id,
    int width,
    int height,
    RegisteredGlTexture& out_slot
) {
    unregister_gl_texture(out_slot);
    if (fn_ctx_set_current_) fn_ctx_set_current_(cu_ctx_);

    CUgraphicsResource res_handle = nullptr;
    const CUresult r = cu_fns_->cuGraphicsGLRegisterImage(
        &res_handle,
        gl_tex_id,
        kGlTexture2D,
        CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY
    );
    if (r != CUDA_SUCCESS) {
        return fail(format_cu_error(r, "cuGraphicsGLRegisterImage"));
    }

    CUevent ev_start = nullptr;
    CUevent ev_done = nullptr;
    if (cu_fns_->cuEventCreate(&ev_start, CU_EVENT_DEFAULT) != CUDA_SUCCESS ||
        cu_fns_->cuEventCreate(&ev_done, CU_EVENT_DEFAULT) != CUDA_SUCCESS) {
        if (ev_start) cu_fns_->cuEventDestroy(ev_start);
        if (ev_done) cu_fns_->cuEventDestroy(ev_done);
        cu_fns_->cuGraphicsUnregisterResource(res_handle);
        return fail("Failed to create CUDA timing events for GL texture slot");
    }

    out_slot.gl_tex_id = gl_tex_id;
    out_slot.width = width;
    out_slot.height = height;
    out_slot.resource = res_handle;
    out_slot.active_tex_obj = 0;
    out_slot.conversion_start = ev_start;
    out_slot.conversion_done = ev_done;
    out_slot.timing_pending = false;
    return {};
}

void CudaInteropContext::unregister_gl_texture(RegisteredGlTexture& slot) noexcept {
    if (!cu_fns_ || !cu_ctx_) return;
    if (fn_ctx_set_current_) fn_ctx_set_current_(cu_ctx_);

    if (slot.conversion_done) {
        cu_fns_->cuEventSynchronize(slot.conversion_done);
    }
    if (slot.active_tex_obj != 0) {
        cu_fns_->cuTexObjectDestroy(slot.active_tex_obj);
        slot.active_tex_obj = 0;
    }
    if (slot.conversion_start) {
        cu_fns_->cuEventDestroy(slot.conversion_start);
        slot.conversion_start = nullptr;
    }
    if (slot.conversion_done) {
        cu_fns_->cuEventDestroy(slot.conversion_done);
        slot.conversion_done = nullptr;
    }
    if (slot.resource) {
        cu_fns_->cuGraphicsUnregisterResource(slot.resource);
        slot.resource = nullptr;
    }
    slot.gl_tex_id = 0;
    slot.timing_pending = false;
}

Result<void> CudaInteropContext::convert_gl_textures_batch_to_nv12(
    std::span<const BatchNv12Target> items,
    bool record_timing,
    std::vector<float>& out_completed_ms
) {
    out_completed_ms.clear();
    if (items.empty()) return {};
    if (fn_ctx_set_current_) fn_ctx_set_current_(cu_ctx_);

    std::vector<CUgraphicsResource> resources;
    resources.reserve(items.size());

    for (const auto& item : items) {
        RegisteredGlTexture& slot = *item.slot;
        if (slot.active_tex_obj != 0) {
            const CUresult sync_r = cu_fns_->cuEventSynchronize(slot.conversion_done);
            if (sync_r != CUDA_SUCCESS) {
                return fail(format_cu_error(sync_r, "cuEventSynchronize(slot recycle)"));
            }
            if (slot.timing_pending && fn_event_elapsed_time_) {
                float prev_ms = -1.0f;
                fn_event_elapsed_time_(&prev_ms, slot.conversion_start, slot.conversion_done);
                if (prev_ms >= 0.0f) out_completed_ms.push_back(prev_ms);
                slot.timing_pending = false;
            }
            cu_fns_->cuTexObjectDestroy(slot.active_tex_obj);
            slot.active_tex_obj = 0;
        }
        resources.push_back(slot.resource);
    }

    CUresult r = cu_fns_->cuGraphicsMapResources(
        static_cast<unsigned int>(resources.size()),
        resources.data(),
        cu_stream_
    );
    if (r != CUDA_SUCCESS) {
        return fail(format_cu_error(r, "cuGraphicsMapResources(batch)"));
    }

    for (const auto& item : items) {
        RegisteredGlTexture& slot = *item.slot;
        CUarray cu_arr = nullptr;
        r = cu_fns_->cuGraphicsSubResourceGetMappedArray(&cu_arr, slot.resource, 0, 0);
        if (r != CUDA_SUCCESS) {
            cu_fns_->cuGraphicsUnmapResources(static_cast<unsigned int>(resources.size()), resources.data(), cu_stream_);
            return fail(format_cu_error(r, "cuGraphicsSubResourceGetMappedArray"));
        }

        CUDA_RESOURCE_DESC res_desc{};
        res_desc.resType = CU_RESOURCE_TYPE_ARRAY;
        res_desc.res.array.hArray = cu_arr;

        CUDA_TEXTURE_DESC tex_desc{};
        tex_desc.addressMode[0] = CU_TR_ADDRESS_MODE_CLAMP;
        tex_desc.addressMode[1] = CU_TR_ADDRESS_MODE_CLAMP;
        tex_desc.addressMode[2] = CU_TR_ADDRESS_MODE_CLAMP;
        tex_desc.filterMode = CU_TR_FILTER_MODE_POINT;
        tex_desc.flags = CU_TRSF_READ_AS_INTEGER;

        CUtexObject tex_obj = 0;
        r = cu_fns_->cuTexObjectCreate(&tex_obj, &res_desc, &tex_desc, nullptr);
        if (r != CUDA_SUCCESS) {
            cu_fns_->cuGraphicsUnmapResources(static_cast<unsigned int>(resources.size()), resources.data(), cu_stream_);
            return fail(format_cu_error(r, "cuTexObjectCreate"));
        }
        slot.active_tex_obj = tex_obj;

        if (record_timing) {
            cu_fns_->cuEventRecord(slot.conversion_start, cu_stream_);
        }

        uint64_t arg_tex = static_cast<uint64_t>(tex_obj);
        CUdeviceptr arg_y = item.dst_y;
        int arg_pitch_y = item.pitch_y;
        CUdeviceptr arg_uv = item.dst_uv;
        int arg_pitch_uv = item.pitch_uv;
        int arg_w = slot.width;
        int arg_h = slot.height;
        int arg_flip = item.flip_y ? 1 : 0;
        float arg_bgr = item.bg_color.r;
        float arg_bgg = item.bg_color.g;
        float arg_bgb = item.bg_color.b;

        void* params[] = {
            &arg_tex,
            &arg_y,
            &arg_pitch_y,
            &arg_uv,
            &arg_pitch_uv,
            &arg_w,
            &arg_h,
            &arg_flip,
            &arg_bgr,
            &arg_bgg,
            &arg_bgb,
        };

        const unsigned int bx = 16u;
        const unsigned int by = 16u;
        const unsigned int uv_w = static_cast<unsigned int>((slot.width + 1) / 2);
        const unsigned int uv_h = static_cast<unsigned int>((slot.height + 1) / 2);
        const unsigned int gx = (uv_w + bx - 1u) / bx;
        const unsigned int gy = (uv_h + by - 1u) / by;

        r = cu_fns_->cuLaunchKernel(
            fn_rgba_to_nv12_,
            gx,
            gy,
            1u,
            bx,
            by,
            1u,
            0u,
            cu_stream_,
            params,
            nullptr
        );
        if (r != CUDA_SUCCESS) {
            cu_fns_->cuGraphicsUnmapResources(static_cast<unsigned int>(resources.size()), resources.data(), cu_stream_);
            return fail(format_cu_error(r, "cuLaunchKernel(rgba8_gl_to_nv12_bt709_kernel)"));
        }

        cu_fns_->cuEventRecord(slot.conversion_done, cu_stream_);
        slot.timing_pending = record_timing;
    }

    r = cu_fns_->cuGraphicsUnmapResources(
        static_cast<unsigned int>(resources.size()),
        resources.data(),
        cu_stream_
    );
    if (r != CUDA_SUCCESS) {
        return fail(format_cu_error(r, "cuGraphicsUnmapResources(batch)"));
    }
    return {};
}

Result<float> CudaInteropContext::convert_gl_texture_to_nv12(
    RegisteredGlTexture& slot,
    CUdeviceptr dst_y,
    int pitch_y,
    CUdeviceptr dst_uv,
    int pitch_uv,
    bool flip_y,
    ColorRGBA bg_color,
    bool record_timing
) {
    const BatchNv12Target item{
        .slot = &slot,
        .dst_y = dst_y,
        .pitch_y = pitch_y,
        .dst_uv = dst_uv,
        .pitch_uv = pitch_uv,
        .flip_y = flip_y,
        .bg_color = bg_color,
    };
    std::vector<float> ms;
    if (auto r = convert_gl_textures_batch_to_nv12(std::span{&item, 1}, record_timing, ms); !r) {
        return fail(r.error());
    }
    return ms.empty() ? -1.0f : ms[0];
}

Result<void> CudaInteropContext::synchronize_stream() {
    if (!cu_fns_ || !cu_stream_) return {};
    if (fn_ctx_set_current_) fn_ctx_set_current_(cu_ctx_);
    const CUresult r = cu_fns_->cuStreamSynchronize(cu_stream_);
    if (r != CUDA_SUCCESS) {
        return fail(format_cu_error(r, "cuStreamSynchronize"));
    }
    return {};
}

Result<void> CudaInteropContext::download_nv12_for_verification(
    CUdeviceptr src_y,
    int pitch_y,
    CUdeviceptr src_uv,
    int pitch_uv,
    int width,
    int height,
    std::vector<uint8_t>& out_y,
    std::vector<uint8_t>& out_uv
) {
    if (auto s = synchronize_stream(); !s) return s;

    out_y.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
    out_uv.resize(static_cast<size_t>(width) * static_cast<size_t>(height / 2));

    CUDA_MEMCPY2D copy_y{};
    copy_y.srcMemoryType = CU_MEMORYTYPE_DEVICE;
    copy_y.srcDevice = src_y;
    copy_y.srcPitch = static_cast<size_t>(pitch_y);
    copy_y.dstMemoryType = CU_MEMORYTYPE_HOST;
    copy_y.dstHost = out_y.data();
    copy_y.dstPitch = static_cast<size_t>(width);
    copy_y.WidthInBytes = static_cast<size_t>(width);
    copy_y.Height = static_cast<size_t>(height);

    CUresult r = cu_fns_->cuMemcpy2D(&copy_y);
    if (r != CUDA_SUCCESS) {
        return fail(format_cu_error(r, "cuMemcpy2D(Y)"));
    }

    CUDA_MEMCPY2D copy_uv{};
    copy_uv.srcMemoryType = CU_MEMORYTYPE_DEVICE;
    copy_uv.srcDevice = src_uv;
    copy_uv.srcPitch = static_cast<size_t>(pitch_uv);
    copy_uv.dstMemoryType = CU_MEMORYTYPE_HOST;
    copy_uv.dstHost = out_uv.data();
    copy_uv.dstPitch = static_cast<size_t>(width);
    copy_uv.WidthInBytes = static_cast<size_t>(width);
    copy_uv.Height = static_cast<size_t>(height / 2);

    r = cu_fns_->cuMemcpy2D(&copy_uv);
    if (r != CUDA_SUCCESS) {
        return fail(format_cu_error(r, "cuMemcpy2D(UV)"));
    }
    return {};
}

Result<std::pair<size_t, size_t>> CudaInteropContext::query_vram_bytes() const {
    if (!fn_mem_get_info_ || !cu_ctx_) {
        return fail("cuMemGetInfo_v2 unavailable");
    }
    if (fn_ctx_set_current_) fn_ctx_set_current_(cu_ctx_);
    size_t free_b = 0;
    size_t total_b = 0;
    const CUresult r = fn_mem_get_info_(&free_b, &total_b);
    if (r != CUDA_SUCCESS) {
        return fail(format_cu_error(r, "cuMemGetInfo_v2"));
    }
    return std::pair{free_b, total_b};
}

void CudaInteropContext::shutdown() noexcept {
    if (cu_fns_ && cu_ctx_) {
        if (fn_ctx_set_current_) fn_ctx_set_current_(cu_ctx_);
        if (cu_stream_) {
            cu_fns_->cuStreamSynchronize(cu_stream_);
            cu_fns_->cuStreamDestroy(cu_stream_);
            cu_stream_ = nullptr;
        }
        if (cu_module_) {
            cu_fns_->cuModuleUnload(cu_module_);
            cu_module_ = nullptr;
            fn_rgba_to_nv12_ = nullptr;
        }
        if (retained_primary_ctx_) {
            cu_fns_->cuDevicePrimaryCtxRelease(cu_dev_);
            retained_primary_ctx_ = false;
        }
        cu_ctx_ = nullptr;
    }
    if (cu_fns_) {
        cuda_free_functions(&cu_fns_);
        cu_fns_ = nullptr;
    }
    if (nvcuda_module_) {
        close_cuda_driver_lib(nvcuda_module_);
        nvcuda_module_ = nullptr;
    }
}

}  // namespace stem::interop
