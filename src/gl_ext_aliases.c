/*
 * Copyright (C) The MooGL Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * gl_ext_aliases.c
 * MGL
 *
 * The ARB/EXT/KHR-suffixed names of entry points MGL already has under their
 * core names. An extension MGL lists has to answer to its own names too:
 * a loader that asks a library for glDrawArraysInstancedARB and does not
 * find it may go on to the system OpenGL and get one that draws nothing here.
 */

#ifdef MGL_GL_CORE

#include <stdbool.h>
#include <stdint.h>

#include "glcorearb.h"
#include "gltypes.h"

void glDrawArraysInstancedARB(GLenum mode, GLint first, GLsizei count, GLsizei instancecount)
{
    glDrawArraysInstanced(mode, first, count, instancecount);
}

void glDrawElementsInstancedARB(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount)
{
    glDrawElementsInstanced(mode, count, type, indices, instancecount);
}

void glMultiDrawArraysIndirectCountARB(GLenum mode, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
    glMultiDrawArraysIndirectCount(mode, indirect, drawcount, maxdrawcount, stride);
}

void glMultiDrawElementsIndirectCountARB(GLenum mode, GLenum type, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
    glMultiDrawElementsIndirectCount(mode, type, indirect, drawcount, maxdrawcount, stride);
}

void glSpecializeShaderARB(GLuint shader, const GLchar *pEntryPoint, GLuint numSpecializationConstants, const GLuint *pConstantIndex, const GLuint *pConstantValue)
{
    glSpecializeShader(shader, pEntryPoint, numSpecializationConstants, pConstantIndex, pConstantValue);
}

void glVertexAttribDivisorARB(GLuint index, GLuint divisor)
{
    glVertexAttribDivisor(index, divisor);
}


