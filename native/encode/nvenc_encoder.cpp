#include "native/encode/nvenc_encoder.hpp"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <format>

namespace stem::encode {

std::string NvencMp4Exporter::av_err_str(int errnum) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(errnum, buf, sizeof(buf));
    return std::string(buf);
}

NvencMp4Exporter::~NvencMp4Exporter() {
    close();
}

Result<std::string> NvencMp4Exporter::probe_nvenc_support(interop::CudaInteropContext& interop) {
    const AVCodec* codec = avcodec_find_encoder_by_name("h264_nvenc");
    if (!codec) {
        return fail("FFmpeg h264_nvenc encoder not found in linked libavcodec");
    }

    AVBufferRef* dev_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA);
    if (!dev_ref) {
        return fail("av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA) failed");
    }
    auto* hw_dev = reinterpret_cast<AVHWDeviceContext*>(dev_ref->data);
    auto* cuda_dev = reinterpret_cast<AVCUDADeviceContext*>(hw_dev->hwctx);
    cuda_dev->cuda_ctx = interop.cu_context();
    cuda_dev->stream = interop.cu_stream();
    int rc = av_hwdevice_ctx_init(dev_ref);
    if (rc < 0) {
        av_buffer_unref(&dev_ref);
        return fail(std::format("av_hwdevice_ctx_init failed: {}", av_err_str(rc)));
    }

    AVBufferRef* frames_ref = av_hwframe_ctx_alloc(dev_ref);
    if (!frames_ref) {
        av_buffer_unref(&dev_ref);
        return fail("av_hwframe_ctx_alloc failed");
    }
    auto* fc = reinterpret_cast<AVHWFramesContext*>(frames_ref->data);
    fc->format = AV_PIX_FMT_CUDA;
    fc->sw_format = AV_PIX_FMT_NV12;
    fc->width = 256;
    fc->height = 256;
    fc->initial_pool_size = 2;
    rc = av_hwframe_ctx_init(frames_ref);
    if (rc < 0) {
        av_buffer_unref(&frames_ref);
        av_buffer_unref(&dev_ref);
        return fail(std::format("av_hwframe_ctx_init(NV12/CUDA) failed: {}", av_err_str(rc)));
    }

    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    ctx->width = 256;
    ctx->height = 256;
    ctx->time_base = AVRational{1, 60};
    ctx->framerate = AVRational{60, 1};
    ctx->pix_fmt = AV_PIX_FMT_CUDA;
    ctx->sw_pix_fmt = AV_PIX_FMT_NV12;
    ctx->hw_frames_ctx = av_buffer_ref(frames_ref);
    ctx->color_primaries = AVCOL_PRI_BT709;
    ctx->color_trc = AVCOL_TRC_BT709;
    ctx->colorspace = AVCOL_SPC_BT709;
    ctx->color_range = AVCOL_RANGE_MPEG;

    av_opt_set(ctx->priv_data, "preset", "p4", 0);
    av_opt_set(ctx->priv_data, "rc", "vbr", 0);

    rc = avcodec_open2(ctx, codec, nullptr);
    avcodec_free_context(&ctx);
    av_buffer_unref(&frames_ref);
    av_buffer_unref(&dev_ref);

    if (rc < 0) {
        return fail(std::format("avcodec_open2(h264_nvenc) failed: {}", av_err_str(rc)));
    }
    return std::string("h264_nvenc + AV_PIX_FMT_CUDA (NV12, BT.709) verified");
}

