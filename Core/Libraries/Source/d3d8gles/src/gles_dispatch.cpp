// GeneralsX @build Android port ANGLE integration
#include "gles_dispatch.h"

#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <cstdio>

namespace {

typedef void (GL_APIENTRY *PFN_glActiveTexture)(GLenum texture);
typedef void (GL_APIENTRY *PFN_glAttachShader)(GLuint program, GLuint shader);
typedef void (GL_APIENTRY *PFN_glBindBuffer)(GLenum target, GLuint buffer);
typedef void (GL_APIENTRY *PFN_glBindBufferBase)(GLenum target, GLuint index, GLuint buffer);
typedef void (GL_APIENTRY *PFN_glBindFramebuffer)(GLenum target, GLuint framebuffer);
typedef void (GL_APIENTRY *PFN_glBindRenderbuffer)(GLenum target, GLuint renderbuffer);
typedef void (GL_APIENTRY *PFN_glBindTexture)(GLenum target, GLuint texture);
typedef void (GL_APIENTRY *PFN_glBindVertexArray)(GLuint array);
typedef void (GL_APIENTRY *PFN_glBlendFunc)(GLenum sfactor, GLenum dfactor);
typedef void (GL_APIENTRY *PFN_glBlendFuncSeparate)(GLenum, GLenum, GLenum, GLenum);
typedef void (GL_APIENTRY *PFN_glDrawBuffers)(GLsizei, const GLenum *);
typedef void (GL_APIENTRY *PFN_glClearBufferfv)(GLenum, GLint, const GLfloat *);
typedef void (GL_APIENTRY *PFN_glBlitFramebuffer)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void (GL_APIENTRY *PFN_glBufferData)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (GL_APIENTRY *PFN_glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
// GeneralsX @perf Android port 09/05/2026 Needed to translate D3DLOCK_NOOVERWRITE
// faithfully: GL_MAP_UNSYNCHRONIZED_BIT is the only way to write into a region
// of a buffer the GPU may still be reading without the driver inserting a wait,
// which is exactly the guarantee NOOVERWRITE makes.
typedef void *(GL_APIENTRY *PFN_glMapBufferRange)(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef GLboolean (GL_APIENTRY *PFN_glUnmapBuffer)(GLenum target);
typedef GLenum (GL_APIENTRY *PFN_glCheckFramebufferStatus)(GLenum target);
typedef void (GL_APIENTRY *PFN_glClear)(GLbitfield mask);
typedef void (GL_APIENTRY *PFN_glClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (GL_APIENTRY *PFN_glClearDepthf)(GLfloat d);
typedef void (GL_APIENTRY *PFN_glClearStencil)(GLint s);
typedef void (GL_APIENTRY *PFN_glColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
typedef void (GL_APIENTRY *PFN_glCompileShader)(GLuint shader);
typedef void (GL_APIENTRY *PFN_glCompressedTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data);
typedef GLuint (GL_APIENTRY *PFN_glCreateProgram)(void);
typedef GLuint (GL_APIENTRY *PFN_glCreateShader)(GLenum type);
typedef void (GL_APIENTRY *PFN_glCullFace)(GLenum mode);
typedef void (GL_APIENTRY *PFN_glDeleteBuffers)(GLsizei n, const GLuint *buffers);
typedef void (GL_APIENTRY *PFN_glDeleteFramebuffers)(GLsizei n, const GLuint *framebuffers);
typedef void (GL_APIENTRY *PFN_glDeleteProgram)(GLuint program);
typedef void (GL_APIENTRY *PFN_glDeleteRenderbuffers)(GLsizei n, const GLuint *renderbuffers);
typedef void (GL_APIENTRY *PFN_glDeleteShader)(GLuint shader);
typedef void (GL_APIENTRY *PFN_glDeleteTextures)(GLsizei n, const GLuint *textures);
typedef void (GL_APIENTRY *PFN_glDeleteVertexArrays)(GLsizei n, const GLuint *arrays);
typedef void (GL_APIENTRY *PFN_glDepthFunc)(GLenum func);
typedef void (GL_APIENTRY *PFN_glDepthMask)(GLboolean flag);
typedef void (GL_APIENTRY *PFN_glDepthRangef)(GLfloat n, GLfloat f);
typedef void (GL_APIENTRY *PFN_glDisable)(GLenum cap);
typedef void (GL_APIENTRY *PFN_glDisableVertexAttribArray)(GLuint index);
typedef void (GL_APIENTRY *PFN_glDrawArrays)(GLenum mode, GLint first, GLsizei count);
typedef void (GL_APIENTRY *PFN_glDrawElements)(GLenum mode, GLsizei count, GLenum type, const void *indices);
typedef void (GL_APIENTRY *PFN_glEnable)(GLenum cap);
typedef void (GL_APIENTRY *PFN_glEnableVertexAttribArray)(GLuint index);
typedef void (GL_APIENTRY *PFN_glFinish)(void);
typedef void (GL_APIENTRY *PFN_glFramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (GL_APIENTRY *PFN_glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (GL_APIENTRY *PFN_glGenBuffers)(GLsizei n, GLuint *buffers);
typedef void (GL_APIENTRY *PFN_glGenFramebuffers)(GLsizei n, GLuint *framebuffers);
typedef void (GL_APIENTRY *PFN_glGenRenderbuffers)(GLsizei n, GLuint *renderbuffers);
typedef void (GL_APIENTRY *PFN_glGenTextures)(GLsizei n, GLuint *textures);
typedef void (GL_APIENTRY *PFN_glGenVertexArrays)(GLsizei n, GLuint *arrays);
typedef void (GL_APIENTRY *PFN_glGenerateMipmap)(GLenum target);
typedef GLenum (GL_APIENTRY *PFN_glGetError)(void);
typedef void (GL_APIENTRY *PFN_glGetIntegerv)(GLenum pname, GLint *data);
typedef void (GL_APIENTRY *PFN_glGetBooleanv)(GLenum pname, GLboolean *data);
typedef void (GL_APIENTRY *PFN_glGenQueries)(GLsizei n, GLuint *ids);
typedef void (GL_APIENTRY *PFN_glBeginQuery)(GLenum target, GLuint id);
typedef void (GL_APIENTRY *PFN_glEndQuery)(GLenum target);
typedef void (GL_APIENTRY *PFN_glGetQueryObjectuiv)(GLuint id, GLenum pname, GLuint *params);
typedef void (GL_APIENTRY *PFN_glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (GL_APIENTRY *PFN_glGetProgramiv)(GLuint program, GLenum pname, GLint *params);
typedef void (GL_APIENTRY *PFN_glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (GL_APIENTRY *PFN_glGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
typedef const GLubyte * (GL_APIENTRY *PFN_glGetString)(GLenum name);
typedef GLuint (GL_APIENTRY *PFN_glGetUniformBlockIndex)(GLuint program, const GLchar *uniformBlockName);
typedef GLint (GL_APIENTRY *PFN_glGetUniformLocation)(GLuint program, const GLchar *name);
typedef void (GL_APIENTRY *PFN_glLinkProgram)(GLuint program);
typedef void (GL_APIENTRY *PFN_glPixelStorei)(GLenum pname, GLint param);
typedef void (GL_APIENTRY *PFN_glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels);
typedef void (GL_APIENTRY *PFN_glPolygonOffset)(GLfloat factor, GLfloat units);
typedef void (GL_APIENTRY *PFN_glRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GL_APIENTRY *PFN_glScissor)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GL_APIENTRY *PFN_glShaderSource)(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (GL_APIENTRY *PFN_glStencilFunc)(GLenum func, GLint ref, GLuint mask);
typedef void (GL_APIENTRY *PFN_glStencilMask)(GLuint mask);
typedef void (GL_APIENTRY *PFN_glStencilOp)(GLenum fail, GLenum zfail, GLenum zpass);
typedef void (GL_APIENTRY *PFN_glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (GL_APIENTRY *PFN_glTexParameteri)(GLenum target, GLenum pname, GLint param);
typedef void (GL_APIENTRY *PFN_glUniformBlockBinding)(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
typedef void (GL_APIENTRY *PFN_glUniform1f)(GLint location, GLfloat v0);
typedef void (GL_APIENTRY *PFN_glUniform1i)(GLint location, GLint v0);
typedef void (GL_APIENTRY *PFN_glUniform1iv)(GLint location, GLsizei count, const GLint *value);
typedef void (GL_APIENTRY *PFN_glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
typedef void (GL_APIENTRY *PFN_glUniform3fv)(GLint location, GLsizei count, const GLfloat *value);
typedef void (GL_APIENTRY *PFN_glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (GL_APIENTRY *PFN_glUniform4fv)(GLint location, GLsizei count, const GLfloat *value);
typedef void (GL_APIENTRY *PFN_glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (GL_APIENTRY *PFN_glUseProgram)(GLuint program);
typedef void (GL_APIENTRY *PFN_glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void (GL_APIENTRY *PFN_glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);

PFN_glActiveTexture d3d8gles_pfn_glActiveTexture = nullptr;
PFN_glAttachShader d3d8gles_pfn_glAttachShader = nullptr;
PFN_glBindBuffer d3d8gles_pfn_glBindBuffer = nullptr;
PFN_glBindBufferBase d3d8gles_pfn_glBindBufferBase = nullptr;
PFN_glBindFramebuffer d3d8gles_pfn_glBindFramebuffer = nullptr;
PFN_glBindRenderbuffer d3d8gles_pfn_glBindRenderbuffer = nullptr;
PFN_glBindTexture d3d8gles_pfn_glBindTexture = nullptr;
PFN_glBindVertexArray d3d8gles_pfn_glBindVertexArray = nullptr;
PFN_glBlendFunc d3d8gles_pfn_glBlendFunc = nullptr;
PFN_glBlendFuncSeparate d3d8gles_pfn_glBlendFuncSeparate = nullptr;
PFN_glDrawBuffers d3d8gles_pfn_glDrawBuffers = nullptr;
PFN_glClearBufferfv d3d8gles_pfn_glClearBufferfv = nullptr;
PFN_glBlitFramebuffer d3d8gles_pfn_glBlitFramebuffer = nullptr;
PFN_glBufferData d3d8gles_pfn_glBufferData = nullptr;
PFN_glBufferSubData d3d8gles_pfn_glBufferSubData = nullptr;
PFN_glMapBufferRange d3d8gles_pfn_glMapBufferRange = nullptr;
PFN_glUnmapBuffer d3d8gles_pfn_glUnmapBuffer = nullptr;
PFN_glCheckFramebufferStatus d3d8gles_pfn_glCheckFramebufferStatus = nullptr;
PFN_glClear d3d8gles_pfn_glClear = nullptr;
PFN_glClearColor d3d8gles_pfn_glClearColor = nullptr;
PFN_glClearDepthf d3d8gles_pfn_glClearDepthf = nullptr;
PFN_glClearStencil d3d8gles_pfn_glClearStencil = nullptr;
PFN_glColorMask d3d8gles_pfn_glColorMask = nullptr;
PFN_glCompileShader d3d8gles_pfn_glCompileShader = nullptr;
PFN_glCompressedTexImage2D d3d8gles_pfn_glCompressedTexImage2D = nullptr;
PFN_glCreateProgram d3d8gles_pfn_glCreateProgram = nullptr;
PFN_glCreateShader d3d8gles_pfn_glCreateShader = nullptr;
PFN_glCullFace d3d8gles_pfn_glCullFace = nullptr;
PFN_glDeleteBuffers d3d8gles_pfn_glDeleteBuffers = nullptr;
PFN_glDeleteFramebuffers d3d8gles_pfn_glDeleteFramebuffers = nullptr;
PFN_glDeleteProgram d3d8gles_pfn_glDeleteProgram = nullptr;
PFN_glDeleteRenderbuffers d3d8gles_pfn_glDeleteRenderbuffers = nullptr;
PFN_glDeleteShader d3d8gles_pfn_glDeleteShader = nullptr;
PFN_glDeleteTextures d3d8gles_pfn_glDeleteTextures = nullptr;
PFN_glDeleteVertexArrays d3d8gles_pfn_glDeleteVertexArrays = nullptr;
PFN_glDepthFunc d3d8gles_pfn_glDepthFunc = nullptr;
PFN_glDepthMask d3d8gles_pfn_glDepthMask = nullptr;
PFN_glDepthRangef d3d8gles_pfn_glDepthRangef = nullptr;
PFN_glDisable d3d8gles_pfn_glDisable = nullptr;
PFN_glDisableVertexAttribArray d3d8gles_pfn_glDisableVertexAttribArray = nullptr;
PFN_glDrawArrays d3d8gles_pfn_glDrawArrays = nullptr;
PFN_glDrawElements d3d8gles_pfn_glDrawElements = nullptr;
PFN_glEnable d3d8gles_pfn_glEnable = nullptr;
PFN_glEnableVertexAttribArray d3d8gles_pfn_glEnableVertexAttribArray = nullptr;
PFN_glFinish d3d8gles_pfn_glFinish = nullptr;
PFN_glFramebufferRenderbuffer d3d8gles_pfn_glFramebufferRenderbuffer = nullptr;
PFN_glFramebufferTexture2D d3d8gles_pfn_glFramebufferTexture2D = nullptr;
PFN_glGenBuffers d3d8gles_pfn_glGenBuffers = nullptr;
PFN_glGenFramebuffers d3d8gles_pfn_glGenFramebuffers = nullptr;
PFN_glGenRenderbuffers d3d8gles_pfn_glGenRenderbuffers = nullptr;
PFN_glGenTextures d3d8gles_pfn_glGenTextures = nullptr;
PFN_glGenVertexArrays d3d8gles_pfn_glGenVertexArrays = nullptr;
PFN_glGenerateMipmap d3d8gles_pfn_glGenerateMipmap = nullptr;
PFN_glGetError d3d8gles_pfn_glGetError = nullptr;
PFN_glGetIntegerv d3d8gles_pfn_glGetIntegerv = nullptr;
PFN_glGetBooleanv d3d8gles_pfn_glGetBooleanv = nullptr;
PFN_glGenQueries d3d8gles_pfn_glGenQueries = nullptr;
PFN_glBeginQuery d3d8gles_pfn_glBeginQuery = nullptr;
PFN_glEndQuery d3d8gles_pfn_glEndQuery = nullptr;
PFN_glGetQueryObjectuiv d3d8gles_pfn_glGetQueryObjectuiv = nullptr;
PFN_glGetProgramInfoLog d3d8gles_pfn_glGetProgramInfoLog = nullptr;
PFN_glGetProgramiv d3d8gles_pfn_glGetProgramiv = nullptr;
PFN_glGetShaderInfoLog d3d8gles_pfn_glGetShaderInfoLog = nullptr;
PFN_glGetShaderiv d3d8gles_pfn_glGetShaderiv = nullptr;
PFN_glGetString d3d8gles_pfn_glGetString = nullptr;
PFN_glGetUniformBlockIndex d3d8gles_pfn_glGetUniformBlockIndex = nullptr;
PFN_glGetUniformLocation d3d8gles_pfn_glGetUniformLocation = nullptr;
PFN_glLinkProgram d3d8gles_pfn_glLinkProgram = nullptr;
PFN_glPixelStorei d3d8gles_pfn_glPixelStorei = nullptr;
PFN_glReadPixels d3d8gles_pfn_glReadPixels = nullptr;
PFN_glPolygonOffset d3d8gles_pfn_glPolygonOffset = nullptr;
PFN_glRenderbufferStorage d3d8gles_pfn_glRenderbufferStorage = nullptr;
PFN_glScissor d3d8gles_pfn_glScissor = nullptr;
PFN_glShaderSource d3d8gles_pfn_glShaderSource = nullptr;
PFN_glStencilFunc d3d8gles_pfn_glStencilFunc = nullptr;
PFN_glStencilMask d3d8gles_pfn_glStencilMask = nullptr;
PFN_glStencilOp d3d8gles_pfn_glStencilOp = nullptr;
PFN_glTexImage2D d3d8gles_pfn_glTexImage2D = nullptr;
PFN_glTexParameteri d3d8gles_pfn_glTexParameteri = nullptr;
PFN_glUniformBlockBinding d3d8gles_pfn_glUniformBlockBinding = nullptr;
PFN_glUniform1f d3d8gles_pfn_glUniform1f = nullptr;
PFN_glUniform1i d3d8gles_pfn_glUniform1i = nullptr;
PFN_glUniform1iv d3d8gles_pfn_glUniform1iv = nullptr;
PFN_glUniform2f d3d8gles_pfn_glUniform2f = nullptr;
PFN_glUniform3fv d3d8gles_pfn_glUniform3fv = nullptr;
PFN_glUniform4f d3d8gles_pfn_glUniform4f = nullptr;
PFN_glUniform4fv d3d8gles_pfn_glUniform4fv = nullptr;
PFN_glUniformMatrix4fv d3d8gles_pfn_glUniformMatrix4fv = nullptr;
PFN_glUseProgram d3d8gles_pfn_glUseProgram = nullptr;
PFN_glVertexAttribPointer d3d8gles_pfn_glVertexAttribPointer = nullptr;
PFN_glViewport d3d8gles_pfn_glViewport = nullptr;

} // namespace

extern "C" {

GL_APICALL void GL_APIENTRY glActiveTexture(GLenum texture)
{
	d3d8gles_pfn_glActiveTexture(texture);
}

GL_APICALL void GL_APIENTRY glAttachShader(GLuint program, GLuint shader)
{
	d3d8gles_pfn_glAttachShader(program, shader);
}

GL_APICALL void GL_APIENTRY glBindBuffer(GLenum target, GLuint buffer)
{
	d3d8gles_pfn_glBindBuffer(target, buffer);
}

GL_APICALL void GL_APIENTRY glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
	d3d8gles_pfn_glBindBufferBase(target, index, buffer);
}

GL_APICALL void GL_APIENTRY glBindFramebuffer(GLenum target, GLuint framebuffer)
{
	d3d8gles_pfn_glBindFramebuffer(target, framebuffer);
}

GL_APICALL void GL_APIENTRY glBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
	d3d8gles_pfn_glBindRenderbuffer(target, renderbuffer);
}

GL_APICALL void GL_APIENTRY glBindTexture(GLenum target, GLuint texture)
{
	d3d8gles_pfn_glBindTexture(target, texture);
}

GL_APICALL void GL_APIENTRY glBindVertexArray(GLuint array)
{
	d3d8gles_pfn_glBindVertexArray(array);
}

GL_APICALL void GL_APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	d3d8gles_pfn_glBlendFunc(sfactor, dfactor);
}
GL_APICALL void GL_APIENTRY glBlendFuncSeparate(GLenum s, GLenum d, GLenum sa, GLenum da) { d3d8gles_pfn_glBlendFuncSeparate(s,d,sa,da); }
GL_APICALL void GL_APIENTRY glDrawBuffers(GLsizei n, const GLenum *b) { d3d8gles_pfn_glDrawBuffers(n,b); }
GL_APICALL void GL_APIENTRY glClearBufferfv(GLenum b, GLint d, const GLfloat *v) { d3d8gles_pfn_glClearBufferfv(b,d,v); }

GL_APICALL void GL_APIENTRY glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                              GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                                              GLbitfield mask, GLenum filter)
{
	d3d8gles_pfn_glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

GL_APICALL void GL_APIENTRY glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
	d3d8gles_pfn_glBufferData(target, size, data, usage);
}

GL_APICALL void GL_APIENTRY glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
	d3d8gles_pfn_glBufferSubData(target, offset, size, data);
}

GL_APICALL void *GL_APIENTRY glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
	return d3d8gles_pfn_glMapBufferRange(target, offset, length, access);
}

GL_APICALL GLboolean GL_APIENTRY glUnmapBuffer(GLenum target)
{
	return d3d8gles_pfn_glUnmapBuffer(target);
}

GL_APICALL GLenum GL_APIENTRY glCheckFramebufferStatus(GLenum target)
{
	return d3d8gles_pfn_glCheckFramebufferStatus(target);
}

GL_APICALL void GL_APIENTRY glClear(GLbitfield mask)
{
	d3d8gles_pfn_glClear(mask);
}

GL_APICALL void GL_APIENTRY glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	d3d8gles_pfn_glClearColor(red, green, blue, alpha);
}

GL_APICALL void GL_APIENTRY glClearDepthf(GLfloat d)
{
	d3d8gles_pfn_glClearDepthf(d);
}

GL_APICALL void GL_APIENTRY glClearStencil(GLint s)
{
	d3d8gles_pfn_glClearStencil(s);
}

GL_APICALL void GL_APIENTRY glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	d3d8gles_pfn_glColorMask(red, green, blue, alpha);
}

GL_APICALL void GL_APIENTRY glCompileShader(GLuint shader)
{
	d3d8gles_pfn_glCompileShader(shader);
}

GL_APICALL void GL_APIENTRY glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data)
{
	d3d8gles_pfn_glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
}

GL_APICALL GLuint GL_APIENTRY glCreateProgram(void)
{
	return d3d8gles_pfn_glCreateProgram();
}

GL_APICALL GLuint GL_APIENTRY glCreateShader(GLenum type)
{
	return d3d8gles_pfn_glCreateShader(type);
}

GL_APICALL void GL_APIENTRY glCullFace(GLenum mode)
{
	d3d8gles_pfn_glCullFace(mode);
}

GL_APICALL void GL_APIENTRY glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
	d3d8gles_pfn_glDeleteBuffers(n, buffers);
}

GL_APICALL void GL_APIENTRY glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
	d3d8gles_pfn_glDeleteFramebuffers(n, framebuffers);
}

GL_APICALL void GL_APIENTRY glDeleteProgram(GLuint program)
{
	d3d8gles_pfn_glDeleteProgram(program);
}

GL_APICALL void GL_APIENTRY glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers)
{
	d3d8gles_pfn_glDeleteRenderbuffers(n, renderbuffers);
}

GL_APICALL void GL_APIENTRY glDeleteShader(GLuint shader)
{
	d3d8gles_pfn_glDeleteShader(shader);
}

GL_APICALL void GL_APIENTRY glDeleteTextures(GLsizei n, const GLuint *textures)
{
	d3d8gles_pfn_glDeleteTextures(n, textures);
}

GL_APICALL void GL_APIENTRY glDeleteVertexArrays(GLsizei n, const GLuint *arrays)
{
	d3d8gles_pfn_glDeleteVertexArrays(n, arrays);
}

GL_APICALL void GL_APIENTRY glDepthFunc(GLenum func)
{
	d3d8gles_pfn_glDepthFunc(func);
}

GL_APICALL void GL_APIENTRY glDepthMask(GLboolean flag)
{
	d3d8gles_pfn_glDepthMask(flag);
}

GL_APICALL void GL_APIENTRY glDepthRangef(GLfloat n, GLfloat f)
{
	d3d8gles_pfn_glDepthRangef(n, f);
}

GL_APICALL void GL_APIENTRY glDisable(GLenum cap)
{
	d3d8gles_pfn_glDisable(cap);
}

GL_APICALL void GL_APIENTRY glDisableVertexAttribArray(GLuint index)
{
	d3d8gles_pfn_glDisableVertexAttribArray(index);
}

GL_APICALL void GL_APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	d3d8gles_pfn_glDrawArrays(mode, first, count);
}

GL_APICALL void GL_APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
	d3d8gles_pfn_glDrawElements(mode, count, type, indices);
}

GL_APICALL void GL_APIENTRY glEnable(GLenum cap)
{
	d3d8gles_pfn_glEnable(cap);
}

GL_APICALL void GL_APIENTRY glEnableVertexAttribArray(GLuint index)
{
	d3d8gles_pfn_glEnableVertexAttribArray(index);
}

GL_APICALL void GL_APIENTRY glFinish(void)
{
	d3d8gles_pfn_glFinish();
}

GL_APICALL void GL_APIENTRY glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	d3d8gles_pfn_glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

GL_APICALL void GL_APIENTRY glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	d3d8gles_pfn_glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

GL_APICALL void GL_APIENTRY glGenBuffers(GLsizei n, GLuint *buffers)
{
	d3d8gles_pfn_glGenBuffers(n, buffers);
}

GL_APICALL void GL_APIENTRY glGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
	d3d8gles_pfn_glGenFramebuffers(n, framebuffers);
}

GL_APICALL void GL_APIENTRY glGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
	d3d8gles_pfn_glGenRenderbuffers(n, renderbuffers);
}