// GL_ARB_multitexture, which a compatibility context lists
void glActiveTexture(GLenum texture);
void glClientActiveTexture(GLenum texture);
void glActiveTextureARB(GLenum texture) { glActiveTexture(texture); }
void glClientActiveTextureARB(GLenum texture) { glClientActiveTexture(texture); }
void glMultiTexCoord1d(GLenum target, GLdouble s);
void glMultiTexCoord1dARB(GLenum target, GLdouble s) { glMultiTexCoord1d(target, s); }
void glMultiTexCoord1dv(GLenum target, const GLdouble *v);
void glMultiTexCoord1dvARB(GLenum target, const GLdouble *v) { glMultiTexCoord1dv(target, v); }
void glMultiTexCoord1f(GLenum target, GLfloat s);
void glMultiTexCoord1fARB(GLenum target, GLfloat s) { glMultiTexCoord1f(target, s); }
void glMultiTexCoord1fv(GLenum target, const GLfloat *v);
void glMultiTexCoord1fvARB(GLenum target, const GLfloat *v) { glMultiTexCoord1fv(target, v); }
void glMultiTexCoord1i(GLenum target, GLint s);
void glMultiTexCoord1iARB(GLenum target, GLint s) { glMultiTexCoord1i(target, s); }
void glMultiTexCoord1iv(GLenum target, const GLint *v);
void glMultiTexCoord1ivARB(GLenum target, const GLint *v) { glMultiTexCoord1iv(target, v); }
void glMultiTexCoord1s(GLenum target, GLshort s);
void glMultiTexCoord1sARB(GLenum target, GLshort s) { glMultiTexCoord1s(target, s); }
void glMultiTexCoord1sv(GLenum target, const GLshort *v);
void glMultiTexCoord1svARB(GLenum target, const GLshort *v) { glMultiTexCoord1sv(target, v); }
void glMultiTexCoord2d(GLenum target, GLdouble s, GLdouble t);
void glMultiTexCoord2dARB(GLenum target, GLdouble s, GLdouble t) { glMultiTexCoord2d(target, s, t); }
void glMultiTexCoord2dv(GLenum target, const GLdouble *v);
void glMultiTexCoord2dvARB(GLenum target, const GLdouble *v) { glMultiTexCoord2dv(target, v); }
void glMultiTexCoord2f(GLenum target, GLfloat s, GLfloat t);
void glMultiTexCoord2fARB(GLenum target, GLfloat s, GLfloat t) { glMultiTexCoord2f(target, s, t); }
void glMultiTexCoord2fv(GLenum target, const GLfloat *v);
void glMultiTexCoord2fvARB(GLenum target, const GLfloat *v) { glMultiTexCoord2fv(target, v); }
void glMultiTexCoord2i(GLenum target, GLint s, GLint t);
void glMultiTexCoord2iARB(GLenum target, GLint s, GLint t) { glMultiTexCoord2i(target, s, t); }
void glMultiTexCoord2iv(GLenum target, const GLint *v);
void glMultiTexCoord2ivARB(GLenum target, const GLint *v) { glMultiTexCoord2iv(target, v); }
void glMultiTexCoord2s(GLenum target, GLshort s, GLshort t);
void glMultiTexCoord2sARB(GLenum target, GLshort s, GLshort t) { glMultiTexCoord2s(target, s, t); }
void glMultiTexCoord2sv(GLenum target, const GLshort *v);
void glMultiTexCoord2svARB(GLenum target, const GLshort *v) { glMultiTexCoord2sv(target, v); }
void glMultiTexCoord3d(GLenum target, GLdouble s, GLdouble t, GLdouble r);
void glMultiTexCoord3dARB(GLenum target, GLdouble s, GLdouble t, GLdouble r) { glMultiTexCoord3d(target, s, t, r); }
void glMultiTexCoord3dv(GLenum target, const GLdouble *v);
void glMultiTexCoord3dvARB(GLenum target, const GLdouble *v) { glMultiTexCoord3dv(target, v); }
void glMultiTexCoord3f(GLenum target, GLfloat s, GLfloat t, GLfloat r);
void glMultiTexCoord3fARB(GLenum target, GLfloat s, GLfloat t, GLfloat r) { glMultiTexCoord3f(target, s, t, r); }
void glMultiTexCoord3fv(GLenum target, const GLfloat *v);
void glMultiTexCoord3fvARB(GLenum target, const GLfloat *v) { glMultiTexCoord3fv(target, v); }
void glMultiTexCoord3i(GLenum target, GLint s, GLint t, GLint r);
void glMultiTexCoord3iARB(GLenum target, GLint s, GLint t, GLint r) { glMultiTexCoord3i(target, s, t, r); }
void glMultiTexCoord3iv(GLenum target, const GLint *v);
void glMultiTexCoord3ivARB(GLenum target, const GLint *v) { glMultiTexCoord3iv(target, v); }
void glMultiTexCoord3s(GLenum target, GLshort s, GLshort t, GLshort r);
void glMultiTexCoord3sARB(GLenum target, GLshort s, GLshort t, GLshort r) { glMultiTexCoord3s(target, s, t, r); }
void glMultiTexCoord3sv(GLenum target, const GLshort *v);
void glMultiTexCoord3svARB(GLenum target, const GLshort *v) { glMultiTexCoord3sv(target, v); }
void glMultiTexCoord4d(GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q);
void glMultiTexCoord4dARB(GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q) { glMultiTexCoord4d(target, s, t, r, q); }
void glMultiTexCoord4dv(GLenum target, const GLdouble *v);
void glMultiTexCoord4dvARB(GLenum target, const GLdouble *v) { glMultiTexCoord4dv(target, v); }
void glMultiTexCoord4f(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
void glMultiTexCoord4fARB(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q) { glMultiTexCoord4f(target, s, t, r, q); }
void glMultiTexCoord4fv(GLenum target, const GLfloat *v);
void glMultiTexCoord4fvARB(GLenum target, const GLfloat *v) { glMultiTexCoord4fv(target, v); }
void glMultiTexCoord4i(GLenum target, GLint s, GLint t, GLint r, GLint q);
void glMultiTexCoord4iARB(GLenum target, GLint s, GLint t, GLint r, GLint q) { glMultiTexCoord4i(target, s, t, r, q); }
void glMultiTexCoord4iv(GLenum target, const GLint *v);
void glMultiTexCoord4ivARB(GLenum target, const GLint *v) { glMultiTexCoord4iv(target, v); }
void glMultiTexCoord4s(GLenum target, GLshort s, GLshort t, GLshort r, GLshort q);
void glMultiTexCoord4sARB(GLenum target, GLshort s, GLshort t, GLshort r, GLshort q) { glMultiTexCoord4s(target, s, t, r, q); }
void glMultiTexCoord4sv(GLenum target, const GLshort *v);
void glMultiTexCoord4svARB(GLenum target, const GLshort *v) { glMultiTexCoord4sv(target, v); }

// GL_EXT_framebuffer_object, which a compatibility context lists
GLboolean glIsRenderbufferEXT(GLuint renderbuffer) { return glIsRenderbuffer(renderbuffer); }
void glBindRenderbufferEXT(GLenum target, GLuint renderbuffer) { glBindRenderbuffer(target, renderbuffer); }
void glDeleteRenderbuffersEXT(GLsizei n, const GLuint *renderbuffers) { glDeleteRenderbuffers(n, renderbuffers); }
void glGenRenderbuffersEXT(GLsizei n, GLuint *renderbuffers) { glGenRenderbuffers(n, renderbuffers); }
void glRenderbufferStorageEXT(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    glRenderbufferStorage(target, internalformat, width, height);
}
void glGetRenderbufferParameterivEXT(GLenum target, GLenum pname, GLint *params)
{
    glGetRenderbufferParameteriv(target, pname, params);
}
GLboolean glIsFramebufferEXT(GLuint framebuffer) { return glIsFramebuffer(framebuffer); }
void glBindFramebufferEXT(GLenum target, GLuint framebuffer) { glBindFramebuffer(target, framebuffer); }
void glDeleteFramebuffersEXT(GLsizei n, const GLuint *framebuffers) { glDeleteFramebuffers(n, framebuffers); }
void glGenFramebuffersEXT(GLsizei n, GLuint *framebuffers) { glGenFramebuffers(n, framebuffers); }
GLenum glCheckFramebufferStatusEXT(GLenum target) { return glCheckFramebufferStatus(target); }
void glFramebufferTexture1DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    glFramebufferTexture1D(target, attachment, textarget, texture, level);
}
void glFramebufferTexture2DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    glFramebufferTexture2D(target, attachment, textarget, texture, level);
}
void glFramebufferTexture3DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level,
                               GLint zoffset)
{
    glFramebufferTexture3D(target, attachment, textarget, texture, level, zoffset);
}
void glFramebufferRenderbufferEXT(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
    glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}