Result<void> NvencMp4Exporter::open(
    interop::CudaInteropContext& interop,
    const EncoderConfig& cfg
) {
    close();
    cfg_ = cfg;

    if (cfg_.width <= 0 || cfg_.height <= 0 || (cfg_.width & 1) != 0 || (cfg_.height & 1) != 0) {
        return fail(std::format(
            "NV12/H.264 requires even positive dimensions, got {}x{}",
            cfg_.width,
            cfg_.height
        ));
    }
    if (!cfg_.fps.valid()) {
        return fail("Invalid RationalFPS");
    }

    const std::filesystem::path out_p(cfg_.output_path);
    if (out_p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(out_p.parent_path(), ec);
    }

    // 1. Wrap unified CUDA primary context into FFmpeg AVHWDeviceContext
    hw_device_ref_ = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA);
    if (!hw_device_ref_) {
        close();
        return fail("av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA) failed");
    }
    auto* hw_dev = reinterpret_cast<AVHWDeviceContext*>(hw_device_ref_->data);
    auto* cuda_dev = reinterpret_cast<AVCUDADeviceContext*>(hw_dev->hwctx);
    cuda_dev->cuda_ctx = interop.cu_context();
    cuda_dev->stream = interop.cu_stream();

    int rc = av_hwdevice_ctx_init(hw_device_ref_);
    if (rc < 0) {
        close();
        return fail(std::format("av_hwdevice_ctx_init failed: {}", av_err_str(rc)));
    }

    // 2. Allocate CUDA NV12 GPU frame pool
    hw_frames_ref_ = av_hwframe_ctx_alloc(hw_device_ref_);
    if (!hw_frames_ref_) {
        close();
        return fail("av_hwframe_ctx_alloc failed");
    }
    auto* fc = reinterpret_cast<AVHWFramesContext*>(hw_frames_ref_->data);
    fc->format = AV_PIX_FMT_CUDA;
    fc->sw_format = AV_PIX_FMT_NV12;
    fc->width = cfg_.width;
    fc->height = cfg_.height;
    fc->initial_pool_size = std::max(3, cfg_.hw_pool_size);

    rc = av_hwframe_ctx_init(hw_frames_ref_);
    if (rc < 0) {
        close();
        return fail(std::format("av_hwframe_ctx_init failed: {}", av_err_str(rc)));
    }

    // 3. Create MP4 muxer context
    rc = avformat_alloc_output_context2(&fmt_ctx_, nullptr, "mp4", cfg_.output_path.c_str());
    if (rc < 0 || !fmt_ctx_) {
        close();
        return fail(std::format("avformat_alloc_output_context2 failed: {}", av_err_str(rc)));
    }

    // 4. Open H.264 NVENC video stream
    const AVCodec* vcodec = avcodec_find_encoder_by_name("h264_nvenc");
    if (!vcodec) {
        close();
        return fail("h264_nvenc encoder unavailable");
    }
    vstream_ = avformat_new_stream(fmt_ctx_, vcodec);
    if (!vstream_) {
        close();
        return fail("avformat_new_stream(video) failed");
    }

    const RationalFPS rfps = cfg_.fps.reduced();
    vcodec_ctx_ = avcodec_alloc_context3(vcodec);
    vcodec_ctx_->codec_id = AV_CODEC_ID_H264;
    vcodec_ctx_->width = cfg_.width;
    vcodec_ctx_->height = cfg_.height;
    vcodec_ctx_->time_base = AVRational{static_cast<int>(rfps.den), static_cast<int>(rfps.num)};
    vcodec_ctx_->framerate = AVRational{static_cast<int>(rfps.num), static_cast<int>(rfps.den)};
    vcodec_ctx_->pix_fmt = AV_PIX_FMT_CUDA;
    vcodec_ctx_->sw_pix_fmt = AV_PIX_FMT_NV12;
    vcodec_ctx_->hw_frames_ctx = av_buffer_ref(hw_frames_ref_);
    vcodec_ctx_->bit_rate = cfg_.bitrate_bps;
    vcodec_ctx_->gop_size = static_cast<int>(std::max<int64_t>(30, rfps.num / std::max<int64_t>(1, rfps.den)));
    vcodec_ctx_->max_b_frames = 2;

    // Explicit SDR ITU-R BT.709 Limited Range signaling matching our CUDA kernel math
    vcodec_ctx_->color_primaries = AVCOL_PRI_BT709;
    vcodec_ctx_->color_trc = AVCOL_TRC_BT709;
    vcodec_ctx_->colorspace = AVCOL_SPC_BT709;
    vcodec_ctx_->color_range = AVCOL_RANGE_MPEG;

    if (fmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER) {
        vcodec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    av_opt_set(vcodec_ctx_->priv_data, "preset", cfg_.preset.c_str(), 0);
    av_opt_set(vcodec_ctx_->priv_data, "tune", "hq", 0);
    av_opt_set(vcodec_ctx_->priv_data, "profile", "high", 0);
    av_opt_set(vcodec_ctx_->priv_data, "rc", "vbr", 0);
    av_opt_set_int(vcodec_ctx_->priv_data, "cq", 19, 0);
    av_opt_set_int(vcodec_ctx_->priv_data, "rc-lookahead", cfg_.rc_lookahead, 0);
    av_opt_set_int(vcodec_ctx_->priv_data, "b_ref_mode", 0, 0);

    rc = avcodec_open2(vcodec_ctx_, vcodec, nullptr);
    if (rc < 0) {
        close();
        return fail(std::format("avcodec_open2(h264_nvenc) failed: {}", av_err_str(rc)));
    }

    rc = avcodec_parameters_from_context(vstream_->codecpar, vcodec_ctx_);
    if (rc < 0) {
        close();
        return fail(std::format("avcodec_parameters_from_context(video) failed: {}", av_err_str(rc)));
    }
    vstream_->time_base = vcodec_ctx_->time_base;
    vstream_->avg_frame_rate = vcodec_ctx_->framerate;
    vpkt_ = av_packet_alloc();

    // 5. Optional AAC stereo audio stream (48 kHz)
    if (cfg_.enable_audio) {
        const AVCodec* acodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        if (!acodec) {
            close();
            return fail("AAC encoder not found");
        }
        astream_ = avformat_new_stream(fmt_ctx_, acodec);
        acodec_ctx_ = avcodec_alloc_context3(acodec);
        acodec_ctx_->sample_rate = cfg_.audio_sample_rate;
        av_channel_layout_default(&acodec_ctx_->ch_layout, 2);
        acodec_ctx_->sample_fmt = AV_SAMPLE_FMT_FLTP;
        acodec_ctx_->bit_rate = 192000;
        acodec_ctx_->time_base = AVRational{1, cfg_.audio_sample_rate};
        if (fmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER) {
            acodec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        rc = avcodec_open2(acodec_ctx_, acodec, nullptr);
        if (rc < 0) {
            close();
            return fail(std::format("avcodec_open2(aac) failed: {}", av_err_str(rc)));
        }
        rc = avcodec_parameters_from_context(astream_->codecpar, acodec_ctx_);
        if (rc < 0) {
            close();
            return fail(std::format("avcodec_parameters_from_context(audio) failed: {}", av_err_str(rc)));
        }
        astream_->time_base = acodec_ctx_->time_base;

        AVChannelLayout in_layout{};
        av_channel_layout_default(&in_layout, 2);
        rc = swr_alloc_set_opts2(
            &swr_ctx_,
            &acodec_ctx_->ch_layout,
            acodec_ctx_->sample_fmt,
            acodec_ctx_->sample_rate,
            &in_layout,
            AV_SAMPLE_FMT_FLT,
            cfg_.audio_sample_rate,
            0,
            nullptr
        );
        av_channel_layout_uninit(&in_layout);
        if (rc < 0 || !swr_ctx_ || swr_init(swr_ctx_) < 0) {
            close();
            return fail("swr_alloc_set_opts2/swr_init failed");
        }
        apkt_ = av_packet_alloc();
    }

    if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
        rc = avio_open(&fmt_ctx_->pb, cfg_.output_path.c_str(), AVIO_FLAG_WRITE);
        if (rc < 0) {
            close();
            return fail(std::format("avio_open({}) failed: {}", cfg_.output_path, av_err_str(rc)));
        }
    }

    AVDictionary* mux_opts = nullptr;
    av_dict_set(&mux_opts, "movflags", "+faststart", 0);
    rc = avformat_write_header(fmt_ctx_, &mux_opts);
    av_dict_free(&mux_opts);
    if (rc < 0) {
        close();
        return fail(std::format("avformat_write_header failed: {}", av_err_str(rc)));
    }

    open_ = true;
    finalized_ = false;
    video_packets_written_ = 0;
    audio_pts_ = 0;
    return {};
}