GL_APICALL void GL_APIENTRY glGenTextures(GLsizei n, GLuint *textures)
{
	d3d8gles_pfn_glGenTextures(n, textures);
}

GL_APICALL void GL_APIENTRY glGenVertexArrays(GLsizei n, GLuint *arrays)
{
	d3d8gles_pfn_glGenVertexArrays(n, arrays);
}

GL_APICALL void GL_APIENTRY glGenerateMipmap(GLenum target)
{
	d3d8gles_pfn_glGenerateMipmap(target);
}

GL_APICALL GLenum GL_APIENTRY glGetError(void)
{
	return d3d8gles_pfn_glGetError();
}

GL_APICALL void GL_APIENTRY glGetBooleanv(GLenum pname, GLboolean *data)
{
	d3d8gles_pfn_glGetBooleanv(pname, data);
}

GL_APICALL void GL_APIENTRY glGenQueries(GLsizei n, GLuint *ids)
{
	d3d8gles_pfn_glGenQueries(n, ids);
}

GL_APICALL void GL_APIENTRY glBeginQuery(GLenum target, GLuint id)
{
	d3d8gles_pfn_glBeginQuery(target, id);
}

GL_APICALL void GL_APIENTRY glEndQuery(GLenum target)
{
	d3d8gles_pfn_glEndQuery(target);
}

