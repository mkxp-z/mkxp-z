/*
 ** gl-fun.cpp
 **
 ** This file is part of mkxp.
 **
 ** Copyright (C) 2014 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
 **
 ** mkxp is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** mkxp is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gl-fun.h"

#include "boost-hash.h"
#include "exception.h"
#include "graphics.h"
#include "sharedstate.h"

#include <string>
#include <utility>

#ifdef MKXPZ_HAVE_ANGLE
extern bool mkxp_use_angle;
#endif // MKXPZ_HAVE_ANGLE

GLFunctions gl;

typedef const GLubyte* (APIENTRYP _PFNGLGETSTRINGIPROC) (GLenum, GLuint);

static void parseExtensionsCore(_PFNGLGETSTRINGIPROC GetStringi, _PFNGLGETINTEGERVPROC GetIntegerv, BoostSet<std::string> &out)
{
    GLint extCount = 0;
    GetIntegerv(GL_NUM_EXTENSIONS, &extCount);
    
    for (GLint i = 0; i < extCount; ++i)
        out.insert((const char*) GetStringi(GL_EXTENSIONS, i));
}

static void parseExtensionsCompat(_PFNGLGETSTRINGPROC GetString, BoostSet<std::string> &out)
{
    const char *ext = (const char*) GetString(GL_EXTENSIONS);
    
    if (!ext)
        return;
    
    char buffer[0x100];
    size_t bufferI;
    
    while (*ext)
    {
        bufferI = 0;
        while (*ext && *ext != ' ')
            buffer[bufferI++] = *ext++;
        
        buffer[bufferI] = '\0';
        
        out.insert(buffer);
        
        if (*ext == ' ')
            ++ext;
    }
}

template <typename Command, typename = void> struct CommandResult
{
    static void get(const Command &command) noexcept
    {
    }
    template <typename Function, typename... Args> static void set(Command &command, Function function, Args... args) noexcept
    {
        function(args...);
    }
};
template <typename Command> struct CommandResult<Command, decltype(std::declval<Command>().result, void())>
{
    static decltype(std::declval<Command>().result) get(const Command &command) noexcept
    {
        return command.result;
    }
    template <typename Function, typename... Args> static void set(Command &command, Function function, Args... args) noexcept
    {
        command.result = function(args...);
    }
};

#define EXECUTE_COMMAND(name, ...) do { \
    if (shState != nullptr && !shState->graphics().isLocked()) { \
        throw Exception(Exception::MKXPError, "Attempted to call gl" #name " without locking the graphics state"); \
    } \
    if (!gl.multithreaded && SDL_GL_GetCurrentContext() == nullptr) { \
        throw Exception(Exception::MKXPError, "Cannot call this function from outside of the graphics thread when Graphics.thread_safe == false"); \
    } \
    if (gl.context_release_behavior_none || !gl.multithreaded) { \
        return gl._impl_##name(__VA_ARGS__); \
    } \
    std::unique_lock<std::mutex> guard(gl.mutex); \
    Command##name command {__VA_ARGS__}; \
    gl.commandId = command.commandId; \
    gl.command = &command; \
    gl.cond.notify_one(); \
    do { \
        gl.cond.wait(guard); \
    } while (gl.commandId != 0); \
    return CommandResult<Command##name>::get(command); \
} while (0)

static constexpr size_t startingCommandId = __COUNTER__;

#define DECLARE_COMMAND_ID static constexpr size_t commandId = (size_t)__COUNTER__ - startingCommandId

struct CommandGetError
{
    DECLARE_COMMAND_ID;
    GLenum result;
};
static GLenum commandGetError(void)
{
    EXECUTE_COMMAND(GetError);
}

struct CommandClearColor
{
    DECLARE_COMMAND_ID;
    GLclampf red;
    GLclampf green;
    GLclampf blue;
    GLclampf alpha;
};
static void commandClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
    EXECUTE_COMMAND(ClearColor, red, green, blue, alpha);
}

struct CommandClear
{
    DECLARE_COMMAND_ID;
    GLbitfield mask;
};
static void commandClear(GLbitfield mask)
{
    EXECUTE_COMMAND(Clear, mask);
}

struct CommandGetString
{
    DECLARE_COMMAND_ID;
    GLenum name;
    const GLubyte *result;
};
static const GLubyte *commandGetString(GLenum name)
{
    EXECUTE_COMMAND(GetString, name);
}

struct CommandGetStringi
{
    DECLARE_COMMAND_ID;
    GLenum name;
    GLuint index;
    const GLubyte *result;
};
static const GLubyte *commandGetStringi(GLenum name, GLuint index)
{
    EXECUTE_COMMAND(GetStringi, name, index);
}

struct CommandGetIntegerv
{
    DECLARE_COMMAND_ID;
    GLenum pname;
    GLint *params;
};
static void commandGetIntegerv(GLenum pname, GLint *params)
{
    EXECUTE_COMMAND(GetIntegerv, pname, params);
}

struct CommandPixelStorei
{
    DECLARE_COMMAND_ID;
    GLenum pname;
    GLint param;
};
static void commandPixelStorei(GLenum pname, GLint param)
{
    EXECUTE_COMMAND(PixelStorei, pname, param);
}

struct CommandReadPixels
{
    DECLARE_COMMAND_ID;
    GLint x;
    GLint y;
    GLsizei width;
    GLsizei height;
    GLenum format;
    GLenum type;
    GLvoid *pixels;
};
static void commandReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels)
{
    EXECUTE_COMMAND(ReadPixels, x, y, width, height, format, type, pixels);
}

struct CommandEnable
{
    DECLARE_COMMAND_ID;
    GLenum cap;
};
static void commandEnable(GLenum cap)
{
    EXECUTE_COMMAND(Enable, cap);
}

struct CommandDisable
{
    DECLARE_COMMAND_ID;
    GLenum cap;
};
static void commandDisable(GLenum cap)
{
    EXECUTE_COMMAND(Disable, cap);
}

struct CommandScissor
{
    DECLARE_COMMAND_ID;
    GLint x;
    GLint y;
    GLsizei width;
    GLsizei height;
};
static void commandScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    EXECUTE_COMMAND(Scissor, x, y, width, height);
}

struct CommandViewport
{
    DECLARE_COMMAND_ID;
    GLint x;
    GLint y;
    GLsizei width;
    GLsizei height;
};
static void commandViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    EXECUTE_COMMAND(Viewport, x, y, width, height);
}

struct CommandBlendFunc
{
    DECLARE_COMMAND_ID;
    GLenum sfactor;
    GLenum dfactor;
};
static void commandBlendFunc(GLenum sfactor, GLenum dfactor)
{
    EXECUTE_COMMAND(BlendFunc, sfactor, dfactor);
}

struct CommandBlendFuncSeparate
{
    DECLARE_COMMAND_ID;
    GLenum sfactorRGB;
    GLenum dfactorRGB;
    GLenum sfactorAlpha;
    GLenum dfactorAlpha;
};
static void commandBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha)
{
    EXECUTE_COMMAND(BlendFuncSeparate, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}

struct CommandBlendEquation
{
    DECLARE_COMMAND_ID;
    GLenum mode;
};
static void commandBlendEquation(GLenum mode)
{
    EXECUTE_COMMAND(BlendEquation, mode);
}

struct CommandDrawElements
{
    DECLARE_COMMAND_ID;
    GLenum mode;
    GLsizei count;
    GLenum type;
    const GLvoid *indices;
};
static void commandDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    EXECUTE_COMMAND(DrawElements, mode, count, type, indices);
}

struct CommandGenTextures
{
    DECLARE_COMMAND_ID;
    GLsizei n;
    GLuint *textures;
};
static void commandGenTextures(GLsizei n, GLuint *textures)
{
    EXECUTE_COMMAND(GenTextures, n, textures);
}

struct CommandDeleteTextures
{
    DECLARE_COMMAND_ID;
    GLsizei n;
    const GLuint *textures;
};
static void commandDeleteTextures(GLsizei n, const GLuint *textures)
{
    EXECUTE_COMMAND(DeleteTextures, n, textures);
}

struct CommandBindTexture
{
    DECLARE_COMMAND_ID;
    GLenum target;
    GLuint texture;
};
static void commandBindTexture(GLenum target, GLuint texture)
{
    EXECUTE_COMMAND(BindTexture, target, texture);
}

struct CommandTexImage2D
{
    DECLARE_COMMAND_ID;
    GLenum target;
    GLint level;
    GLint internalformat;
    GLsizei width;
    GLsizei height;
    GLint border;
    GLenum format;
    GLenum type;
    const GLvoid *pixels;
};
static void commandTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
    EXECUTE_COMMAND(TexImage2D, target, level, internalformat, width, height, border, format, type, pixels);
}

struct CommandTexSubImage2D
{
    DECLARE_COMMAND_ID;
    GLenum target;
    GLint level;
    GLint xoffset;
    GLint yoffset;
    GLsizei width;
    GLsizei height;
    GLenum format;
    GLenum type;
    const GLvoid *pixels;
};
static void commandTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
    EXECUTE_COMMAND(TexSubImage2D, target, level, xoffset, yoffset, width, height, format, type, pixels);
}

struct CommandTexParameteri
{
    DECLARE_COMMAND_ID;
    GLenum target;
    GLenum pname;
    GLint param;
};
static void commandTexParameteri(GLenum target, GLenum pname, GLint param)
{
    EXECUTE_COMMAND(TexParameteri, target, pname, param);
}

struct CommandActiveTexture
{
    DECLARE_COMMAND_ID;
    GLenum texture;
};
static void commandActiveTexture(GLenum texture)
{
    EXECUTE_COMMAND(ActiveTexture, texture);
}

struct CommandGenerateMipmap
{
    DECLARE_COMMAND_ID;
    GLenum target;
};
static void commandGenerateMipmap(GLenum target)
{
    EXECUTE_COMMAND(GenerateMipmap, target);
}

struct CommandGenerateTextureMipmap
{
    DECLARE_COMMAND_ID;
    GLuint texture;
};
static void commandGenerateTextureMipmap(GLuint texture)
{
    EXECUTE_COMMAND(GenerateTextureMipmap, texture);
}

struct CommandDebugMessageCallback
{
    DECLARE_COMMAND_ID;
    _GLDEBUGPROC callback;
    const void *userParam;
};
static void commandDebugMessageCallback(_GLDEBUGPROC callback, const void *userParam)
{
    EXECUTE_COMMAND(DebugMessageCallback, callback, userParam);
}

struct CommandStringMarker
{
    DECLARE_COMMAND_ID;
    GLsizei len;
    const GLvoid *string;
};
static void commandStringMarker(GLsizei len, const GLvoid *string)
{
    EXECUTE_COMMAND(StringMarker, len, string);
}

struct CommandGenBuffers
{
    DECLARE_COMMAND_ID;
    GLsizei n;
    GLuint *buffers;
};
static void commandGenBuffers(GLsizei n, GLuint *buffers)
{
    EXECUTE_COMMAND(GenBuffers, n, buffers);
}

struct CommandDeleteBuffers
{
    DECLARE_COMMAND_ID;
    GLsizei n;
    const GLuint *buffers;
};
static void commandDeleteBuffers(GLsizei n, const GLuint *buffers)
{
    EXECUTE_COMMAND(DeleteBuffers, n, buffers);
}

struct CommandBindBuffer
{
    DECLARE_COMMAND_ID;
    GLenum target;
    GLuint buffer;
};
static void commandBindBuffer(GLenum target, GLuint buffer)
{
    EXECUTE_COMMAND(BindBuffer, target, buffer);
}

struct CommandBufferData
{
    DECLARE_COMMAND_ID;
    GLenum target;
    GLsizeiptr size;
    const GLvoid *data;
    GLenum usage;
};
static void commandBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage)
{
    EXECUTE_COMMAND(BufferData, target, size, data, usage);
}

struct CommandBufferSubData
{
    DECLARE_COMMAND_ID;
    GLenum target;
    GLintptr offset;
    GLsizeiptr size;
    const GLvoid *data;
};
static void commandBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data)
{
    EXECUTE_COMMAND(BufferSubData, target, offset, size, data);
}

struct CommandCreateShader
{
    DECLARE_COMMAND_ID;
    GLenum type;
    GLuint result;
};
static GLuint commandCreateShader(GLenum type)
{
    EXECUTE_COMMAND(CreateShader, type);
}

struct CommandDeleteShader
{
    DECLARE_COMMAND_ID;
    GLuint shader;
};
static void commandDeleteShader(GLuint shader)
{
    EXECUTE_COMMAND(DeleteShader, shader);
}

struct CommandShaderSource
{
    DECLARE_COMMAND_ID;
    GLuint shader;
    GLsizei count;
    const GLchar *const *strings;
    const GLint *lengths;
};
static void commandShaderSource(GLuint shader, GLsizei count, const GLchar *const *strings, const GLint *lengths)
{
    EXECUTE_COMMAND(ShaderSource, shader, count, strings, lengths);
}

struct CommandCompileShader
{
    DECLARE_COMMAND_ID;
    GLuint shader;
};
static void commandCompileShader(GLuint shader)
{
    EXECUTE_COMMAND(CompileShader, shader);
}

struct CommandAttachShader
{
    DECLARE_COMMAND_ID;
    GLuint program;
    GLuint shader;
};
static void commandAttachShader(GLuint program, GLuint shader)
{
    EXECUTE_COMMAND(AttachShader, program, shader);
}

struct CommandGetShaderiv
{
    DECLARE_COMMAND_ID;
    GLuint shader;
    GLenum pname;
    GLint *param;
};
static void commandGetShaderiv(GLuint shader, GLenum pname, GLint *param)
{
    EXECUTE_COMMAND(GetShaderiv, shader, pname, param);
}

struct CommandGetShaderInfoLog
{
    DECLARE_COMMAND_ID;
    GLuint shader;
    GLsizei bufSize;
    GLsizei *length;
    GLchar *infoLog;
};
static void commandGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
    EXECUTE_COMMAND(GetShaderInfoLog, shader, bufSize, length, infoLog);
}

struct CommandCreateProgram
{
    DECLARE_COMMAND_ID;
    GLuint result;
};
static GLuint commandCreateProgram(void)
{
    EXECUTE_COMMAND(CreateProgram);
}

struct CommandDeleteProgram
{
    DECLARE_COMMAND_ID;
    GLuint program;
};
static void commandDeleteProgram(GLuint program)
{
    EXECUTE_COMMAND(DeleteProgram, program);
}

struct CommandUseProgram
{
    DECLARE_COMMAND_ID;
    GLuint program;
};
static void commandUseProgram(GLuint program)
{
    EXECUTE_COMMAND(UseProgram, program);
}

struct CommandLinkProgram
{
    DECLARE_COMMAND_ID;
    GLuint program;
};
static void commandLinkProgram(GLuint program)
{
    EXECUTE_COMMAND(LinkProgram, program);
}

struct CommandGetProgramiv
{
    DECLARE_COMMAND_ID;
    GLuint program;
    GLenum pname;
    GLint *param;
};
static void commandGetProgramiv(GLuint program, GLenum pname, GLint *param)
{
    EXECUTE_COMMAND(GetProgramiv, program, pname, param);
}

struct CommandGetProgramInfoLog
{
    DECLARE_COMMAND_ID;
    GLuint program;
    GLsizei bufSize;
    GLsizei *length;
    GLchar *infoLog;
};
static void commandGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
    EXECUTE_COMMAND(GetProgramInfoLog, program, bufSize, length, infoLog);
}

struct CommandGetUniformLocation
{
    DECLARE_COMMAND_ID;
    GLuint program;
    const GLchar *name;
    GLint result;
};
static GLint commandGetUniformLocation(GLuint program, const GLchar *name)
{
    EXECUTE_COMMAND(GetUniformLocation, program, name);
}

struct CommandUniform1f
{
    DECLARE_COMMAND_ID;
    GLint location;
    GLfloat v0;
};
static void commandUniform1f(GLint location, GLfloat v0)
{
    EXECUTE_COMMAND(Uniform1f, location, v0);
}

struct CommandUniform2f
{
    DECLARE_COMMAND_ID;
    GLint location;
    GLfloat v0;
    GLfloat v1;
};
static void commandUniform2f(GLint location, GLfloat v0, GLfloat v1)
{
    EXECUTE_COMMAND(Uniform2f, location, v0, v1);
}

struct CommandUniform4f
{
    DECLARE_COMMAND_ID;
    GLint location;
    GLfloat v0;
    GLfloat v1;
    GLfloat v2;
    GLfloat v3;
};
static void commandUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
    EXECUTE_COMMAND(Uniform4f, location, v0, v1, v2, v3);
}

struct CommandUniform1i
{
    DECLARE_COMMAND_ID;
    GLint location;
    GLint v0;
};
static void commandUniform1i(GLint location, GLint v0)
{
    EXECUTE_COMMAND(Uniform1i, location, v0);
}

struct CommandUniform1iv
{
    DECLARE_COMMAND_ID;
    GLint location;
    GLsizei count;
    const GLint *value;
};
static void commandUniform1iv(GLint location, GLsizei count, const GLint *value)
{
    EXECUTE_COMMAND(Uniform1iv, location, count, value);
}

struct CommandUniformMatrix4fv
{
    DECLARE_COMMAND_ID;
    GLint location;
    GLsizei count;
    GLboolean transpose;
    const GLfloat *value;
};
static void commandUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    EXECUTE_COMMAND(UniformMatrix4fv, location, count, transpose, value);
}

struct CommandBindAttribLocation
{
    DECLARE_COMMAND_ID;
    GLuint program;
    GLuint index;
    const GLchar *name;
};
static void commandBindAttribLocation(GLuint program, GLuint index, const GLchar *name)
{
    EXECUTE_COMMAND(BindAttribLocation, program, index, name);
}

struct CommandEnableVertexAttribArray
{
    DECLARE_COMMAND_ID;
    GLuint index;
};
static void commandEnableVertexAttribArray(GLuint index)
{
    EXECUTE_COMMAND(EnableVertexAttribArray, index);
}

struct CommandDisableVertexAttribArray
{
    DECLARE_COMMAND_ID;
    GLuint index;
};
static void commandDisableVertexAttribArray(GLuint index)
{
    EXECUTE_COMMAND(DisableVertexAttribArray, index);
}

struct CommandVertexAttribPointer
{
    DECLARE_COMMAND_ID;
    GLuint index;
    GLint size;
    GLenum type;
    GLboolean normalized;
    GLsizei stride;
    const GLvoid *pointer;
};
static void commandVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer)
{
    EXECUTE_COMMAND(VertexAttribPointer, index, size, type, normalized, stride, pointer);
}

struct CommandGenFramebuffers
{
    DECLARE_COMMAND_ID;
    GLsizei n;
    GLuint *framebuffers;
};
static void commandGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
    EXECUTE_COMMAND(GenFramebuffers, n, framebuffers);
}

struct CommandDeleteFramebuffers
{
    DECLARE_COMMAND_ID;
    GLsizei n;
    const GLuint *framebuffers;
};
static void commandDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
    EXECUTE_COMMAND(DeleteFramebuffers, n, framebuffers);
}

struct CommandBindFramebuffer
{
    DECLARE_COMMAND_ID;
    GLenum target;
    GLuint framebuffer;
};
static void commandBindFramebuffer(GLenum target, GLuint framebuffer)
{
    EXECUTE_COMMAND(BindFramebuffer, target, framebuffer);
}

struct CommandFramebufferTexture2D
{
    DECLARE_COMMAND_ID;
    GLenum target;
    GLenum attachment;
    GLenum textarget;
    GLuint texture;
    GLint level;
};
static void commandFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    EXECUTE_COMMAND(FramebufferTexture2D, target, attachment, textarget, texture, level);
}

struct CommandBlitFramebuffer
{
    DECLARE_COMMAND_ID;
    GLint srcX0;
    GLint srcY0;
    GLint srcX1;
    GLint srcY1;
    GLint dstX0;
    GLint dstY0;
    GLint dstX1;
    GLint dstY1;
    GLbitfield mask;
    GLenum filter;
};
static void commandBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    EXECUTE_COMMAND(BlitFramebuffer, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

struct CommandGenVertexArrays
{
    DECLARE_COMMAND_ID;
    GLsizei n;
    GLuint *arrays;
};
static void commandGenVertexArrays(GLsizei n, GLuint *arrays)
{
    EXECUTE_COMMAND(GenVertexArrays, n, arrays);
}

struct CommandDeleteVertexArrays
{
    DECLARE_COMMAND_ID;
    GLsizei n;
    const GLuint *arrays;
};
static void commandDeleteVertexArrays(GLsizei n, const GLuint *arrays)
{
    EXECUTE_COMMAND(DeleteVertexArrays, n, arrays);
}

struct CommandBindVertexArray
{
    DECLARE_COMMAND_ID;
    GLuint array;
};
static void commandBindVertexArray(GLuint array)
{
    EXECUTE_COMMAND(BindVertexArray, array);
}

struct CommandReleaseShaderCompiler
{
    DECLARE_COMMAND_ID;
};
static void commandReleaseShaderCompiler(void)
{
    EXECUTE_COMMAND(ReleaseShaderCompiler);
}

struct CommandGetProcAddress
{
    DECLARE_COMMAND_ID;
    const char *proc;
    void *result;
};
static void *commandGetProcAddress(const char *proc)
{
    EXECUTE_COMMAND(GetProcAddress, proc);
}

struct CommandMakeCurrent
{
    DECLARE_COMMAND_ID;
    SDL_Window *window;
    SDL_GLContext context;
    const char *result;
};
static const char *commandMakeCurrent(SDL_Window *window, SDL_GLContext context)
{
    EXECUTE_COMMAND(MakeCurrent, window, context);
}

struct CommandSetSwapInterval
{
    DECLARE_COMMAND_ID;
    int interval;
    int result;
};
static int commandSetSwapInterval(int interval)
{
    EXECUTE_COMMAND(SetSwapInterval, interval);
}

struct CommandGetSwapInterval
{
    DECLARE_COMMAND_ID;
    int result;
};
static int commandGetSwapInterval(void)
{
    EXECUTE_COMMAND(GetSwapInterval);
}

struct CommandSwapWindow
{
    DECLARE_COMMAND_ID;
    SDL_Window *window;
};
static void commandSwapWindow(SDL_Window *window)
{
    EXECUTE_COMMAND(SwapWindow, window);
}

#define GL_FUN(name, type) \
    gl.name = (gl._impl_##name = (type)gl.GetProcAddress("gl" #name EXT_SUFFIX)) == nullptr ? nullptr : command##name;

#define EXC(msg) \
Exception(Exception::MKXPError, "%s", msg)

#define DEF_COMMAND_HANDLER(name, ...) []() { \
    static_assert(Command##name::commandId == (size_t)__COUNTER__ - startingCommandHandlerId, "command handlers need to be defined in the same order as the commands themselves"); \
    Command##name &command = *(Command##name *)gl.command; \
    CommandResult<Command##name>::set(command, gl._impl_##name, __VA_ARGS__); \
}

#define DEF_COMMAND_HANDLER_NO_ARGS(name) []() { \
    static_assert(Command##name::commandId == (size_t)__COUNTER__ - startingCommandHandlerId, "command handlers need to be defined in the same order as the commands themselves"); \
    Command##name &command = *(Command##name *)gl.command; \
    CommandResult<Command##name>::set(command, gl._impl_##name); \
}

int glThreadFun(void *userdata)
{
    static constexpr size_t startingCommandHandlerId = __COUNTER__;
    static void (*const handlers[])(void) = {
        DEF_COMMAND_HANDLER_NO_ARGS(GetError),
        DEF_COMMAND_HANDLER(ClearColor, command.red, command.green, command.blue, command.alpha),
        DEF_COMMAND_HANDLER(Clear, command.mask),
        DEF_COMMAND_HANDLER(GetString, command.name),
        DEF_COMMAND_HANDLER(GetStringi, command.name, command.index),
        DEF_COMMAND_HANDLER(GetIntegerv, command.pname, command.params),
        DEF_COMMAND_HANDLER(PixelStorei, command.pname, command.param),
        DEF_COMMAND_HANDLER(ReadPixels, command.x, command.y, command.width, command.height, command.format, command.type, command.pixels),
        DEF_COMMAND_HANDLER(Enable, command.cap),
        DEF_COMMAND_HANDLER(Disable, command.cap),
        DEF_COMMAND_HANDLER(Scissor, command.x, command.y, command.width, command.height),
        DEF_COMMAND_HANDLER(Viewport, command.x, command.y, command.width, command.height),
        DEF_COMMAND_HANDLER(BlendFunc, command.sfactor, command.dfactor),
        DEF_COMMAND_HANDLER(BlendFuncSeparate, command.sfactorRGB, command.dfactorRGB, command.sfactorAlpha, command.dfactorAlpha),
        DEF_COMMAND_HANDLER(BlendEquation, command.mode),
        DEF_COMMAND_HANDLER(DrawElements, command.mode, command.count, command.type, command.indices),
        DEF_COMMAND_HANDLER(GenTextures, command.n, command.textures),
        DEF_COMMAND_HANDLER(DeleteTextures, command.n, command.textures),
        DEF_COMMAND_HANDLER(BindTexture, command.target, command.texture),
        DEF_COMMAND_HANDLER(TexImage2D, command.target, command.level, command.internalformat, command.width, command.height, command.border, command.format, command.type, command.pixels),
        DEF_COMMAND_HANDLER(TexSubImage2D, command.target, command.level, command.xoffset, command.yoffset, command.width, command.height, command.format, command.type, command.pixels),
        DEF_COMMAND_HANDLER(TexParameteri, command.target, command.pname, command.param),
        DEF_COMMAND_HANDLER(ActiveTexture, command.texture),
        DEF_COMMAND_HANDLER(GenerateMipmap, command.target),
        DEF_COMMAND_HANDLER(GenerateTextureMipmap, command.texture),
        DEF_COMMAND_HANDLER(DebugMessageCallback, command.callback, command.userParam),
        DEF_COMMAND_HANDLER(StringMarker, command.len, command.string),
        DEF_COMMAND_HANDLER(GenBuffers, command.n, command.buffers),
        DEF_COMMAND_HANDLER(DeleteBuffers, command.n, command.buffers),
        DEF_COMMAND_HANDLER(BindBuffer, command.target, command.buffer),
        DEF_COMMAND_HANDLER(BufferData, command.target, command.size, command.data, command.usage),
        DEF_COMMAND_HANDLER(BufferSubData, command.target, command.offset, command.size, command.data),
        DEF_COMMAND_HANDLER(CreateShader, command.type),
        DEF_COMMAND_HANDLER(DeleteShader, command.shader),
        DEF_COMMAND_HANDLER(ShaderSource, command.shader, command.count, command.strings, command.lengths),
        DEF_COMMAND_HANDLER(CompileShader, command.shader),
        DEF_COMMAND_HANDLER(AttachShader, command.program, command.shader),
        DEF_COMMAND_HANDLER(GetShaderiv, command.shader, command.pname, command.param),
        DEF_COMMAND_HANDLER(GetShaderInfoLog, command.shader, command.bufSize, command.length, command.infoLog),
        DEF_COMMAND_HANDLER_NO_ARGS(CreateProgram),
        DEF_COMMAND_HANDLER(DeleteProgram, command.program),
        DEF_COMMAND_HANDLER(UseProgram, command.program),
        DEF_COMMAND_HANDLER(LinkProgram, command.program),
        DEF_COMMAND_HANDLER(GetProgramiv, command.program, command.pname, command.param),
        DEF_COMMAND_HANDLER(GetProgramInfoLog, command.program, command.bufSize, command.length, command.infoLog),
        DEF_COMMAND_HANDLER(GetUniformLocation, command.program, command.name),
        DEF_COMMAND_HANDLER(Uniform1f, command.location, command.v0),
        DEF_COMMAND_HANDLER(Uniform2f, command.location, command.v0, command.v1),
        DEF_COMMAND_HANDLER(Uniform4f, command.location, command.v0, command.v1, command.v2, command.v3),
        DEF_COMMAND_HANDLER(Uniform1i, command.location, command.v0),
        DEF_COMMAND_HANDLER(Uniform1iv, command.location, command.count, command.value),
        DEF_COMMAND_HANDLER(UniformMatrix4fv, command.location, command.count, command.transpose, command.value),
        DEF_COMMAND_HANDLER(BindAttribLocation, command.program, command.index, command.name),
        DEF_COMMAND_HANDLER(EnableVertexAttribArray, command.index),
        DEF_COMMAND_HANDLER(DisableVertexAttribArray, command.index),
        DEF_COMMAND_HANDLER(VertexAttribPointer, command.index, command.size, command.type, command.normalized, command.stride, command.pointer),
        DEF_COMMAND_HANDLER(GenFramebuffers, command.n, command.framebuffers),
        DEF_COMMAND_HANDLER(DeleteFramebuffers, command.n, command.framebuffers),
        DEF_COMMAND_HANDLER(BindFramebuffer, command.target, command.framebuffer),
        DEF_COMMAND_HANDLER(FramebufferTexture2D, command.target, command.attachment, command.textarget, command.texture, command.level),
        DEF_COMMAND_HANDLER(BlitFramebuffer, command.srcX0, command.srcY0, command.srcX1, command.srcY1, command.dstX0, command.dstY0, command.dstX1, command.dstY1, command.mask, command.filter),
        DEF_COMMAND_HANDLER(GenVertexArrays, command.n, command.arrays),
        DEF_COMMAND_HANDLER(DeleteVertexArrays, command.n, command.arrays),
        DEF_COMMAND_HANDLER(BindVertexArray, command.array),
        DEF_COMMAND_HANDLER_NO_ARGS(ReleaseShaderCompiler),
        DEF_COMMAND_HANDLER(GetProcAddress, command.proc),
        DEF_COMMAND_HANDLER(MakeCurrent, command.window, command.context),
        DEF_COMMAND_HANDLER(SetSwapInterval, command.interval),
        DEF_COMMAND_HANDLER_NO_ARGS(GetSwapInterval),
        DEF_COMMAND_HANDLER(SwapWindow, command.window),
    };
    std::unique_lock<std::mutex> guard(gl.mutex);
    for (;;) {
        gl.commandId = 0;
        gl.cond.notify_one();
        do {
            gl.cond.wait(guard);
        } while (gl.commandId == 0);
        if (gl.commandId > sizeof handlers / sizeof *handlers) {
            return 0;
        }
        handlers[gl.commandId - 1]();
    }
    return 0;
}

void initGLFunctions(const Config &conf, SDL_Window *window, SDL_GLContext context)
{
    gl.GetProcAddress = gl._impl_GetProcAddress = SDL_GL_GetProcAddress;
    gl.MakeCurrent = commandMakeCurrent;
    gl._impl_MakeCurrent = [](SDL_Window *window, SDL_GLContext context) {
        return SDL_GL_MakeCurrent(window, context) < 0 ? SDL_GetError() : nullptr;
    };
    gl.SetSwapInterval = commandSetSwapInterval;
    gl._impl_SetSwapInterval = SDL_GL_SetSwapInterval;
    gl.GetSwapInterval = commandGetSwapInterval;
    gl._impl_GetSwapInterval = SDL_GL_GetSwapInterval;
    gl.SwapWindow = commandSwapWindow;
    gl._impl_SwapWindow = SDL_GL_SwapWindow;

#define EXT_SUFFIX ""
    GL_20_FUN;

    gl.multithreaded = true;

    switch (conf.multithreadedGl) {
        default:
            /* ANGLE doesn't support KHR_context_flush_control but doesn't flush the OpenGL pipeline when the context is bound/unbound,
             * so consider the release behavior to be GL_NONE when using ANGLE on any platform.
             * Core OpenGL (CGL) also neither supports KHR_context_flush_control nor flushes the pipeline on context binding/unbinding,
             * so also consider the release behavior to be GL_NONE on Apple platforms when ANGLE is not being used. */