Result<GpuVideoFrame> NvencMp4Exporter::acquire_gpu_frame() {
    if (!open_ || !hw_frames_ref_) {
        return fail("Encoder not open");
    }
    AVFrame* f = av_frame_alloc();
    if (!f) {
        return fail("av_frame_alloc failed");
    }
    const int rc = av_hwframe_get_buffer(hw_frames_ref_, f, 0);
    if (rc < 0) {
        av_frame_free(&f);
        return fail(std::format("av_hwframe_get_buffer failed: {}", av_err_str(rc)));
    }
    f->color_primaries = AVCOL_PRI_BT709;
    f->color_trc = AVCOL_TRC_BT709;
    f->colorspace = AVCOL_SPC_BT709;
    f->color_range = AVCOL_RANGE_MPEG;

    return GpuVideoFrame{
        .av_frame = f,
        .y_ptr = reinterpret_cast<CUdeviceptr>(f->data[0]),
        .y_pitch = f->linesize[0],
        .uv_ptr = reinterpret_cast<CUdeviceptr>(f->data[1]),
        .uv_pitch = f->linesize[1],
    };
}

Result<void> NvencMp4Exporter::drain_encoder(AVCodecContext* ctx, AVStream* stream, AVPacket* pkt) {
    while (true) {
        const int rc = avcodec_receive_packet(ctx, pkt);
        if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) {
            return {};
        }
        if (rc < 0) {
            return fail(std::format("avcodec_receive_packet failed: {}", av_err_str(rc)));
        }
        av_packet_rescale_ts(pkt, ctx->time_base, stream->time_base);
        pkt->stream_index = stream->index;
        const int wrc = av_interleaved_write_frame(fmt_ctx_, pkt);
        av_packet_unref(pkt);
        if (wrc < 0) {
            return fail(std::format("av_interleaved_write_frame failed: {}", av_err_str(wrc)));
        }
        if (ctx == vcodec_ctx_) {
            ++video_packets_written_;
        }
    }
}