GL_APICALL void GL_APIENTRY glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params)
{
	d3d8gles_pfn_glGetQueryObjectuiv(id, pname, params);
}

GL_APICALL void GL_APIENTRY glGetIntegerv(GLenum pname, GLint *data)
{
	d3d8gles_pfn_glGetIntegerv(pname, data);
}

GL_APICALL void GL_APIENTRY glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
	d3d8gles_pfn_glGetProgramInfoLog(program, bufSize, length, infoLog);
}

GL_APICALL void GL_APIENTRY glGetProgramiv(GLuint program, GLenum pname, GLint *params)
{
	d3d8gles_pfn_glGetProgramiv(program, pname, params);
}

GL_APICALL void GL_APIENTRY glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
	d3d8gles_pfn_glGetShaderInfoLog(shader, bufSize, length, infoLog);
}

GL_APICALL void GL_APIENTRY glGetShaderiv(GLuint shader, GLenum pname, GLint *params)
{
	d3d8gles_pfn_glGetShaderiv(shader, pname, params);
}

GL_APICALL const GLubyte * GL_APIENTRY glGetString(GLenum name)
{
	return d3d8gles_pfn_glGetString(name);
}

GL_APICALL GLuint GL_APIENTRY glGetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName)
{
	return d3d8gles_pfn_glGetUniformBlockIndex(program, uniformBlockName);
}

