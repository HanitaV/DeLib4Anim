#pragma once

#include "native/core/types.hpp"
#include "native/interop/cuda_drv.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>
#include <libswresample/swresample.h>
}

#include <cstdint>
#include <string>
#include <vector>

namespace stem::encode {

struct EncoderConfig {
    std::string output_path = "outputs/out.mp4";
    int width = 1920;
    int height = 1080;
    RationalFPS fps{60, 1};
    int64_t bitrate_bps = 12'000'000;
    std::string preset = "p4";  // NVENC p1 (fastest) .. p7 (highest quality)
    int rc_lookahead = 0;       // 0 minimizes pipeline latency/stall
    int hw_pool_size = 6;
    bool enable_audio = true;
    int audio_sample_rate = 48000;
};

struct GpuVideoFrame {
    AVFrame* av_frame = nullptr;
    CUdeviceptr y_ptr = 0;
    int y_pitch = 0;
    CUdeviceptr uv_ptr = 0;
    int uv_pitch = 0;
};

class NvencMp4Exporter {
public:
    NvencMp4Exporter() = default;
    ~NvencMp4Exporter();

    NvencMp4Exporter(const NvencMp4Exporter&) = delete;
    NvencMp4Exporter& operator=(const NvencMp4Exporter&) = delete;

    // Initializes AVHWDeviceContext wrapping `interop.cu_context()` + `interop.cu_stream()`,
    // AVHWFramesContext (AV_PIX_FMT_CUDA, sw_format=NV12), h264_nvenc, optional AAC audio, and MP4 muxer.
    [[nodiscard]] Result<void> open(
        interop::CudaInteropContext& interop,
        const EncoderConfig& cfg
    );

    // Allocates/borrows a GPU NV12 frame from AVHWFramesContext pool (no CPU staging).
    [[nodiscard]] Result<GpuVideoFrame> acquire_gpu_frame();

    // Sends GPU frame directly to h264_nvenc and drains ready compressed packets into MP4 muxer.
    [[nodiscard]] Result<void> submit_gpu_frame(GpuVideoFrame& frame, int64_t frame_index);

    // Encodes pre-mixed stereo float32 PCM audio into AAC packets before finalizing MP4.
    [[nodiscard]] Result<void> write_audio_f32_stereo(const std::vector<float>& interleaved_stereo);

    // Flushes video & audio encoders to AVERROR_EOF, writes MP4 trailer, and releases FFmpeg handles.
    [[nodiscard]] Result<void> finalize();

    [[nodiscard]] int64_t packets_written() const noexcept { return video_packets_written_; }
    [[nodiscard]] bool is_open() const noexcept { return open_; }

    [[nodiscard]] static Result<std::string> probe_nvenc_support(interop::CudaInteropContext& interop);

    void close() noexcept;

private:
    Result<void> drain_encoder(AVCodecContext* ctx, AVStream* stream, AVPacket* pkt);
    static std::string av_err_str(int errnum);

    EncoderConfig cfg_{};
    bool open_ = false;
    bool finalized_ = false;

    AVBufferRef* hw_device_ref_ = nullptr;
    AVBufferRef* hw_frames_ref_ = nullptr;
    AVFormatContext* fmt_ctx_ = nullptr;

    AVCodecContext* vcodec_ctx_ = nullptr;
    AVStream* vstream_ = nullptr;
    AVPacket* vpkt_ = nullptr;

    AVCodecContext* acodec_ctx_ = nullptr;
    AVStream* astream_ = nullptr;
    AVPacket* apkt_ = nullptr;
    SwrContext* swr_ctx_ = nullptr;
    int64_t audio_pts_ = 0;

    int64_t video_packets_written_ = 0;
};

}  // namespace stem::encode
