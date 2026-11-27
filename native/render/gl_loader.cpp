#include "native/render/gl_loader.hpp"

#include <format>

namespace stem::gl {

namespace {

template <typename FnPtr>
bool resolve_fn(GlGetProcAddressFn get_proc, const char* name, FnPtr& out_fn, std::string& missing) {
    void* p = get_proc(name);
#if defined(_WIN32)
    // OpenGL 1.1 entrypoints on Windows may reside directly in opengl32.dll rather than wglGetProcAddress.
    if (!p) {
        static HMODULE ogl32 = GetModuleHandleA("opengl32.dll");
        if (ogl32) {
            p = reinterpret_cast<void*>(GetProcAddress(ogl32, name));
        }
    }
#endif
    if (!p) {
        if (!missing.empty()) missing += ", ";
        missing += name;
        return false;
    }
    out_fn = reinterpret_cast<FnPtr>(p);
    return true;
}

}  // namespace

Result<GlDispatch> load_gl45(GlGetProcAddressFn get_proc) {
    if (!get_proc) {
        return fail("Null OpenGL GetProcAddress callback");
    }

    GlDispatch gl{};
    std::string missing;

    resolve_fn(get_proc, "glGetIntegerv", gl.GetIntegerv, missing);
    resolve_fn(get_proc, "glGetString", gl.GetString, missing);
    resolve_fn(get_proc, "glViewport", gl.Viewport, missing);
    resolve_fn(get_proc, "glClearColor", gl.ClearColor, missing);
    resolve_fn(get_proc, "glClearDepth", gl.ClearDepth, missing);
    resolve_fn(get_proc, "glClearStencil", gl.ClearStencil, missing);
    resolve_fn(get_proc, "glClear", gl.Clear, missing);
    resolve_fn(get_proc, "glEnable", gl.Enable, missing);
    resolve_fn(get_proc, "glDisable", gl.Disable, missing);
    resolve_fn(get_proc, "glDepthMask", gl.DepthMask, missing);
    resolve_fn(get_proc, "glDepthFunc", gl.DepthFunc, missing);
    resolve_fn(get_proc, "glBlendFunc", gl.BlendFunc, missing);
    resolve_fn(get_proc, "glBlendFuncSeparate", gl.BlendFuncSeparate, missing);
    resolve_fn(get_proc, "glColorMask", gl.ColorMask, missing);
    resolve_fn(get_proc, "glStencilMask", gl.StencilMask, missing);
    resolve_fn(get_proc, "glStencilFunc", gl.StencilFunc, missing);
    resolve_fn(get_proc, "glStencilOp", gl.StencilOp, missing);
    resolve_fn(get_proc, "glStencilOpSeparate", gl.StencilOpSeparate, missing);
    resolve_fn(get_proc, "glPixelStorei", gl.PixelStorei, missing);
    resolve_fn(get_proc, "glFinish", gl.Finish, missing);
    resolve_fn(get_proc, "glFlush", gl.Flush, missing);

    resolve_fn(get_proc, "glCreateFramebuffers", gl.CreateFramebuffers, missing);
    resolve_fn(get_proc, "glDeleteFramebuffers", gl.DeleteFramebuffers, missing);
    resolve_fn(get_proc, "glBindFramebuffer", gl.BindFramebuffer, missing);
    resolve_fn(get_proc, "glNamedFramebufferTexture", gl.NamedFramebufferTexture, missing);
    resolve_fn(get_proc, "glNamedFramebufferRenderbuffer", gl.NamedFramebufferRenderbuffer, missing);
    resolve_fn(get_proc, "glCheckNamedFramebufferStatus", gl.CheckNamedFramebufferStatus, missing);
    resolve_fn(get_proc, "glBlitNamedFramebuffer", gl.BlitNamedFramebuffer, missing);

    resolve_fn(get_proc, "glCreateRenderbuffers", gl.CreateRenderbuffers, missing);
    resolve_fn(get_proc, "glDeleteRenderbuffers", gl.DeleteRenderbuffers, missing);
    resolve_fn(get_proc, "glNamedRenderbufferStorageMultisample", gl.NamedRenderbufferStorageMultisample, missing);

    resolve_fn(get_proc, "glCreateTextures", gl.CreateTextures, missing);
    resolve_fn(get_proc, "glDeleteTextures", gl.DeleteTextures, missing);
    resolve_fn(get_proc, "glTextureStorage2D", gl.TextureStorage2D, missing);
    resolve_fn(get_proc, "glTextureSubImage2D", gl.TextureSubImage2D, missing);
    resolve_fn(get_proc, "glTextureParameteri", gl.TextureParameteri, missing);
    resolve_fn(get_proc, "glBindTextureUnit", gl.BindTextureUnit, missing);
    resolve_fn(get_proc, "glGetTextureImage", gl.GetTextureImage, missing);

    resolve_fn(get_proc, "glCreateBuffers", gl.CreateBuffers, missing);
    resolve_fn(get_proc, "glDeleteBuffers", gl.DeleteBuffers, missing);
    resolve_fn(get_proc, "glNamedBufferData", gl.NamedBufferData, missing);
    resolve_fn(get_proc, "glNamedBufferSubData", gl.NamedBufferSubData, missing);

    resolve_fn(get_proc, "glCreateVertexArrays", gl.CreateVertexArrays, missing);
    resolve_fn(get_proc, "glDeleteVertexArrays", gl.DeleteVertexArrays, missing);
    resolve_fn(get_proc, "glBindVertexArray", gl.BindVertexArray, missing);
    resolve_fn(get_proc, "glVertexArrayVertexBuffer", gl.VertexArrayVertexBuffer, missing);
    resolve_fn(get_proc, "glVertexArrayElementBuffer", gl.VertexArrayElementBuffer, missing);
    resolve_fn(get_proc, "glEnableVertexArrayAttrib", gl.EnableVertexArrayAttrib, missing);
    resolve_fn(get_proc, "glVertexArrayAttribFormat", gl.VertexArrayAttribFormat, missing);
    resolve_fn(get_proc, "glVertexArrayAttribBinding", gl.VertexArrayAttribBinding, missing);
    resolve_fn(get_proc, "glVertexArrayBindingDivisor", gl.VertexArrayBindingDivisor, missing);

    resolve_fn(get_proc, "glCreateShader", gl.CreateShader, missing);
    resolve_fn(get_proc, "glDeleteShader", gl.DeleteShader, missing);
    resolve_fn(get_proc, "glShaderSource", gl.ShaderSource, missing);
    resolve_fn(get_proc, "glCompileShader", gl.CompileShader, missing);
    resolve_fn(get_proc, "glGetShaderiv", gl.GetShaderiv, missing);
    resolve_fn(get_proc, "glGetShaderInfoLog", gl.GetShaderInfoLog, missing);
    resolve_fn(get_proc, "glCreateProgram", gl.CreateProgram, missing);
    resolve_fn(get_proc, "glDeleteProgram", gl.DeleteProgram, missing);
    resolve_fn(get_proc, "glAttachShader", gl.AttachShader, missing);
    resolve_fn(get_proc, "glLinkProgram", gl.LinkProgram, missing);
    resolve_fn(get_proc, "glGetProgramiv", gl.GetProgramiv, missing);
    resolve_fn(get_proc, "glGetProgramInfoLog", gl.GetProgramInfoLog, missing);
    resolve_fn(get_proc, "glUseProgram", gl.UseProgram, missing);
    resolve_fn(get_proc, "glGetUniformLocation", gl.GetUniformLocation, missing);
    resolve_fn(get_proc, "glUniform1i", gl.Uniform1i, missing);
    resolve_fn(get_proc, "glUniform1f", gl.Uniform1f, missing);
    resolve_fn(get_proc, "glUniform2f", gl.Uniform2f, missing);
    resolve_fn(get_proc, "glUniform4f", gl.Uniform4f, missing);

    resolve_fn(get_proc, "glDrawArrays", gl.DrawArrays, missing);
    resolve_fn(get_proc, "glDrawArraysInstanced", gl.DrawArraysInstanced, missing);
    resolve_fn(get_proc, "glDrawElements", gl.DrawElements, missing);

    resolve_fn(get_proc, "glGenQueries", gl.GenQueries, missing);
    resolve_fn(get_proc, "glDeleteQueries", gl.DeleteQueries, missing);
    resolve_fn(get_proc, "glBeginQuery", gl.BeginQuery, missing);
    resolve_fn(get_proc, "glEndQuery", gl.EndQuery, missing);
    resolve_fn(get_proc, "glGetQueryObjectiv", gl.GetQueryObjectiv, missing);
    resolve_fn(get_proc, "glGetQueryObjectui64v", gl.GetQueryObjectui64v, missing);

    if (!missing.empty()) {
        return fail(std::format("Missing OpenGL 4.5 functions: {}", missing));
    }

    GLint maj = 0;
    GLint min = 0;
    gl.GetIntegerv(kMajorVersion, &maj);
    gl.GetIntegerv(kMinorVersion, &min);
    if (maj < 4 || (maj == 4 && min < 5)) {
        return fail(std::format("OpenGL 4.5 Core required, got {}.{}", maj, min));
    }
    return gl;
}

}  // namespace stem::gl