Result<void> NvencMp4Exporter::submit_gpu_frame(GpuVideoFrame& frame, int64_t frame_index) {
    if (!open_ || !frame.av_frame) {
        return fail("submit_gpu_frame called with closed encoder or null frame");
    }
    frame.av_frame->pts = frame_index;
    frame.av_frame->duration = 1;
    const int rc = avcodec_send_frame(vcodec_ctx_, frame.av_frame);
    // Reference held internally by avcodec/hwframe pool until encoder consumes it; drop caller handle.
    av_frame_free(&frame.av_frame);
    if (rc < 0) {
        return fail(std::format("avcodec_send_frame(video) failed: {}", av_err_str(rc)));
    }
    return drain_encoder(vcodec_ctx_, vstream_, vpkt_);
}

Result<void> NvencMp4Exporter::write_audio_f32_stereo(const std::vector<float>& interleaved_stereo) {
    if (!open_ || !acodec_ctx_ || interleaved_stereo.empty()) return {};

    const int frame_sz = acodec_ctx_->frame_size > 0 ? acodec_ctx_->frame_size : 1024;
    const size_t total_samples = interleaved_stereo.size() / 2u;

    AVFrame* af = av_frame_alloc();
    af->nb_samples = frame_sz;
    af->format = acodec_ctx_->sample_fmt;
    av_channel_layout_copy(&af->ch_layout, &acodec_ctx_->ch_layout);
    af->sample_rate = acodec_ctx_->sample_rate;
    if (av_frame_get_buffer(af, 0) < 0) {
        av_frame_free(&af);
        return fail("av_frame_get_buffer(audio) failed");
    }

    size_t pos = 0;
    while (pos < total_samples) {
        const int chunk = static_cast<int>(std::min<size_t>(frame_sz, total_samples - pos));
        if (av_frame_make_writable(af) < 0) {
            av_frame_free(&af);
            return fail("av_frame_make_writable(audio) failed");
        }
        const uint8_t* in_data[1] = {
            reinterpret_cast<const uint8_t*>(interleaved_stereo.data() + pos * 2u)
        };
        const int converted = swr_convert(swr_ctx_, af->data, frame_sz, in_data, chunk);
        if (converted < 0) {
            av_frame_free(&af);
            return fail("swr_convert failed");
        }
        // Pad final partial AAC frame with silence so sample counts remain valid
        if (chunk < frame_sz) {
            for (int ch = 0; ch < 2; ++ch) {
                auto* dst = reinterpret_cast<float*>(af->data[ch]);
                std::fill(dst + converted, dst + frame_sz, 0.0f);
            }
        }
        af->nb_samples = frame_sz;
        af->pts = audio_pts_;
        audio_pts_ += frame_sz;

        const int rc = avcodec_send_frame(acodec_ctx_, af);
        if (rc < 0) {
            av_frame_free(&af);
            return fail(std::format("avcodec_send_frame(audio) failed: {}", av_err_str(rc)));
        }
        if (auto dr = drain_encoder(acodec_ctx_, astream_, apkt_); !dr) {
            av_frame_free(&af);
            return dr;
        }
        pos += static_cast<size_t>(chunk);
    }

    av_frame_free(&af);
    return {};
}