void glGetFramebufferAttachmentParameterivEXT(GLenum target, GLenum attachment, GLenum pname, GLint *params)
{
    glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}
void glGenerateMipmapEXT(GLenum target) { glGenerateMipmap(target); }

// GL_ARB_vertex_buffer_object
void glBindBufferARB(GLenum target, GLuint buffer) { glBindBuffer(target, buffer); }
void glBufferDataARB(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
    glBufferData(target, size, data, usage);
}
void glBufferSubDataARB(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
    glBufferSubData(target, offset, size, data);
}
void glDeleteBuffersARB(GLsizei n, const GLuint *buffers) { glDeleteBuffers(n, buffers); }
void glGenBuffersARB(GLsizei n, GLuint *buffers) { glGenBuffers(n, buffers); }
GLboolean glIsBufferARB(GLuint buffer) { return glIsBuffer(buffer); }
void *glMapBufferARB(GLenum target, GLenum access) { return glMapBuffer(target, access); }
GLboolean glUnmapBufferARB(GLenum target) { return glUnmapBuffer(target); }
void glGetBufferParameterivARB(GLenum target, GLenum pname, GLint *params)
{
    glGetBufferParameteriv(target, pname, params);
}
void glGetBufferPointervARB(GLenum target, GLenum pname, void **params) { glGetBufferPointerv(target, pname, params); }
void glGetBufferSubDataARB(GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
    glGetBufferSubData(target, offset, size, data);
}