GL_APICALL GLint GL_APIENTRY glGetUniformLocation(GLuint program, const GLchar *name)
{
	return d3d8gles_pfn_glGetUniformLocation(program, name);
}

GL_APICALL void GL_APIENTRY glLinkProgram(GLuint program)
{
	d3d8gles_pfn_glLinkProgram(program);
}

GL_APICALL void GL_APIENTRY glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                                         GLenum format, GLenum type, void *pixels)
{
	d3d8gles_pfn_glReadPixels(x, y, width, height, format, type, pixels);
}

GL_APICALL void GL_APIENTRY glPixelStorei(GLenum pname, GLint param)
{
	d3d8gles_pfn_glPixelStorei(pname, param);
}

GL_APICALL void GL_APIENTRY glPolygonOffset(GLfloat factor, GLfloat units)
{
	d3d8gles_pfn_glPolygonOffset(factor, units);
}

GL_APICALL void GL_APIENTRY glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
	d3d8gles_pfn_glRenderbufferStorage(target, internalformat, width, height);
}

GL_APICALL void GL_APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
	d3d8gles_pfn_glScissor(x, y, width, height);
}

GL_APICALL void GL_APIENTRY glShaderSource(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length)
{
	d3d8gles_pfn_glShaderSource(shader, count, string, length);
}

