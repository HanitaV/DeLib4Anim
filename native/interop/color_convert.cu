// CUDA color conversion kernel: OpenGL RGBA8 texture (CUarray via CUtexObject) -> pitched NV12 (BT.709 Limited Range)
// Supports both nvcc and standalone NVRTC (--std=c++20 --generate-line-info).

using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;

struct alignas(4) RgbaU8 {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

// Unnormalized integer 2D texture fetch (CU_TRSF_READ_AS_INTEGER, 4x uint8 channels, CLAMP).
__device__ __forceinline__ RgbaU8 tex2d_rgba_u8(uint64_t tex_obj, int x, int y) {
    uint32_t r = 0;
    uint32_t g = 0;
    uint32_t b = 0;
    uint32_t a = 255;
    asm("tex.2d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5, %6}];"
        : "=r"(r), "=r"(g), "=r"(b), "=r"(a)
        : "l"(tex_obj), "r"(x), "r"(y));
    return RgbaU8{
        static_cast<uint8_t>(r),
        static_cast<uint8_t>(g),
        static_cast<uint8_t>(b),
        static_cast<uint8_t>(a),
    };
}

__device__ __forceinline__ void composite_over_bg(
    RgbaU8 px,
    float bg_r,
    float bg_g,
    float bg_b,
    float& out_r01,
    float& out_g01,
    float& out_b01
) {
    const float inv255 = 1.0f / 255.0f;
    const float a = static_cast<float>(px.a) * inv255;
    const float r = static_cast<float>(px.r) * inv255;
    const float g = static_cast<float>(px.g) * inv255;
    const float b = static_cast<float>(px.b) * inv255;
    out_r01 = r * a + bg_r * (1.0f - a);
    out_g01 = g * a + bg_g * (1.0f - a);
    out_b01 = b * a + bg_b * (1.0f - a);
}

__device__ __forceinline__ uint8_t clamp_u8_round(float v, float lo, float hi) {
    const float c = v < lo ? lo : (v > hi ? hi : v);
    return static_cast<uint8_t>(c + 0.5f);
}

// ITU-R BT.709-6 Studio / Limited Range (Y in [16..235], Cb/Cr in [16..240])
// Input: gamma-encoded sRGB/BT.709 R'G'B' in [0..1].
// Each thread processes one 2x2 pixel quad -> 4 Y samples + 1 (Cb, Cr) chroma pair.
extern "C" __global__ void rgba8_gl_to_nv12_bt709_kernel(
    uint64_t gl_tex_obj,
    uint8_t* __restrict__ dst_y,
    int pitch_y,
    uint8_t* __restrict__ dst_uv,
    int pitch_uv,
    int width,
    int height,
    int flip_y,
    float bg_r,
    float bg_g,
    float bg_b
) {
    const int uv_x = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    const int uv_y = static_cast<int>(blockIdx.y * blockDim.y + threadIdx.y);
    const int x0 = uv_x * 2;
    const int y0 = uv_y * 2;
    if (x0 >= width || y0 >= height) {
        return;
    }

    const int x1 = (x0 + 1 < width) ? (x0 + 1) : x0;
    const int y1 = (y0 + 1 < height) ? (y0 + 1) : y0;

    const int src_y0 = flip_y ? ((height - 1) - y0) : y0;
    const int src_y1 = flip_y ? ((height - 1) - y1) : y1;

    const RgbaU8 p00 = tex2d_rgba_u8(gl_tex_obj, x0, src_y0);
    const RgbaU8 p10 = tex2d_rgba_u8(gl_tex_obj, x1, src_y0);
    const RgbaU8 p01 = tex2d_rgba_u8(gl_tex_obj, x0, src_y1);
    const RgbaU8 p11 = tex2d_rgba_u8(gl_tex_obj, x1, src_y1);

    float r00, g00, b00;
    float r10, g10, b10;
    float r01, g01, b01;
    float r11, g11, b11;
    composite_over_bg(p00, bg_r, bg_g, bg_b, r00, g00, b00);
    composite_over_bg(p10, bg_r, bg_g, bg_b, r10, g10, b10);
    composite_over_bg(p01, bg_r, bg_g, bg_b, r01, g01, b01);
    composite_over_bg(p11, bg_r, bg_g, bg_b, r11, g11, b11);

    // BT.709 luma: Y' = 16 + 219 * (0.2126 R' + 0.7152 G' + 0.0722 B')
    constexpr float kYr = 219.0f * 0.2126f;
    constexpr float kYg = 219.0f * 0.7152f;
    constexpr float kYb = 219.0f * 0.0722f;

    const uint8_t y_00 = clamp_u8_round(16.0f + kYr * r00 + kYg * g00 + kYb * b00, 16.0f, 235.0f);
    const uint8_t y_10 = clamp_u8_round(16.0f + kYr * r10 + kYg * g10 + kYb * b10, 16.0f, 235.0f);
    const uint8_t y_01 = clamp_u8_round(16.0f + kYr * r01 + kYg * g01 + kYb * b01, 16.0f, 235.0f);
    const uint8_t y_11 = clamp_u8_round(16.0f + kYr * r11 + kYg * g11 + kYb * b11, 16.0f, 235.0f);

    uint8_t* row_y0 = dst_y + static_cast<size_t>(y0) * static_cast<size_t>(pitch_y);
    row_y0[x0] = y_00;
    if (x0 + 1 < width) {
        row_y0[x0 + 1] = y_10;
    }
    if (y0 + 1 < height) {
        uint8_t* row_y1 = dst_y + static_cast<size_t>(y0 + 1) * static_cast<size_t>(pitch_y);
        row_y1[x0] = y_01;
        if (x0 + 1 < width) {
            row_y1[x0 + 1] = y_11;
        }
    }

    // 2x2 box average R'G'B' for 4:2:0 chroma subsampling
    const float r_avg = 0.25f * (r00 + r10 + r01 + r11);
    const float g_avg = 0.25f * (g00 + g10 + g01 + g11);
    const float b_avg = 0.25f * (b00 + b10 + b01 + b11);

    // BT.709 chroma:
    // Cb = 128 + 224 * (-0.114572106f * R' - 0.385427894f * G' + 0.5f * B')
    // Cr = 128 + 224 * ( 0.5f * R' - 0.454152908f * G' - 0.045847092f * B')
    constexpr float kCbR = -224.0f * (0.2126f / 1.8556f);
    constexpr float kCbG = -224.0f * (0.7152f / 1.8556f);
    constexpr float kCbB = 224.0f * 0.5f;

    constexpr float kCrR = 224.0f * 0.5f;
    constexpr float kCrG = -224.0f * (0.7152f / 1.5748f);
    constexpr float kCrB = -224.0f * (0.0722f / 1.5748f);

    const uint8_t cb = clamp_u8_round(128.0f + kCbR * r_avg + kCbG * g_avg + kCbB * b_avg, 16.0f, 240.0f);
    const uint8_t cr = clamp_u8_round(128.0f + kCrR * r_avg + kCrG * g_avg + kCrB * b_avg, 16.0f, 240.0f);

    uint8_t* row_uv = dst_uv + static_cast<size_t>(uv_y) * static_cast<size_t>(pitch_uv);
    row_uv[uv_x * 2 + 0] = cb;
    row_uv[uv_x * 2 + 1] = cr;
}