// GL_ARB_vertex_shader
void glVertexAttrib1dARB(GLuint index, GLdouble x) { glVertexAttrib1d(index, x); }
void glVertexAttrib1dvARB(GLuint index, const GLdouble *v) { glVertexAttrib1dv(index, v); }
void glVertexAttrib1fARB(GLuint index, GLfloat x) { glVertexAttrib1f(index, x); }
void glVertexAttrib1fvARB(GLuint index, const GLfloat *v) { glVertexAttrib1fv(index, v); }
void glVertexAttrib1sARB(GLuint index, GLshort x) { glVertexAttrib1s(index, x); }
void glVertexAttrib1svARB(GLuint index, const GLshort *v) { glVertexAttrib1sv(index, v); }
void glVertexAttrib2dARB(GLuint index, GLdouble x, GLdouble y) { glVertexAttrib2d(index, x, y); }
void glVertexAttrib2dvARB(GLuint index, const GLdouble *v) { glVertexAttrib2dv(index, v); }
void glVertexAttrib2fARB(GLuint index, GLfloat x, GLfloat y) { glVertexAttrib2f(index, x, y); }
void glVertexAttrib2fvARB(GLuint index, const GLfloat *v) { glVertexAttrib2fv(index, v); }
void glVertexAttrib2sARB(GLuint index, GLshort x, GLshort y) { glVertexAttrib2s(index, x, y); }
void glVertexAttrib2svARB(GLuint index, const GLshort *v) { glVertexAttrib2sv(index, v); }
void glVertexAttrib3dARB(GLuint index, GLdouble x, GLdouble y, GLdouble z) { glVertexAttrib3d(index, x, y, z); }
void glVertexAttrib3dvARB(GLuint index, const GLdouble *v) { glVertexAttrib3dv(index, v); }
void glVertexAttrib3fARB(GLuint index, GLfloat x, GLfloat y, GLfloat z) { glVertexAttrib3f(index, x, y, z); }
void glVertexAttrib3fvARB(GLuint index, const GLfloat *v) { glVertexAttrib3fv(index, v); }
void glVertexAttrib3sARB(GLuint index, GLshort x, GLshort y, GLshort z) { glVertexAttrib3s(index, x, y, z); }
void glVertexAttrib3svARB(GLuint index, const GLshort *v) { glVertexAttrib3sv(index, v); }
void glVertexAttrib4NbvARB(GLuint index, const GLbyte *v) { glVertexAttrib4Nbv(index, v); }
void glVertexAttrib4NivARB(GLuint index, const GLint *v) { glVertexAttrib4Niv(index, v); }
void glVertexAttrib4NsvARB(GLuint index, const GLshort *v) { glVertexAttrib4Nsv(index, v); }
void glVertexAttrib4NubARB(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w)
{
    glVertexAttrib4Nub(index, x, y, z, w);
}
void glVertexAttrib4NubvARB(GLuint index, const GLubyte *v) { glVertexAttrib4Nubv(index, v); }
void glVertexAttrib4NuivARB(GLuint index, const GLuint *v) { glVertexAttrib4Nuiv(index, v); }
void glVertexAttrib4NusvARB(GLuint index, const GLushort *v) { glVertexAttrib4Nusv(index, v); }
void glVertexAttrib4bvARB(GLuint index, const GLbyte *v) { glVertexAttrib4bv(index, v); }
void glVertexAttrib4dARB(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    glVertexAttrib4d(index, x, y, z, w);
}
void glVertexAttrib4dvARB(GLuint index, const GLdouble *v) { glVertexAttrib4dv(index, v); }
void glVertexAttrib4fARB(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    glVertexAttrib4f(index, x, y, z, w);
}
void glVertexAttrib4fvARB(GLuint index, const GLfloat *v) { glVertexAttrib4fv(index, v); }
void glVertexAttrib4ivARB(GLuint index, const GLint *v) { glVertexAttrib4iv(index, v); }
void glVertexAttrib4sARB(GLuint index, GLshort x, GLshort y, GLshort z, GLshort w)
{
    glVertexAttrib4s(index, x, y, z, w);
}
void glVertexAttrib4svARB(GLuint index, const GLshort *v) { glVertexAttrib4sv(index, v); }
void glVertexAttrib4ubvARB(GLuint index, const GLubyte *v) { glVertexAttrib4ubv(index, v); }
void glVertexAttrib4uivARB(GLuint index, const GLuint *v) { glVertexAttrib4uiv(index, v); }
void glVertexAttrib4usvARB(GLuint index, const GLushort *v) { glVertexAttrib4usv(index, v); }
void glVertexAttribPointerARB(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)
{
    glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}
