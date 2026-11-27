#pragma once

#include "native/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include <GL/gl.h>

#if defined(_WIN32)
#define STEM_GLAPI __stdcall
#else
#define STEM_GLAPI
#endif

namespace stem::gl {

inline constexpr std::string_view kLoaderVersion = "stem-gl45-loader-1.0";

using GLsizeiptr = ptrdiff_t;
using GLintptr = ptrdiff_t;
using GLchar = char;
using GLuint64 = uint64_t;
using GLint64 = int64_t;

// Core OpenGL 4.5 constants (only those not guaranteed by Windows <GL/gl.h> 1.1)
inline constexpr GLenum kMajorVersion = 0x821B;
inline constexpr GLenum kMinorVersion = 0x821C;
inline constexpr GLenum kShadingLanguageVersion = 0x8B8C;

inline constexpr GLenum kTexture2D = 0x0DE1;
inline constexpr GLenum kTextureWrapS = 0x2802;
inline constexpr GLenum kTextureWrapT = 0x2803;
inline constexpr GLenum kTextureMinFilter = 0x2801;
inline constexpr GLenum kTextureMagFilter = 0x2800;
inline constexpr GLenum kClampToEdge = 0x812F;
inline constexpr GLenum kLinear = 0x2601;
inline constexpr GLenum kNearest = 0x2600;
inline constexpr GLenum kRgba8 = 0x8058;
inline constexpr GLenum kR8 = 0x8229;
inline constexpr GLenum kRed = 0x1903;
inline constexpr GLenum kRgba = 0x1908;
inline constexpr GLenum kUnsignedByte = 0x1401;
inline constexpr GLenum kDepth24Stencil8 = 0x88F0;

inline constexpr GLenum kFramebuffer = 0x8D40;
inline constexpr GLenum kReadFramebuffer = 0x8CA8;
inline constexpr GLenum kDrawFramebuffer = 0x8CA9;
inline constexpr GLenum kColorAttachment0 = 0x8CE0;
inline constexpr GLenum kDepthStencilAttachment = 0x821A;
inline constexpr GLenum kRenderbuffer = 0x8D41;
inline constexpr GLenum kFramebufferComplete = 0x8CD5;
inline constexpr GLenum kColorBufferBit = 0x00004000;
inline constexpr GLenum kStencilBufferBit = 0x00000400;
inline constexpr GLenum kDepthBufferBit = 0x00000100;

inline constexpr GLenum kArrayBuffer = 0x8892;
inline constexpr GLenum kElementArrayBuffer = 0x8893;
inline constexpr GLenum kStaticDraw = 0x88E4;
inline constexpr GLenum kDynamicDraw = 0x88E8;
inline constexpr GLenum kStreamDraw = 0x88E0;

inline constexpr GLenum kVertexShader = 0x8B31;
inline constexpr GLenum kFragmentShader = 0x8B30;
inline constexpr GLenum kCompileStatus = 0x8B81;
inline constexpr GLenum kLinkStatus = 0x8B82;
inline constexpr GLenum kInfoLogLength = 0x8B84;

inline constexpr GLenum kFloat = 0x1406;
inline constexpr GLenum kUnsignedInt = 0x1405;
inline constexpr GLenum kTriangles = 0x0004;
inline constexpr GLenum kTriangleStrip = 0x0005;
inline constexpr GLenum kTriangleFan = 0x0006;

inline constexpr GLenum kBlend = 0x0BE2;
inline constexpr GLenum kSrcAlpha = 0x0302;
inline constexpr GLenum kOneMinusSrcAlpha = 0x0303;
inline constexpr GLenum kOne = 1;
inline constexpr GLenum kZero = 0;
inline constexpr GLenum kCullFace = 0x0B44;
inline constexpr GLenum kDepthTest = 0x0B71;
inline constexpr GLenum kStencilTest = 0x0B90;
inline constexpr GLenum kMultisample = 0x809D;
inline constexpr GLenum kSampleAlphaToCoverage = 0x809E;
inline constexpr GLenum kFramebufferSrgb = 0x8DB9;
inline constexpr GLenum kUnpackAlignment = 0x0CF5;
inline constexpr GLenum kPackAlignment = 0x0D05;

inline constexpr GLenum kLess = 0x0201;
inline constexpr GLenum kLequal = 0x0203;
inline constexpr GLenum kAlways = 0x0207;
inline constexpr GLenum kNotequal = 0x0205;
inline constexpr GLenum kKeep = 0x1E00;
inline constexpr GLenum kIncrWrap = 0x8507;
inline constexpr GLenum kDecrWrap = 0x8508;
inline constexpr GLenum kFront = 0x0404;
inline constexpr GLenum kBack = 0x0405;
inline constexpr GLenum kFrontAndBack = 0x0408;

inline constexpr GLenum kTimeElapsed = 0x88BF;
inline constexpr GLenum kQueryResult = 0x8866;
inline constexpr GLenum kQueryResultAvailable = 0x8867;

using PFN_glGetIntegerv = void(STEM_GLAPI*)(GLenum pname, GLint* data);
using PFN_glGetString = const GLubyte*(STEM_GLAPI*)(GLenum name);
using PFN_glViewport = void(STEM_GLAPI*)(GLint x, GLint y, GLsizei width, GLsizei height);
using PFN_glClearColor = void(STEM_GLAPI*)(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
using PFN_glClearDepth = void(STEM_GLAPI*)(double depth);
using PFN_glClearStencil = void(STEM_GLAPI*)(GLint s);
using PFN_glClear = void(STEM_GLAPI*)(GLbitfield mask);
using PFN_glEnable = void(STEM_GLAPI*)(GLenum cap);
using PFN_glDisable = void(STEM_GLAPI*)(GLenum cap);
using PFN_glDepthMask = void(STEM_GLAPI*)(GLboolean flag);
using PFN_glDepthFunc = void(STEM_GLAPI*)(GLenum func);
using PFN_glBlendFunc = void(STEM_GLAPI*)(GLenum sfactor, GLenum dfactor);
using PFN_glBlendFuncSeparate = void(STEM_GLAPI*)(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
using PFN_glColorMask = void(STEM_GLAPI*)(GLboolean r, GLboolean g, GLboolean b, GLboolean a);
using PFN_glStencilMask = void(STEM_GLAPI*)(GLuint mask);
using PFN_glStencilFunc = void(STEM_GLAPI*)(GLenum func, GLint ref, GLuint mask);
using PFN_glStencilOp = void(STEM_GLAPI*)(GLenum fail, GLenum zfail, GLenum zpass);
using PFN_glStencilOpSeparate = void(STEM_GLAPI*)(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
using PFN_glPixelStorei = void(STEM_GLAPI*)(GLenum pname, GLint param);
using PFN_glFinish = void(STEM_GLAPI*)();
using PFN_glFlush = void(STEM_GLAPI*)();

using PFN_glCreateFramebuffers = void(STEM_GLAPI*)(GLsizei n, GLuint* framebuffers);
using PFN_glDeleteFramebuffers = void(STEM_GLAPI*)(GLsizei n, const GLuint* framebuffers);
using PFN_glBindFramebuffer = void(STEM_GLAPI*)(GLenum target, GLuint framebuffer);
using PFN_glNamedFramebufferTexture = void(STEM_GLAPI*)(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level);
using PFN_glNamedFramebufferRenderbuffer = void(STEM_GLAPI*)(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
using PFN_glCheckNamedFramebufferStatus = GLenum(STEM_GLAPI*)(GLuint framebuffer, GLenum target);
using PFN_glBlitNamedFramebuffer = void(STEM_GLAPI*)(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);

using PFN_glCreateRenderbuffers = void(STEM_GLAPI*)(GLsizei n, GLuint* renderbuffers);
using PFN_glDeleteRenderbuffers = void(STEM_GLAPI*)(GLsizei n, const GLuint* renderbuffers);
using PFN_glNamedRenderbufferStorageMultisample = void(STEM_GLAPI*)(GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);

using PFN_glCreateTextures = void(STEM_GLAPI*)(GLenum target, GLsizei n, GLuint* textures);
using PFN_glDeleteTextures = void(STEM_GLAPI*)(GLsizei n, const GLuint* textures);
using PFN_glTextureStorage2D = void(STEM_GLAPI*)(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
using PFN_glTextureSubImage2D = void(STEM_GLAPI*)(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
using PFN_glTextureParameteri = void(STEM_GLAPI*)(GLuint texture, GLenum pname, GLint param);
using PFN_glBindTextureUnit = void(STEM_GLAPI*)(GLuint unit, GLuint texture);
using PFN_glGetTextureImage = void(STEM_GLAPI*)(GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* pixels);

using PFN_glCreateBuffers = void(STEM_GLAPI*)(GLsizei n, GLuint* buffers);
using PFN_glDeleteBuffers = void(STEM_GLAPI*)(GLsizei n, const GLuint* buffers);
using PFN_glNamedBufferData = void(STEM_GLAPI*)(GLuint buffer, GLsizeiptr size, const void* data, GLenum usage);
using PFN_glNamedBufferSubData = void(STEM_GLAPI*)(GLuint buffer, GLintptr offset, GLsizeiptr size, const void* data);

using PFN_glCreateVertexArrays = void(STEM_GLAPI*)(GLsizei n, GLuint* arrays);
using PFN_glDeleteVertexArrays = void(STEM_GLAPI*)(GLsizei n, const GLuint* arrays);
using PFN_glBindVertexArray = void(STEM_GLAPI*)(GLuint array);
using PFN_glVertexArrayVertexBuffer = void(STEM_GLAPI*)(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
using PFN_glVertexArrayElementBuffer = void(STEM_GLAPI*)(GLuint vaobj, GLuint buffer);
using PFN_glEnableVertexArrayAttrib = void(STEM_GLAPI*)(GLuint vaobj, GLuint index);
using PFN_glVertexArrayAttribFormat = void(STEM_GLAPI*)(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset);
using PFN_glVertexArrayAttribBinding = void(STEM_GLAPI*)(GLuint vaobj, GLuint attribindex, GLuint bindingindex);
using PFN_glVertexArrayBindingDivisor = void(STEM_GLAPI*)(GLuint vaobj, GLuint bindingindex, GLuint divisor);

using PFN_glCreateShader = GLuint(STEM_GLAPI*)(GLenum type);
using PFN_glDeleteShader = void(STEM_GLAPI*)(GLuint shader);
using PFN_glShaderSource = void(STEM_GLAPI*)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
using PFN_glCompileShader = void(STEM_GLAPI*)(GLuint shader);
using PFN_glGetShaderiv = void(STEM_GLAPI*)(GLuint shader, GLenum pname, GLint* params);
using PFN_glGetShaderInfoLog = void(STEM_GLAPI*)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
using PFN_glCreateProgram = GLuint(STEM_GLAPI*)();
using PFN_glDeleteProgram = void(STEM_GLAPI*)(GLuint program);
using PFN_glAttachShader = void(STEM_GLAPI*)(GLuint program, GLuint shader);
using PFN_glLinkProgram = void(STEM_GLAPI*)(GLuint program);
using PFN_glGetProgramiv = void(STEM_GLAPI*)(GLuint program, GLenum pname, GLint* params);
using PFN_glGetProgramInfoLog = void(STEM_GLAPI*)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
using PFN_glUseProgram = void(STEM_GLAPI*)(GLuint program);
using PFN_glGetUniformLocation = GLint(STEM_GLAPI*)(GLuint program, const GLchar* name);
using PFN_glUniform1i = void(STEM_GLAPI*)(GLint location, GLint v0);
using PFN_glUniform1f = void(STEM_GLAPI*)(GLint location, GLfloat v0);
using PFN_glUniform2f = void(STEM_GLAPI*)(GLint location, GLfloat v0, GLfloat v1);
using PFN_glUniform4f = void(STEM_GLAPI*)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);

using PFN_glDrawArrays = void(STEM_GLAPI*)(GLenum mode, GLint first, GLsizei count);
using PFN_glDrawArraysInstanced = void(STEM_GLAPI*)(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
using PFN_glDrawElements = void(STEM_GLAPI*)(GLenum mode, GLsizei count, GLenum type, const void* indices);

using PFN_glGenQueries = void(STEM_GLAPI*)(GLsizei n, GLuint* ids);
using PFN_glDeleteQueries = void(STEM_GLAPI*)(GLsizei n, const GLuint* ids);
using PFN_glBeginQuery = void(STEM_GLAPI*)(GLenum target, GLuint id);
using PFN_glEndQuery = void(STEM_GLAPI*)(GLenum target);
using PFN_glGetQueryObjectiv = void(STEM_GLAPI*)(GLuint id, GLenum pname, GLint* params);
using PFN_glGetQueryObjectui64v = void(STEM_GLAPI*)(GLuint id, GLenum pname, GLuint64* params);

struct GlDispatch {
    PFN_glGetIntegerv GetIntegerv = nullptr;
    PFN_glGetString GetString = nullptr;
    PFN_glViewport Viewport = nullptr;
    PFN_glClearColor ClearColor = nullptr;
    PFN_glClearDepth ClearDepth = nullptr;
    PFN_glClearStencil ClearStencil = nullptr;
    PFN_glClear Clear = nullptr;
    PFN_glEnable Enable = nullptr;
    PFN_glDisable Disable = nullptr;
    PFN_glDepthMask DepthMask = nullptr;
    PFN_glDepthFunc DepthFunc = nullptr;
    PFN_glBlendFunc BlendFunc = nullptr;
    PFN_glBlendFuncSeparate BlendFuncSeparate = nullptr;
    PFN_glColorMask ColorMask = nullptr;
    PFN_glStencilMask StencilMask = nullptr;
    PFN_glStencilFunc StencilFunc = nullptr;
    PFN_glStencilOp StencilOp = nullptr;
    PFN_glStencilOpSeparate StencilOpSeparate = nullptr;
    PFN_glPixelStorei PixelStorei = nullptr;
    PFN_glFinish Finish = nullptr;
    PFN_glFlush Flush = nullptr;

    PFN_glCreateFramebuffers CreateFramebuffers = nullptr;
    PFN_glDeleteFramebuffers DeleteFramebuffers = nullptr;
    PFN_glBindFramebuffer BindFramebuffer = nullptr;
    PFN_glNamedFramebufferTexture NamedFramebufferTexture = nullptr;
    PFN_glNamedFramebufferRenderbuffer NamedFramebufferRenderbuffer = nullptr;
    PFN_glCheckNamedFramebufferStatus CheckNamedFramebufferStatus = nullptr;
    PFN_glBlitNamedFramebuffer BlitNamedFramebuffer = nullptr;

    PFN_glCreateRenderbuffers CreateRenderbuffers = nullptr;
    PFN_glDeleteRenderbuffers DeleteRenderbuffers = nullptr;
    PFN_glNamedRenderbufferStorageMultisample NamedRenderbufferStorageMultisample = nullptr;

    PFN_glCreateTextures CreateTextures = nullptr;
    PFN_glDeleteTextures DeleteTextures = nullptr;
    PFN_glTextureStorage2D TextureStorage2D = nullptr;
    PFN_glTextureSubImage2D TextureSubImage2D = nullptr;
    PFN_glTextureParameteri TextureParameteri = nullptr;
    PFN_glBindTextureUnit BindTextureUnit = nullptr;
    PFN_glGetTextureImage GetTextureImage = nullptr;

    PFN_glCreateBuffers CreateBuffers = nullptr;
    PFN_glDeleteBuffers DeleteBuffers = nullptr;
    PFN_glNamedBufferData NamedBufferData = nullptr;
    PFN_glNamedBufferSubData NamedBufferSubData = nullptr;

    PFN_glCreateVertexArrays CreateVertexArrays = nullptr;
    PFN_glDeleteVertexArrays DeleteVertexArrays = nullptr;
    PFN_glBindVertexArray BindVertexArray = nullptr;
    PFN_glVertexArrayVertexBuffer VertexArrayVertexBuffer = nullptr;
    PFN_glVertexArrayElementBuffer VertexArrayElementBuffer = nullptr;
    PFN_glEnableVertexArrayAttrib EnableVertexArrayAttrib = nullptr;
    PFN_glVertexArrayAttribFormat VertexArrayAttribFormat = nullptr;
    PFN_glVertexArrayAttribBinding VertexArrayAttribBinding = nullptr;
    PFN_glVertexArrayBindingDivisor VertexArrayBindingDivisor = nullptr;

    PFN_glCreateShader CreateShader = nullptr;
    PFN_glDeleteShader DeleteShader = nullptr;
    PFN_glShaderSource ShaderSource = nullptr;
    PFN_glCompileShader CompileShader = nullptr;
    PFN_glGetShaderiv GetShaderiv = nullptr;
    PFN_glGetShaderInfoLog GetShaderInfoLog = nullptr;
    PFN_glCreateProgram CreateProgram = nullptr;
    PFN_glDeleteProgram DeleteProgram = nullptr;
    PFN_glAttachShader AttachShader = nullptr;
    PFN_glLinkProgram LinkProgram = nullptr;
    PFN_glGetProgramiv GetProgramiv = nullptr;
    PFN_glGetProgramInfoLog GetProgramInfoLog = nullptr;
    PFN_glUseProgram UseProgram = nullptr;
    PFN_glGetUniformLocation GetUniformLocation = nullptr;
    PFN_glUniform1i Uniform1i = nullptr;
    PFN_glUniform1f Uniform1f = nullptr;
    PFN_glUniform2f Uniform2f = nullptr;
    PFN_glUniform4f Uniform4f = nullptr;

    PFN_glDrawArrays DrawArrays = nullptr;
    PFN_glDrawArraysInstanced DrawArraysInstanced = nullptr;
    PFN_glDrawElements DrawElements = nullptr;

    PFN_glGenQueries GenQueries = nullptr;
    PFN_glDeleteQueries DeleteQueries = nullptr;
    PFN_glBeginQuery BeginQuery = nullptr;
    PFN_glEndQuery EndQuery = nullptr;
    PFN_glGetQueryObjectiv GetQueryObjectiv = nullptr;
    PFN_glGetQueryObjectui64v GetQueryObjectui64v = nullptr;
};

using GlGetProcAddressFn = void* (*)(const char* name);

// Loads all OpenGL 4.5 Core DSA + timer query entrypoints and verifies version >= 4.5.
[[nodiscard]] Result<GlDispatch> load_gl45(GlGetProcAddressFn get_proc);

}  // namespace stem::gl