#ifdef __APPLE__
            gl.context_release_behavior_none = true;
#else
#  ifdef MKXPZ_HAVE_ANGLE
            if (mkxp_use_angle)
                gl.context_release_behavior_none = true;
            else
#  endif // MKXPZ_HAVE_ANGLE
            {
                GLint context_release_behavior = 0x82fc /* GL_CONTEXT_RELEASE_BEHAVIOR_FLUSH */;
                gl._impl_GetIntegerv(0x82fb /* GL_CONTEXT_RELEASE_BEHAVIOR */, &context_release_behavior);
                gl.context_release_behavior_none = context_release_behavior == GL_NONE;
            }
#endif // __APPLE__
            break;
        case 1:
            gl.context_release_behavior_none = true;
            break;
        case 2:
            gl.context_release_behavior_none = false;
            break;
    }

    if (gl.context_release_behavior_none) {
        gl.thread = nullptr;
    } else {
        if (gl.multithreaded && SDL_GL_MakeCurrent(window, nullptr) < 0) {
            throw Exception(Exception::MKXPError, "Could not unbind OpenGL context: %s", SDL_GetError());
        }
        {
            std::unique_lock<std::mutex> guard(gl.mutex);
            gl.commandId = -1;
            if ((gl.thread = SDL_CreateThread(glThreadFun, "gl", nullptr)) == nullptr) {
                throw Exception(Exception::MKXPError, "Could not create OpenGL thread: %s", SDL_GetError());
            }
            do {
                gl.cond.wait(guard);
            } while (gl.commandId != 0);
        }
        if (gl.multithreaded) {
            const char *error = gl.MakeCurrent(window, context);
            if (error != nullptr) {
                throw Exception(Exception::MKXPError, "Could not bind OpenGL context on the OpenGL thread: %s", SDL_GetError());
            }
        }
    }

    gl.GetProcAddress = commandGetProcAddress;

    /* Determine GL version */
    const char *ver = (const char*) gl.GetString(GL_VERSION);
    
    const char glesPrefix[] = "OpenGL ES ";
    const size_t glesPrefixN = sizeof(glesPrefix)-1;
    
    bool gles = false;
    
    if (!strncmp(ver, glesPrefix, glesPrefixN))
    {
        gles = true;
        gl.glsles = true;
        
        ver += glesPrefixN;
    }
    
    /* Assume single digit */
    int glMajor = *ver - '0';
    
    if (glMajor < 2)