void glEnableVertexAttribArrayARB(GLuint index) { glEnableVertexAttribArray(index); }
void glDisableVertexAttribArrayARB(GLuint index) { glDisableVertexAttribArray(index); }
void glGetVertexAttribdvARB(GLuint index, GLenum pname, GLdouble *params) { glGetVertexAttribdv(index, pname, params); }
void glGetVertexAttribfvARB(GLuint index, GLenum pname, GLfloat *params) { glGetVertexAttribfv(index, pname, params); }
void glGetVertexAttribivARB(GLuint index, GLenum pname, GLint *params) { glGetVertexAttribiv(index, pname, params); }
void glGetVertexAttribPointervARB(GLuint index, GLenum pname, void **pointer)
{
    glGetVertexAttribPointerv(index, pname, pointer);
}

// GL_ARB_shader_objects, the calls without a handle
void glUniform1fARB(GLint location, GLfloat v0) { glUniform1f(location, v0); }
void glUniform1iARB(GLint location, GLint v0) { glUniform1i(location, v0); }
void glUniform1fvARB(GLint location, GLsizei count, const GLfloat *value) { glUniform1fv(location, count, value); }
void glUniform1ivARB(GLint location, GLsizei count, const GLint *value) { glUniform1iv(location, count, value); }
void glUniform2fARB(GLint location, GLfloat v0, GLfloat v1) { glUniform2f(location, v0, v1); }
void glUniform2iARB(GLint location, GLint v0, GLint v1) { glUniform2i(location, v0, v1); }
void glUniform2fvARB(GLint location, GLsizei count, const GLfloat *value) { glUniform2fv(location, count, value); }
void glUniform2ivARB(GLint location, GLsizei count, const GLint *value) { glUniform2iv(location, count, value); }
void glUniform3fARB(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) { glUniform3f(location, v0, v1, v2); }
void glUniform3iARB(GLint location, GLint v0, GLint v1, GLint v2) { glUniform3i(location, v0, v1, v2); }
void glUniform3fvARB(GLint location, GLsizei count, const GLfloat *value) { glUniform3fv(location, count, value); }
void glUniform3ivARB(GLint location, GLsizei count, const GLint *value) { glUniform3iv(location, count, value); }
void glUniform4fARB(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
    glUniform4f(location, v0, v1, v2, v3);
}
void glUniform4iARB(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) { glUniform4i(location, v0, v1, v2, v3); }
void glUniform4fvARB(GLint location, GLsizei count, const GLfloat *value) { glUniform4fv(location, count, value); }
void glUniform4ivARB(GLint location, GLsizei count, const GLint *value) { glUniform4iv(location, count, value); }
void glUniformMatrix2fvARB(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    glUniformMatrix2fv(location, count, transpose, value);
}
void glUniformMatrix3fvARB(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    glUniformMatrix3fv(location, count, transpose, value);
}
void glUniformMatrix4fvARB(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    glUniformMatrix4fv(location, count, transpose, value);
}