GL_APICALL void GL_APIENTRY glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
	d3d8gles_pfn_glStencilFunc(func, ref, mask);
}

GL_APICALL void GL_APIENTRY glStencilMask(GLuint mask)
{
	d3d8gles_pfn_glStencilMask(mask);
}

GL_APICALL void GL_APIENTRY glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
	d3d8gles_pfn_glStencilOp(fail, zfail, zpass);
}

GL_APICALL void GL_APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels)
{
	d3d8gles_pfn_glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

GL_APICALL void GL_APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	d3d8gles_pfn_glTexParameteri(target, pname, param);
}

GL_APICALL void GL_APIENTRY glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
	d3d8gles_pfn_glUniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
}

GL_APICALL void GL_APIENTRY glUniform1f(GLint location, GLfloat v0)
{
	d3d8gles_pfn_glUniform1f(location, v0);
}

GL_APICALL void GL_APIENTRY glUniform1i(GLint location, GLint v0)
{
	d3d8gles_pfn_glUniform1i(location, v0);
}

GL_APICALL void GL_APIENTRY glUniform1iv(GLint location, GLsizei count, const GLint *value)
{
	d3d8gles_pfn_glUniform1iv(location, count, value);
}

GL_APICALL void GL_APIENTRY glUniform2f(GLint location, GLfloat v0, GLfloat v1)
{
	d3d8gles_pfn_glUniform2f(location, v0, v1);
}

GL_APICALL void GL_APIENTRY glUniform3fv(GLint location, GLsizei count, const GLfloat *value)
{
	d3d8gles_pfn_glUniform3fv(location, count, value);
}

GL_APICALL void GL_APIENTRY glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
	d3d8gles_pfn_glUniform4f(location, v0, v1, v2, v3);
}

GL_APICALL void GL_APIENTRY glUniform4fv(GLint location, GLsizei count, const GLfloat *value)
{
	d3d8gles_pfn_glUniform4fv(location, count, value);
}

GL_APICALL void GL_APIENTRY glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	d3d8gles_pfn_glUniformMatrix4fv(location, count, transpose, value);
}

GL_APICALL void GL_APIENTRY glUseProgram(GLuint program)
{
	d3d8gles_pfn_glUseProgram(program);
}

GL_APICALL void GL_APIENTRY glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)
{
	d3d8gles_pfn_glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

GL_APICALL void GL_APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	d3d8gles_pfn_glViewport(x, y, width, height);
}

} // extern "C"