Result<void> NvencMp4Exporter::finalize() {
    if (!open_ || finalized_) return {};

    // Flush video encoder (preserves final frames)
    int rc = avcodec_send_frame(vcodec_ctx_, nullptr);
    if (rc < 0 && rc != AVERROR_EOF) {
        return fail(std::format("avcodec_send_frame(flush video) failed: {}", av_err_str(rc)));
    }
    if (auto dr = drain_encoder(vcodec_ctx_, vstream_, vpkt_); !dr) {
        return dr;
    }

    // Flush audio encoder
    if (acodec_ctx_) {
        rc = avcodec_send_frame(acodec_ctx_, nullptr);
        if (rc < 0 && rc != AVERROR_EOF) {
            return fail(std::format("avcodec_send_frame(flush audio) failed: {}", av_err_str(rc)));
        }
        if (auto dr = drain_encoder(acodec_ctx_, astream_, apkt_); !dr) {
            return dr;
        }
    }

    rc = av_write_trailer(fmt_ctx_);
    if (rc < 0) {
        return fail(std::format("av_write_trailer failed: {}", av_err_str(rc)));
    }

    finalized_ = true;
    close();
    return {};
}

void NvencMp4Exporter::close() noexcept {
    if (fmt_ctx_) {
        if (fmt_ctx_->oformat && !(fmt_ctx_->oformat->flags & AVFMT_NOFILE) && fmt_ctx_->pb) {
            avio_closep(&fmt_ctx_->pb);
        }
        avformat_free_context(fmt_ctx_);
        fmt_ctx_ = nullptr;
    }
    if (vpkt_) av_packet_free(&vpkt_);
    if (apkt_) av_packet_free(&apkt_);
    if (vcodec_ctx_) avcodec_free_context(&vcodec_ctx_);
    if (acodec_ctx_) avcodec_free_context(&acodec_ctx_);
    if (swr_ctx_) swr_free(&swr_ctx_);
    if (hw_frames_ref_) av_buffer_unref(&hw_frames_ref_);
    if (hw_device_ref_) av_buffer_unref(&hw_device_ref_);
    vstream_ = nullptr;
    astream_ = nullptr;
    open_ = false;
}

}  // namespace stem::encode