// GL_EXT_blend_minmax, GL_EXT_blend_subtract, GL_EXT_blend_equation_separate, GL_EXT_blend_func_separate
void glBlendEquationEXT(GLenum mode) { glBlendEquation(mode); }
void glBlendEquationSeparateEXT(GLenum modeRGB, GLenum modeAlpha) { glBlendEquationSeparate(modeRGB, modeAlpha); }
void glBlendFuncSeparateEXT(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha)
{
    glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}

// GL_EXT_framebuffer_blit, GL_EXT_framebuffer_multisample
void glBlitFramebufferEXT(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}
void glRenderbufferStorageMultisampleEXT(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
    glRenderbufferStorageMultisample(target, samples, internalformat, width, height);
}

// GL_ARB_shader_objects and the handle calls of GL_ARB_vertex_shader. Apple's
// GLhandleARB is a pointer; it holds the GL name.
#define NAME(h) ((GLuint)(uintptr_t)(h))
#define HANDLE(n) ((GLhandleARB)(uintptr_t)(n))

#define GL_PROGRAM_OBJECT_ARB 0x8B40
#define GL_SHADER_OBJECT_ARB 0x8B48
#define GL_OBJECT_TYPE_ARB 0x8B4E
#define GL_OBJECT_SUBTYPE_ARB 0x8B4F

void glDeleteObjectARB(GLhandleARB obj)
{
    if (glIsProgram(NAME(obj)))
        glDeleteProgram(NAME(obj));
    else
        glDeleteShader(NAME(obj));
}

GLhandleARB glGetHandleARB(GLenum pname)
{
    GLint program = 0;

    glGetIntegerv(pname == GL_PROGRAM_OBJECT_ARB ? GL_CURRENT_PROGRAM : pname, &program);
    return HANDLE(program);
}

void glDetachObjectARB(GLhandleARB container, GLhandleARB obj) { glDetachShader(NAME(container), NAME(obj)); }
GLhandleARB glCreateShaderObjectARB(GLenum type) { return HANDLE(glCreateShader(type)); }

void glShaderSourceARB(GLhandleARB obj, GLsizei count, const GLcharARB **string, const GLint *length)
{
    glShaderSource(NAME(obj), count, string, length);
}

void glCompileShaderARB(GLhandleARB obj) { glCompileShader(NAME(obj)); }
GLhandleARB glCreateProgramObjectARB(void) { return HANDLE(glCreateProgram()); }
void glAttachObjectARB(GLhandleARB container, GLhandleARB obj) { glAttachShader(NAME(container), NAME(obj)); }
void glLinkProgramARB(GLhandleARB obj) { glLinkProgram(NAME(obj)); }
void glUseProgramObjectARB(GLhandleARB obj) { glUseProgram(NAME(obj)); }
void glValidateProgramARB(GLhandleARB obj) { glValidateProgram(NAME(obj)); }

void glGetObjectParameterivARB(GLhandleARB obj, GLenum pname, GLint *params)
{
    const bool program = glIsProgram(NAME(obj));

    if (pname == GL_OBJECT_TYPE_ARB)
        *params = program ? GL_PROGRAM_OBJECT_ARB : GL_SHADER_OBJECT_ARB;
    else if (pname == GL_OBJECT_SUBTYPE_ARB && !program)
        glGetShaderiv(NAME(obj), GL_SHADER_TYPE, params);
    else if (program)
        glGetProgramiv(NAME(obj), pname, params);
    else
        glGetShaderiv(NAME(obj), pname, params);
}