bool d3d8gles_LoadGLESDispatch(const char *libName)
{
	void *lib = dlopen(libName, RTLD_NOW | RTLD_GLOBAL);
	if (!lib) {
		fprintf(stderr, "[d3d8gles] GLES dispatch: dlopen(%s) failed: %s\n", libName, dlerror());
		return false;
	}

	bool ok = true;
	d3d8gles_pfn_glActiveTexture = reinterpret_cast<PFN_glActiveTexture>(dlsym(lib, "glActiveTexture"));
	if (!d3d8gles_pfn_glActiveTexture) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glActiveTexture in %s\n", libName); ok = false; }
	d3d8gles_pfn_glAttachShader = reinterpret_cast<PFN_glAttachShader>(dlsym(lib, "glAttachShader"));
	if (!d3d8gles_pfn_glAttachShader) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glAttachShader in %s\n", libName); ok = false; }
	d3d8gles_pfn_glBindBuffer = reinterpret_cast<PFN_glBindBuffer>(dlsym(lib, "glBindBuffer"));
	if (!d3d8gles_pfn_glBindBuffer) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glBindBuffer in %s\n", libName); ok = false; }
	d3d8gles_pfn_glBindBufferBase = reinterpret_cast<PFN_glBindBufferBase>(dlsym(lib, "glBindBufferBase"));
	if (!d3d8gles_pfn_glBindBufferBase) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glBindBufferBase in %s\n", libName); ok = false; }
	d3d8gles_pfn_glBindFramebuffer = reinterpret_cast<PFN_glBindFramebuffer>(dlsym(lib, "glBindFramebuffer"));
	if (!d3d8gles_pfn_glBindFramebuffer) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glBindFramebuffer in %s\n", libName); ok = false; }
	d3d8gles_pfn_glBindRenderbuffer = reinterpret_cast<PFN_glBindRenderbuffer>(dlsym(lib, "glBindRenderbuffer"));
	if (!d3d8gles_pfn_glBindRenderbuffer) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glBindRenderbuffer in %s\n", libName); ok = false; }
	d3d8gles_pfn_glBindTexture = reinterpret_cast<PFN_glBindTexture>(dlsym(lib, "glBindTexture"));
	if (!d3d8gles_pfn_glBindTexture) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glBindTexture in %s\n", libName); ok = false; }
	d3d8gles_pfn_glBindVertexArray = reinterpret_cast<PFN_glBindVertexArray>(dlsym(lib, "glBindVertexArray"));
	if (!d3d8gles_pfn_glBindVertexArray) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glBindVertexArray in %s\n", libName); ok = false; }
	d3d8gles_pfn_glBlendFunc = reinterpret_cast<PFN_glBlendFunc>(dlsym(lib, "glBlendFunc"));
	d3d8gles_pfn_glBlendFuncSeparate = reinterpret_cast<PFN_glBlendFuncSeparate>(dlsym(lib,"glBlendFuncSeparate"));
	d3d8gles_pfn_glDrawBuffers = reinterpret_cast<PFN_glDrawBuffers>(dlsym(lib,"glDrawBuffers"));
	d3d8gles_pfn_glClearBufferfv = reinterpret_cast<PFN_glClearBufferfv>(dlsym(lib,"glClearBufferfv"));
	if(!d3d8gles_pfn_glBlendFuncSeparate || !d3d8gles_pfn_glDrawBuffers || !d3d8gles_pfn_glClearBufferfv) {
		fprintf(stderr,"[d3d8gles] GLES dispatch: missing GLES3 layer entry points\n"); ok=false;
	}
	if (!d3d8gles_pfn_glBlendFunc) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glBlendFunc in %s\n", libName); ok = false; }
	d3d8gles_pfn_glBlitFramebuffer = reinterpret_cast<PFN_glBlitFramebuffer>(dlsym(lib, "glBlitFramebuffer"));
	if (!d3d8gles_pfn_glBlitFramebuffer) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glBlitFramebuffer in %s\n", libName); ok = false; }
	d3d8gles_pfn_glBufferData = reinterpret_cast<PFN_glBufferData>(dlsym(lib, "glBufferData"));
	if (!d3d8gles_pfn_glBufferData) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glBufferData in %s\n", libName); ok = false; }
	d3d8gles_pfn_glBufferSubData = reinterpret_cast<PFN_glBufferSubData>(dlsym(lib, "glBufferSubData"));
	if (!d3d8gles_pfn_glBufferSubData) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glBufferSubData in %s\n", libName); ok = false; }
	d3d8gles_pfn_glMapBufferRange = reinterpret_cast<PFN_glMapBufferRange>(dlsym(lib, "glMapBufferRange"));
	if (!d3d8gles_pfn_glMapBufferRange) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glMapBufferRange in %s\n", libName); ok = false; }
	d3d8gles_pfn_glUnmapBuffer = reinterpret_cast<PFN_glUnmapBuffer>(dlsym(lib, "glUnmapBuffer"));
	if (!d3d8gles_pfn_glUnmapBuffer) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glUnmapBuffer in %s\n", libName); ok = false; }
	d3d8gles_pfn_glCheckFramebufferStatus = reinterpret_cast<PFN_glCheckFramebufferStatus>(dlsym(lib, "glCheckFramebufferStatus"));
	if (!d3d8gles_pfn_glCheckFramebufferStatus) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glCheckFramebufferStatus in %s\n", libName); ok = false; }
	d3d8gles_pfn_glClear = reinterpret_cast<PFN_glClear>(dlsym(lib, "glClear"));
	if (!d3d8gles_pfn_glClear) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glClear in %s\n", libName); ok = false; }
	d3d8gles_pfn_glClearColor = reinterpret_cast<PFN_glClearColor>(dlsym(lib, "glClearColor"));
	if (!d3d8gles_pfn_glClearColor) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glClearColor in %s\n", libName); ok = false; }
	d3d8gles_pfn_glClearDepthf = reinterpret_cast<PFN_glClearDepthf>(dlsym(lib, "glClearDepthf"));
	if (!d3d8gles_pfn_glClearDepthf) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glClearDepthf in %s\n", libName); ok = false; }
	d3d8gles_pfn_glClearStencil = reinterpret_cast<PFN_glClearStencil>(dlsym(lib, "glClearStencil"));
	if (!d3d8gles_pfn_glClearStencil) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glClearStencil in %s\n", libName); ok = false; }
	d3d8gles_pfn_glColorMask = reinterpret_cast<PFN_glColorMask>(dlsym(lib, "glColorMask"));
	if (!d3d8gles_pfn_glColorMask) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glColorMask in %s\n", libName); ok = false; }
	d3d8gles_pfn_glCompileShader = reinterpret_cast<PFN_glCompileShader>(dlsym(lib, "glCompileShader"));
	if (!d3d8gles_pfn_glCompileShader) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glCompileShader in %s\n", libName); ok = false; }
	d3d8gles_pfn_glCompressedTexImage2D = reinterpret_cast<PFN_glCompressedTexImage2D>(dlsym(lib, "glCompressedTexImage2D"));
	if (!d3d8gles_pfn_glCompressedTexImage2D) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glCompressedTexImage2D in %s\n", libName); ok = false; }
	d3d8gles_pfn_glCreateProgram = reinterpret_cast<PFN_glCreateProgram>(dlsym(lib, "glCreateProgram"));
	if (!d3d8gles_pfn_glCreateProgram) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glCreateProgram in %s\n", libName); ok = false; }
	d3d8gles_pfn_glCreateShader = reinterpret_cast<PFN_glCreateShader>(dlsym(lib, "glCreateShader"));
	if (!d3d8gles_pfn_glCreateShader) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glCreateShader in %s\n", libName); ok = false; }
	d3d8gles_pfn_glCullFace = reinterpret_cast<PFN_glCullFace>(dlsym(lib, "glCullFace"));
	if (!d3d8gles_pfn_glCullFace) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glCullFace in %s\n", libName); ok = false; }
	d3d8gles_pfn_glDeleteBuffers = reinterpret_cast<PFN_glDeleteBuffers>(dlsym(lib, "glDeleteBuffers"));
	if (!d3d8gles_pfn_glDeleteBuffers) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glDeleteBuffers in %s\n", libName); ok = false; }
	d3d8gles_pfn_glDeleteFramebuffers = reinterpret_cast<PFN_glDeleteFramebuffers>(dlsym(lib, "glDeleteFramebuffers"));
	if (!d3d8gles_pfn_glDeleteFramebuffers) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glDeleteFramebuffers in %s\n", libName); ok = false; }
	d3d8gles_pfn_glDeleteProgram = reinterpret_cast<PFN_glDeleteProgram>(dlsym(lib, "glDeleteProgram"));
	if (!d3d8gles_pfn_glDeleteProgram) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glDeleteProgram in %s\n", libName); ok = false; }
	d3d8gles_pfn_glDeleteRenderbuffers = reinterpret_cast<PFN_glDeleteRenderbuffers>(dlsym(lib, "glDeleteRenderbuffers"));
	if (!d3d8gles_pfn_glDeleteRenderbuffers) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glDeleteRenderbuffers in %s\n", libName); ok = false; }
	d3d8gles_pfn_glDeleteShader = reinterpret_cast<PFN_glDeleteShader>(dlsym(lib, "glDeleteShader"));
	if (!d3d8gles_pfn_glDeleteShader) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glDeleteShader in %s\n", libName); ok = false; }
	d3d8gles_pfn_glDeleteTextures = reinterpret_cast<PFN_glDeleteTextures>(dlsym(lib, "glDeleteTextures"));
	if (!d3d8gles_pfn_glDeleteTextures) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glDeleteTextures in %s\n", libName); ok = false; }
	d3d8gles_pfn_glDeleteVertexArrays = reinterpret_cast<PFN_glDeleteVertexArrays>(dlsym(lib, "glDeleteVertexArrays"));
	if (!d3d8gles_pfn_glDeleteVertexArrays) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glDeleteVertexArrays in %s\n", libName); ok = false; }
	d3d8gles_pfn_glDepthFunc = reinterpret_cast<PFN_glDepthFunc>(dlsym(lib, "glDepthFunc"));
	if (!d3d8gles_pfn_glDepthFunc) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glDepthFunc in %s\n", libName); ok = false; }
	d3d8gles_pfn_glDepthMask = reinterpret_cast<PFN_glDepthMask>(dlsym(lib, "glDepthMask"));
	if (!d3d8gles_pfn_glDepthMask) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glDepthMask in %s\n", libName); ok = false; }
	d3d8gles_pfn_glDepthRangef = reinterpret_cast<PFN_glDepthRangef>(dlsym(lib, "glDepthRangef"));
	if (!d3d8gles_pfn_glDepthRangef) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glDepthRangef in %s\n", libName); ok = false; }
	d3d8gles_pfn_glDisable = reinterpret_cast<PFN_glDisable>(dlsym(lib, "glDisable"));
	if (!d3d8gles_pfn_glDisable) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glDisable in %s\n", libName); ok = false; }
	d3d8gles_pfn_glDisableVertexAttribArray = reinterpret_cast<PFN_glDisableVertexAttribArray>(dlsym(lib, "glDisableVertexAttribArray"));
	if (!d3d8gles_pfn_glDisableVertexAttribArray) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glDisableVertexAttribArray in %s\n", libName); ok = false; }
	d3d8gles_pfn_glDrawArrays = reinterpret_cast<PFN_glDrawArrays>(dlsym(lib, "glDrawArrays"));
	if (!d3d8gles_pfn_glDrawArrays) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glDrawArrays in %s\n", libName); ok = false; }
	d3d8gles_pfn_glDrawElements = reinterpret_cast<PFN_glDrawElements>(dlsym(lib, "glDrawElements"));
	if (!d3d8gles_pfn_glDrawElements) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glDrawElements in %s\n", libName); ok = false; }
	d3d8gles_pfn_glEnable = reinterpret_cast<PFN_glEnable>(dlsym(lib, "glEnable"));
	if (!d3d8gles_pfn_glEnable) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glEnable in %s\n", libName); ok = false; }
	d3d8gles_pfn_glEnableVertexAttribArray = reinterpret_cast<PFN_glEnableVertexAttribArray>(dlsym(lib, "glEnableVertexAttribArray"));
	if (!d3d8gles_pfn_glEnableVertexAttribArray) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glEnableVertexAttribArray in %s\n", libName); ok = false; }
	d3d8gles_pfn_glFinish = reinterpret_cast<PFN_glFinish>(dlsym(lib, "glFinish"));
	if (!d3d8gles_pfn_glFinish) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glFinish in %s\n", libName); ok = false; }
	d3d8gles_pfn_glFramebufferRenderbuffer = reinterpret_cast<PFN_glFramebufferRenderbuffer>(dlsym(lib, "glFramebufferRenderbuffer"));
	if (!d3d8gles_pfn_glFramebufferRenderbuffer) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glFramebufferRenderbuffer in %s\n", libName); ok = false; }
	d3d8gles_pfn_glFramebufferTexture2D = reinterpret_cast<PFN_glFramebufferTexture2D>(dlsym(lib, "glFramebufferTexture2D"));
	if (!d3d8gles_pfn_glFramebufferTexture2D) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glFramebufferTexture2D in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGenBuffers = reinterpret_cast<PFN_glGenBuffers>(dlsym(lib, "glGenBuffers"));
	if (!d3d8gles_pfn_glGenBuffers) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGenBuffers in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGenFramebuffers = reinterpret_cast<PFN_glGenFramebuffers>(dlsym(lib, "glGenFramebuffers"));
	if (!d3d8gles_pfn_glGenFramebuffers) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGenFramebuffers in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGenRenderbuffers = reinterpret_cast<PFN_glGenRenderbuffers>(dlsym(lib, "glGenRenderbuffers"));
	if (!d3d8gles_pfn_glGenRenderbuffers) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGenRenderbuffers in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGenTextures = reinterpret_cast<PFN_glGenTextures>(dlsym(lib, "glGenTextures"));
	if (!d3d8gles_pfn_glGenTextures) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGenTextures in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGenVertexArrays = reinterpret_cast<PFN_glGenVertexArrays>(dlsym(lib, "glGenVertexArrays"));
	if (!d3d8gles_pfn_glGenVertexArrays) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGenVertexArrays in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGenerateMipmap = reinterpret_cast<PFN_glGenerateMipmap>(dlsym(lib, "glGenerateMipmap"));
	if (!d3d8gles_pfn_glGenerateMipmap) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGenerateMipmap in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGetError = reinterpret_cast<PFN_glGetError>(dlsym(lib, "glGetError"));
	if (!d3d8gles_pfn_glGetError) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGetError in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGetIntegerv = reinterpret_cast<PFN_glGetIntegerv>(dlsym(lib, "glGetIntegerv"));
	if (!d3d8gles_pfn_glGetIntegerv) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGetIntegerv in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGetBooleanv = reinterpret_cast<PFN_glGetBooleanv>(dlsym(lib, "glGetBooleanv"));
	if (!d3d8gles_pfn_glGetBooleanv) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGetBooleanv in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGenQueries = reinterpret_cast<PFN_glGenQueries>(dlsym(lib, "glGenQueries"));
	if (!d3d8gles_pfn_glGenQueries) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGenQueries in %s\n", libName); ok = false; }
	d3d8gles_pfn_glBeginQuery = reinterpret_cast<PFN_glBeginQuery>(dlsym(lib, "glBeginQuery"));
	if (!d3d8gles_pfn_glBeginQuery) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glBeginQuery in %s\n", libName); ok = false; }
	d3d8gles_pfn_glEndQuery = reinterpret_cast<PFN_glEndQuery>(dlsym(lib, "glEndQuery"));
	if (!d3d8gles_pfn_glEndQuery) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glEndQuery in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGetQueryObjectuiv = reinterpret_cast<PFN_glGetQueryObjectuiv>(dlsym(lib, "glGetQueryObjectuiv"));
	if (!d3d8gles_pfn_glGetQueryObjectuiv) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGetQueryObjectuiv in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGetProgramInfoLog = reinterpret_cast<PFN_glGetProgramInfoLog>(dlsym(lib, "glGetProgramInfoLog"));
	if (!d3d8gles_pfn_glGetProgramInfoLog) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGetProgramInfoLog in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGetProgramiv = reinterpret_cast<PFN_glGetProgramiv>(dlsym(lib, "glGetProgramiv"));
	if (!d3d8gles_pfn_glGetProgramiv) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGetProgramiv in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGetShaderInfoLog = reinterpret_cast<PFN_glGetShaderInfoLog>(dlsym(lib, "glGetShaderInfoLog"));
	if (!d3d8gles_pfn_glGetShaderInfoLog) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGetShaderInfoLog in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGetShaderiv = reinterpret_cast<PFN_glGetShaderiv>(dlsym(lib, "glGetShaderiv"));
	if (!d3d8gles_pfn_glGetShaderiv) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGetShaderiv in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGetString = reinterpret_cast<PFN_glGetString>(dlsym(lib, "glGetString"));
	if (!d3d8gles_pfn_glGetString) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGetString in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGetUniformBlockIndex = reinterpret_cast<PFN_glGetUniformBlockIndex>(dlsym(lib, "glGetUniformBlockIndex"));
	if (!d3d8gles_pfn_glGetUniformBlockIndex) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGetUniformBlockIndex in %s\n", libName); ok = false; }
	d3d8gles_pfn_glGetUniformLocation = reinterpret_cast<PFN_glGetUniformLocation>(dlsym(lib, "glGetUniformLocation"));
	if (!d3d8gles_pfn_glGetUniformLocation) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glGetUniformLocation in %s\n", libName); ok = false; }
	d3d8gles_pfn_glLinkProgram = reinterpret_cast<PFN_glLinkProgram>(dlsym(lib, "glLinkProgram"));
	if (!d3d8gles_pfn_glLinkProgram) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glLinkProgram in %s\n", libName); ok = false; }
	d3d8gles_pfn_glPixelStorei = reinterpret_cast<PFN_glPixelStorei>(dlsym(lib, "glPixelStorei"));
	if (!d3d8gles_pfn_glPixelStorei) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glPixelStorei in %s\n", libName); ok = false; }
	d3d8gles_pfn_glReadPixels = reinterpret_cast<PFN_glReadPixels>(dlsym(lib, "glReadPixels"));
	if (!d3d8gles_pfn_glReadPixels) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glReadPixels in %s\n", libName); ok = false; }
	d3d8gles_pfn_glPolygonOffset = reinterpret_cast<PFN_glPolygonOffset>(dlsym(lib, "glPolygonOffset"));
	if (!d3d8gles_pfn_glPolygonOffset) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glPolygonOffset in %s\n", libName); ok = false; }
	d3d8gles_pfn_glRenderbufferStorage = reinterpret_cast<PFN_glRenderbufferStorage>(dlsym(lib, "glRenderbufferStorage"));
	if (!d3d8gles_pfn_glRenderbufferStorage) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glRenderbufferStorage in %s\n", libName); ok = false; }
	d3d8gles_pfn_glScissor = reinterpret_cast<PFN_glScissor>(dlsym(lib, "glScissor"));
	if (!d3d8gles_pfn_glScissor) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glScissor in %s\n", libName); ok = false; }
	d3d8gles_pfn_glShaderSource = reinterpret_cast<PFN_glShaderSource>(dlsym(lib, "glShaderSource"));
	if (!d3d8gles_pfn_glShaderSource) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glShaderSource in %s\n", libName); ok = false; }
	d3d8gles_pfn_glStencilFunc = reinterpret_cast<PFN_glStencilFunc>(dlsym(lib, "glStencilFunc"));
	if (!d3d8gles_pfn_glStencilFunc) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glStencilFunc in %s\n", libName); ok = false; }
	d3d8gles_pfn_glStencilMask = reinterpret_cast<PFN_glStencilMask>(dlsym(lib, "glStencilMask"));
	if (!d3d8gles_pfn_glStencilMask) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glStencilMask in %s\n", libName); ok = false; }
	d3d8gles_pfn_glStencilOp = reinterpret_cast<PFN_glStencilOp>(dlsym(lib, "glStencilOp"));
	if (!d3d8gles_pfn_glStencilOp) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glStencilOp in %s\n", libName); ok = false; }
	d3d8gles_pfn_glTexImage2D = reinterpret_cast<PFN_glTexImage2D>(dlsym(lib, "glTexImage2D"));
	if (!d3d8gles_pfn_glTexImage2D) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glTexImage2D in %s\n", libName); ok = false; }
	d3d8gles_pfn_glTexParameteri = reinterpret_cast<PFN_glTexParameteri>(dlsym(lib, "glTexParameteri"));
	if (!d3d8gles_pfn_glTexParameteri) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glTexParameteri in %s\n", libName); ok = false; }
	d3d8gles_pfn_glUniformBlockBinding = reinterpret_cast<PFN_glUniformBlockBinding>(dlsym(lib, "glUniformBlockBinding"));
	if (!d3d8gles_pfn_glUniformBlockBinding) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glUniformBlockBinding in %s\n", libName); ok = false; }
	d3d8gles_pfn_glUniform1f = reinterpret_cast<PFN_glUniform1f>(dlsym(lib, "glUniform1f"));
	if (!d3d8gles_pfn_glUniform1f) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glUniform1f in %s\n", libName); ok = false; }
	d3d8gles_pfn_glUniform1i = reinterpret_cast<PFN_glUniform1i>(dlsym(lib, "glUniform1i"));
	if (!d3d8gles_pfn_glUniform1i) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glUniform1i in %s\n", libName); ok = false; }
	d3d8gles_pfn_glUniform1iv = reinterpret_cast<PFN_glUniform1iv>(dlsym(lib, "glUniform1iv"));
	if (!d3d8gles_pfn_glUniform1iv) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glUniform1iv in %s\n", libName); ok = false; }
	d3d8gles_pfn_glUniform2f = reinterpret_cast<PFN_glUniform2f>(dlsym(lib, "glUniform2f"));
	if (!d3d8gles_pfn_glUniform2f) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glUniform2f in %s\n", libName); ok = false; }
	d3d8gles_pfn_glUniform3fv = reinterpret_cast<PFN_glUniform3fv>(dlsym(lib, "glUniform3fv"));
	if (!d3d8gles_pfn_glUniform3fv) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glUniform3fv in %s\n", libName); ok = false; }
	d3d8gles_pfn_glUniform4f = reinterpret_cast<PFN_glUniform4f>(dlsym(lib, "glUniform4f"));
	if (!d3d8gles_pfn_glUniform4f) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glUniform4f in %s\n", libName); ok = false; }
	d3d8gles_pfn_glUniform4fv = reinterpret_cast<PFN_glUniform4fv>(dlsym(lib, "glUniform4fv"));
	if (!d3d8gles_pfn_glUniform4fv) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glUniform4fv in %s\n", libName); ok = false; }
	d3d8gles_pfn_glUniformMatrix4fv = reinterpret_cast<PFN_glUniformMatrix4fv>(dlsym(lib, "glUniformMatrix4fv"));
	if (!d3d8gles_pfn_glUniformMatrix4fv) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glUniformMatrix4fv in %s\n", libName); ok = false; }
	d3d8gles_pfn_glUseProgram = reinterpret_cast<PFN_glUseProgram>(dlsym(lib, "glUseProgram"));
	if (!d3d8gles_pfn_glUseProgram) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glUseProgram in %s\n", libName); ok = false; }
	d3d8gles_pfn_glVertexAttribPointer = reinterpret_cast<PFN_glVertexAttribPointer>(dlsym(lib, "glVertexAttribPointer"));
	if (!d3d8gles_pfn_glVertexAttribPointer) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glVertexAttribPointer in %s\n", libName); ok = false; }
	d3d8gles_pfn_glViewport = reinterpret_cast<PFN_glViewport>(dlsym(lib, "glViewport"));
	if (!d3d8gles_pfn_glViewport) { fprintf(stderr, "[d3d8gles] GLES dispatch: missing symbol glViewport in %s\n", libName); ok = false; }

	if (!ok) return false;
	fprintf(stderr, "[d3d8gles] GLES dispatch: resolved %zu entry points from %s\n", static_cast<size_t>(87), libName);
	return true;
}