#ifndef GLES2_HEADER
        throw Exception(Exception::MKXPError,
                  "A graphics card that supports OpenGL 2.0 or later is required.\n\n"
                  "Driver information:\n"
                  "Vendor: %s\n"
                  "Renderer: %s\n"
                  "Version: %s\n"
                  "GLSL Version: %s\n",
                  gl.GetString(GL_VENDOR), gl.GetString(GL_RENDERER), gl.GetString(GL_VERSION),
                  gl.GetString(GL_SHADING_LANGUAGE_VERSION));
#else
        // on macOS, we're actually using either desktop GL or Metal due to ANGLE, but every Mac that supports Sierra
        // (officially or otherwise) should support ANGLE, so this should never be seen. Probably, anyway. Don't @ me
        throw EXC("A graphics card that supports OpenGL ES 2.0 or later is required.");
#endif
    
    if (gles)
    {
        GL_ES_FUN;
    }
    
    BoostSet<std::string> ext;
    
    if (glMajor >= 3)
        parseExtensionsCore(gl.GetStringi, gl.GetIntegerv, ext);
    else
        parseExtensionsCompat(gl.GetString, ext);
    
#define HAVE_EXT(_ext) ext.contains("GL_" #_ext)
    
    /* FBO entrypoints */
    if (glMajor >= 3 || HAVE_EXT(ARB_framebuffer_object))
    {
#undef EXT_SUFFIX
#define EXT_SUFFIX ""
        GL_FBO_FUN;
        GL_FBO_BLIT_FUN;
    }
    else if (gles && glMajor == 2)
    {
        GL_FBO_FUN;
    }
    else if (HAVE_EXT(EXT_framebuffer_object))
    {
#undef EXT_SUFFIX
#define EXT_SUFFIX "EXT"
        GL_FBO_FUN;
        
        if (HAVE_EXT(EXT_framebuffer_blit))
        {
            GL_FBO_BLIT_FUN;
        }
    }
    else
    {
        throw EXC("No FBO support available");
    }
    
    /* VAO entrypoints */
    if (HAVE_EXT(ARB_vertex_array_object) || glMajor >= 3)
    {
#undef EXT_SUFFIX
#define EXT_SUFFIX ""
        GL_VAO_FUN;
    }
    else if (HAVE_EXT(APPLE_vertex_array_object))
    {
#undef EXT_SUFFIX
#define EXT_SUFFIX "APPLE"
        GL_VAO_FUN;
    }
    else if (HAVE_EXT(OES_vertex_array_object))
    {
#undef EXT_SUFFIX
#define EXT_SUFFIX "OES"
        GL_VAO_FUN;
    }
    
    /* Debug callback entrypoints */
    if (HAVE_EXT(KHR_debug))
    {
#undef EXT_SUFFIX
#define EXT_SUFFIX ""
        GL_DEBUG_KHR_FUN;
    }
    else if (HAVE_EXT(ARB_debug_output))
    {
#undef EXT_SUFFIX
#define EXT_SUFFIX "ARB"
        GL_DEBUG_KHR_FUN;
    }
    
    if (HAVE_EXT(GREMEDY_string_marker))
    {
#undef EXT_SUFFIX
#define EXT_SUFFIX "GREMEDY"
        GL_GREMEMDY_FUN;
    }
    
    /* Misc caps */
    if (!gles || glMajor >= 3 || HAVE_EXT(EXT_unpack_subimage))
        gl.unpack_subimage = true;
    
    if (!gles || glMajor >= 3 || HAVE_EXT(OES_texture_npot))
        gl.npot_repeat = true;
}