void glGetObjectParameterfvARB(GLhandleARB obj, GLenum pname, GLfloat *params)
{
    GLint v = 0;

    glGetObjectParameterivARB(obj, pname, &v);
    *params = (GLfloat)v;
}

void glGetInfoLogARB(GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog)
{
    if (glIsProgram(NAME(obj)))
        glGetProgramInfoLog(NAME(obj), maxLength, length, infoLog);
    else
        glGetShaderInfoLog(NAME(obj), maxLength, length, infoLog);
}

void glGetAttachedObjectsARB(GLhandleARB container, GLsizei maxCount, GLsizei *count, GLhandleARB *obj)
{
    GLuint names[64];
    GLsizei n = 0;

    glGetAttachedShaders(NAME(container), maxCount < 64 ? maxCount : 64, &n, names);
    for (GLsizei i = 0; i < n; i++)
        obj[i] = HANDLE(names[i]);
    if (count)
        *count = n;
}

GLint glGetUniformLocationARB(GLhandleARB obj, const GLcharARB *name) { return glGetUniformLocation(NAME(obj), name); }

void glGetActiveUniformARB(GLhandleARB obj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size,
                           GLenum *type, GLcharARB *name)
{
    glGetActiveUniform(NAME(obj), index, maxLength, length, size, type, name);
}

void glGetUniformfvARB(GLhandleARB obj, GLint location, GLfloat *params) { glGetUniformfv(NAME(obj), location, params); }
void glGetUniformivARB(GLhandleARB obj, GLint location, GLint *params) { glGetUniformiv(NAME(obj), location, params); }

void glGetShaderSourceARB(GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *source)
{
    glGetShaderSource(NAME(obj), maxLength, length, source);
}

void glBindAttribLocationARB(GLhandleARB obj, GLuint index, const GLcharARB *name)
{
    glBindAttribLocation(NAME(obj), index, name);
}

void glGetActiveAttribARB(GLhandleARB obj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size,
                          GLenum *type, GLcharARB *name)
{
    glGetActiveAttrib(NAME(obj), index, maxLength, length, size, type, name);
}

GLint glGetAttribLocationARB(GLhandleARB obj, const GLcharARB *name) { return glGetAttribLocation(NAME(obj), name); }

// GL_KHR_debug, which an ES context lists, names its calls with KHR
void glDebugMessageControlKHR(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids,
                              GLboolean enabled)
{
    glDebugMessageControl(source, type, severity, count, ids, enabled);
}

void glDebugMessageInsertKHR(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                             const GLchar *buf)
{
    glDebugMessageInsert(source, type, id, severity, length, buf);
}

void glDebugMessageCallbackKHR(GLDEBUGPROC callback, const void *userParam)
{
    glDebugMessageCallback(callback, userParam);
}

GLuint glGetDebugMessageLogKHR(GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids,
                               GLenum *severities, GLsizei *lengths, GLchar *messageLog)
{
    return glGetDebugMessageLog(count, bufSize, sources, types, ids, severities, lengths, messageLog);
}

void glPushDebugGroupKHR(GLenum source, GLuint id, GLsizei length, const GLchar *message)
{
    glPushDebugGroup(source, id, length, message);
}

void glPopDebugGroupKHR(void) { glPopDebugGroup(); }

void glObjectLabelKHR(GLenum identifier, GLuint name, GLsizei length, const GLchar *label)
{
    glObjectLabel(identifier, name, length, label);
}

void glGetObjectLabelKHR(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label)
{
    glGetObjectLabel(identifier, name, bufSize, length, label);
}

void glObjectPtrLabelKHR(const void *ptr, GLsizei length, const GLchar *label)
{
    glObjectPtrLabel(ptr, length, label);
}

void glGetObjectPtrLabelKHR(const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label)
{
    glGetObjectPtrLabel(ptr, bufSize, length, label);
}

void glGetPointervKHR(GLenum pname, void **params) { glGetPointerv(pname, params); }

#endif
