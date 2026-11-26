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
 * apple_gl.c
 * MGL
 *
 * The rest of the names Apple's libGL exports, so a program built against
 * OpenGL.framework finds every symbol it binds at launch. Suffixed names
 * call the core function; Apple's small extensions do what they say; the
 * extensions MooGL does not list raise GL_INVALID_OPERATION.
 */

#ifdef MGL_GL_CORE

#include <stdint.h>
#include <string.h>

#include "glm_context.h"

extern __thread GLMContext _ctx;
extern void mgl_lazy_init(void);

typedef struct _CGLContextObject *CGLContextObj;
CGLContextObj CGLGetCurrentContext(void);
int CGLFlushDrawable(CGLContextObj ctx);

typedef void *GLhandleARB_;
typedef int64_t GLint64EXT;
typedef uint64_t GLuint64EXT;
typedef char GLcharARB;

static void unsupported(const char *func)
{
    GLMContext ctx = (mgl_lazy_init(), _ctx);

    if (ctx)
        ctx->error_func(ctx, func, GL_INVALID_OPERATION);
}

GLboolean glIsQuery(GLuint id);
GLboolean glIsVertexArray(GLuint array);
GLint glGetFragDataLocation(GLuint program, const GLchar *name);
void glBeginConditionalRender(GLuint id, GLenum mode);
void glBeginQuery(GLenum target, GLuint id);
void glBeginTransformFeedback(GLenum primitiveMode);
void glBindBufferBase(GLenum target, GLuint index, GLuint buffer);
void glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
void glBindFragDataLocation(GLuint program, GLuint color, const GLchar *name);
void glBindVertexArray(GLuint array);
void glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
void glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha);
void glBlendEquationi(GLuint buf, GLenum mode);
void glBlendFuncSeparatei(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
void glBlendFunci(GLuint buf, GLenum src, GLenum dst);
void glClampColor(GLenum target, GLenum clamp);
void glCompressedTexImage1D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *data);
void glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data);
void glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data);
void glCompressedTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data);
void glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data);
void glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data);
void glDeleteQueries(GLsizei n, const GLuint *ids);
void glDeleteVertexArrays(GLsizei n, const GLuint *arrays);
void glDrawBuffers(GLsizei n, const GLenum *bufs);
void glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices);
void glEndConditionalRender(void);
void glEndQuery(GLenum target);
void glEndTransformFeedback(void);
void glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length);
void glFogCoordPointer(GLenum type, GLsizei stride, const void *pointer);
void glFogCoordd(GLdouble coord);
void glFogCoorddv(const GLdouble *coord);
void glFogCoordf(GLfloat coord);
void glFogCoordfv(const GLfloat *coord);
void glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level);
void glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
void glGenQueries(GLsizei n, GLuint *ids);
void glGenVertexArrays(GLsizei n, GLuint *arrays);
void glGetCompressedTexImage(GLenum target, GLint level, void *img);
void glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64 *params);
void glGetQueryObjectiv(GLuint id, GLenum pname, GLint *params);
void glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64 *params);
void glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params);
void glGetQueryiv(GLenum target, GLenum pname, GLint *params);
void glGetTexParameterIiv(GLenum target, GLenum pname, GLint *params);
void glGetTexParameterIuiv(GLenum target, GLenum pname, GLuint *params);
void glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name);
void glGetUniformuiv(GLuint program, GLint location, GLuint *params);
void glGetVertexAttribIiv(GLuint index, GLenum pname, GLint *params);
void glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params);
void glLoadTransposeMatrixd(const GLdouble *m);
void glLoadTransposeMatrixf(const GLfloat *m);
void glMinSampleShading(GLfloat value);
void glMultTransposeMatrixd(const GLdouble *m);
void glMultTransposeMatrixf(const GLfloat *m);
void glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount);
void glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount);
void glPointParameterf(GLenum pname, GLfloat param);
void glPointParameterfv(GLenum pname, const GLfloat *params);
void glPointParameteri(GLenum pname, GLint param);
void glPointParameteriv(GLenum pname, const GLint *params);
void glProgramParameteri(GLuint program, GLenum pname, GLint value);
void glProvokingVertex(GLenum mode);
void glSampleCoverage(GLfloat value, GLboolean invert);
void glSecondaryColor3b(GLbyte red, GLbyte green, GLbyte blue);
void glSecondaryColor3bv(const GLbyte *v);
void glSecondaryColor3d(GLdouble red, GLdouble green, GLdouble blue);
void glSecondaryColor3dv(const GLdouble *v);
void glSecondaryColor3f(GLfloat red, GLfloat green, GLfloat blue);
void glSecondaryColor3fv(const GLfloat *v);
void glSecondaryColor3i(GLint red, GLint green, GLint blue);
void glSecondaryColor3iv(const GLint *v);
void glSecondaryColor3s(GLshort red, GLshort green, GLshort blue);
void glSecondaryColor3sv(const GLshort *v);
void glSecondaryColor3ub(GLubyte red, GLubyte green, GLubyte blue);
void glSecondaryColor3ubv(const GLubyte *v);
void glSecondaryColor3ui(GLuint red, GLuint green, GLuint blue);
void glSecondaryColor3uiv(const GLuint *v);
void glSecondaryColor3us(GLushort red, GLushort green, GLushort blue);
void glSecondaryColor3usv(const GLushort *v);
void glSecondaryColorPointer(GLint size, GLenum type, GLsizei stride, const void *pointer);
void glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
void glTexParameterIiv(GLenum target, GLenum pname, const GLint *params);
void glTexParameterIuiv(GLenum target, GLenum pname, const GLuint *params);
void glTextureBarrier(void);
void glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar *const*varyings, GLenum bufferMode);
void glUniform1ui(GLint location, GLuint v0);
void glUniform1uiv(GLint location, GLsizei count, const GLuint *value);
void glUniform2ui(GLint location, GLuint v0, GLuint v1);
void glUniform2uiv(GLint location, GLsizei count, const GLuint *value);
void glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2);
void glUniform3uiv(GLint location, GLsizei count, const GLuint *value);
void glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
void glUniform4uiv(GLint location, GLsizei count, const GLuint *value);
void glVertexAttribI1i(GLuint index, GLint x);
void glVertexAttribI1iv(GLuint index, const GLint *v);
void glVertexAttribI1ui(GLuint index, GLuint x);
void glVertexAttribI1uiv(GLuint index, const GLuint *v);
void glVertexAttribI2i(GLuint index, GLint x, GLint y);
void glVertexAttribI2iv(GLuint index, const GLint *v);
void glVertexAttribI2ui(GLuint index, GLuint x, GLuint y);
void glVertexAttribI2uiv(GLuint index, const GLuint *v);
void glVertexAttribI3i(GLuint index, GLint x, GLint y, GLint z);
void glVertexAttribI3iv(GLuint index, const GLint *v);
void glVertexAttribI3ui(GLuint index, GLuint x, GLuint y, GLuint z);
void glVertexAttribI3uiv(GLuint index, const GLuint *v);
void glVertexAttribI4bv(GLuint index, const GLbyte *v);
void glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w);
void glVertexAttribI4iv(GLuint index, const GLint *v);
void glVertexAttribI4sv(GLuint index, const GLshort *v);
void glVertexAttribI4ubv(GLuint index, const GLubyte *v);
void glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
void glVertexAttribI4uiv(GLuint index, const GLuint *v);
void glVertexAttribI4usv(GLuint index, const GLushort *v);
void glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer);
void glWindowPos2d(GLdouble x, GLdouble y);
void glWindowPos2dv(const GLdouble *v);
void glWindowPos2f(GLfloat x, GLfloat y);
void glWindowPos2fv(const GLfloat *v);
void glWindowPos2i(GLint x, GLint y);
void glWindowPos2iv(const GLint *v);
void glWindowPos2s(GLshort x, GLshort y);
void glWindowPos2sv(const GLshort *v);
void glWindowPos3d(GLdouble x, GLdouble y, GLdouble z);
void glWindowPos3dv(const GLdouble *v);
void glWindowPos3f(GLfloat x, GLfloat y, GLfloat z);
void glWindowPos3fv(const GLfloat *v);
void glWindowPos3i(GLint x, GLint y, GLint z);
void glWindowPos3iv(const GLint *v);
void glWindowPos3s(GLshort x, GLshort y, GLshort z);
void glWindowPos3sv(const GLshort *v);

// Apple's extensions
// Apple's own names for flush, finish and swap

void glFlushRenderAPPLE(void)
{
    glFlush();
}

void glFinishRenderAPPLE(void)
{
    glFinish();
}

void glSwapAPPLE(void)
{
    CGLContextObj ctx = CGLGetCurrentContext();

    if (ctx)
        CGLFlushDrawable(ctx);
}

// GL_APPLE_fence, done with glFlush and glFinish: a fence is always reached
// by the time anyone can wait on it.

static GLuint next_fence = 1;

void glGenFencesAPPLE(GLsizei n, GLuint *fences)
{
    for (GLsizei i = 0; fences && i < n; i++)
        fences[i] = __atomic_fetch_add(&next_fence, 1, __ATOMIC_RELAXED);
}

void glDeleteFencesAPPLE(GLsizei n, const GLuint *fences)
{
}

void glSetFenceAPPLE(GLuint fence)
{
    glFlush();
}

GLboolean glIsFenceAPPLE(GLuint fence)
{
    return fence != 0 && fence < next_fence;
}

GLboolean glTestFenceAPPLE(GLuint fence)
{
    return GL_TRUE;
}

void glFinishFenceAPPLE(GLuint fence)
{
    glFinish();
}

GLboolean glTestObjectAPPLE(GLenum object, GLuint name)
{
    return GL_TRUE;
}

void glFinishObjectAPPLE(GLenum object, GLint name)
{
    glFinish();
}

// GL_APPLE_object_purgeable: nothing is ever purged

#define MGL_RELEASED_APPLE  0x8A19
#define MGL_VOLATILE_APPLE  0x8A1A
#define MGL_RETAINED_APPLE  0x8A1B
#define MGL_PURGEABLE_APPLE 0x8A1D

GLenum glObjectPurgeableAPPLE(GLenum objectType, GLuint name, GLenum option)
{
    return option == MGL_VOLATILE_APPLE ? MGL_VOLATILE_APPLE : MGL_RELEASED_APPLE;
}

GLenum glObjectUnpurgeableAPPLE(GLenum objectType, GLuint name, GLenum option)
{
    return MGL_RETAINED_APPLE;
}

void glGetObjectParameterivAPPLE(GLenum objectType, GLuint name, GLenum pname, GLint *params)
{
    if (pname != MGL_PURGEABLE_APPLE)
    {
        unsupported(__FUNCTION__);
        return;
    }
    *params = GL_FALSE;
}

// GL_EXT_draw_buffers2's names for the indexed calls

void glEnableIndexedEXT(GLenum target, GLuint index)
{
    glEnablei(target, index);
}

void glDisableIndexedEXT(GLenum target, GLuint index)
{
    glDisablei(target, index);
}

GLboolean glIsEnabledIndexedEXT(GLenum target, GLuint index)
{
    return glIsEnabledi(target, index);
}

void glColorMaskIndexedEXT(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
    glColorMaski(index, r, g, b, a);
}

void glGetBooleanIndexedvEXT(GLenum target, GLuint index, GLboolean *data)
{
    glGetBooleani_v(target, index, data);
}

void glGetIntegerIndexedvEXT(GLenum target, GLuint index, GLint *data)
{
    glGetIntegeri_v(target, index, data);
}

void glFramebufferTextureFaceEXT(GLenum target, GLenum attachment, GLuint texture, GLint level, GLenum face)
{
    glFramebufferTextureLayer(target, attachment, texture, level, (GLint)(face - GL_TEXTURE_CUBE_MAP_POSITIVE_X));
}

// GL_EXT_transform_feedback's offset binding runs to the end of the buffer
void glBindBufferOffsetEXT(GLenum target, GLuint index, GLuint buffer, GLintptr offset)
{
    GLint64 size = 0;

    if (buffer == 0)
    {
        glBindBufferBase(target, index, 0);
        return;
    }
    glGetNamedBufferParameteri64v(buffer, GL_BUFFER_SIZE, &size);
    glBindBufferRange(target, index, buffer, offset, size - offset);
}

// ATI's two-sided stencil takes both faces' functions at once
void glStencilFuncSeparateATI(GLenum frontfunc, GLenum backfunc, GLint ref, GLuint mask)
{
    glStencilFuncSeparate(GL_FRONT, frontfunc, ref, mask);
    glStencilFuncSeparate(GL_BACK, backfunc, ref, mask);
}

// GL_ARB_vertex_program's programs, which MooGL does not have; the GLSL
// calls of the same name without ARB are another thing entirely
GLboolean glIsProgramARB(GLuint program)
{
    return GL_FALSE;
}

void glGetProgramivARB(GLenum target, GLenum pname, GLint *params)
{
    unsupported(__FUNCTION__);
}

// Apple-only calls with no published signature
void glMultiDrawElementsIndirectAPPLE(void)
{
    unsupported(__FUNCTION__);
}

void glMultiDrawElementsIndirectBaseInstanceAPPLE(void)
{
    unsupported(__FUNCTION__);
}

void glLabelObjectWithResponsibleProcessAPPLE(void)
{
}

void glSamplePass(void)
{
}

void glSamplePassARB(void)
{
}

void gll_noop(void)
{
}

// GL_EXT_debug_label, on KHR_debug's labels

static GLenum khrIdentifier(GLenum type)
{
    switch (type)
    {
        case 0x9151: return GL_BUFFER;
        case 0x8B48: return GL_SHADER;
        case 0x8B40: return GL_PROGRAM;
        case 0x9154: return GL_VERTEX_ARRAY;
        case 0x9153: return GL_QUERY;
        case 0x8A4F: return GL_PROGRAM_PIPELINE;
        default: return type;
    }
}

void glLabelObjectEXT(GLenum type, GLuint object, GLsizei length, const GLchar *label)
{
    glObjectLabel(khrIdentifier(type), object, length == 0 ? -1 : length, label);
}

void glGetObjectLabelEXT(GLenum type, GLuint object, GLsizei bufSize, GLsizei *length, GLchar *label)
{
    glGetObjectLabel(khrIdentifier(type), object, bufSize, length, label);
}

// GL_EXT_debug_marker: markers only show in a GPU capture, which MooGL's
// Metal labels already cover
void glInsertEventMarkerEXT(GLsizei length, const GLchar *marker)
{
}

void glPushGroupMarkerEXT(GLsizei length, const GLchar *marker)
{
}

void glPopGroupMarkerEXT(void)
{
}

// GL_APPLE_element_array, on glDrawElements

static __thread GLenum element_type = GL_UNSIGNED_INT;
static __thread const char *element_pointer;

static size_t elementSize(void)
{
    return element_type == GL_UNSIGNED_BYTE ? 1 : element_type == GL_UNSIGNED_SHORT ? 2 : 4;
}

void glElementPointerAPPLE(GLenum type, const void *pointer)
{
    element_type = type;
    element_pointer = pointer;
}

void glDrawElementArrayAPPLE(GLenum mode, GLint first, GLsizei count)
{
    glDrawElements(mode, count, element_type, element_pointer + first * elementSize());
}

void glDrawRangeElementArrayAPPLE(GLenum mode, GLuint start, GLuint end, GLint first, GLsizei count)
{
    glDrawRangeElements(mode, start, end, count, element_type, element_pointer + first * elementSize());
}

void glMultiDrawElementArrayAPPLE(GLenum mode, const GLint *first, const GLsizei *count, GLsizei primcount)
{
    for (GLsizei i = 0; i < primcount; i++)
        glDrawElementArrayAPPLE(mode, first[i], count[i]);
}

void glMultiDrawRangeElementArrayAPPLE(GLenum mode, GLuint start, GLuint end, const GLint *first, const GLsizei *count, GLsizei primcount)
{
    for (GLsizei i = 0; i < primcount; i++)
        glDrawRangeElementArrayAPPLE(mode, start, end, first[i], count[i]);
}

// suffixed names of core functions

void glBeginConditionalRenderNV(GLuint id, GLenum mode)
{
    glBeginConditionalRender(id, mode);
}

void glBeginQueryARB(GLenum target, GLuint id)
{
    glBeginQuery(target, id);
}

void glBeginTransformFeedbackEXT(GLenum primitiveMode)
{
    glBeginTransformFeedback(primitiveMode);
}

void glBindBufferBaseEXT(GLenum target, GLuint index, GLuint buffer)
{
    glBindBufferBase(target, index, buffer);
}

void glBindBufferRangeEXT(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    glBindBufferRange(target, index, buffer, offset, size);
}

void glBindFragDataLocationEXT(GLuint program, GLuint color, const GLchar *name)
{
    glBindFragDataLocation(program, color, name);
}

void glBindVertexArrayAPPLE(GLuint array)
{
    glBindVertexArray(array);
}

void glBlendColorEXT(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    glBlendColor(red, green, blue, alpha);
}

void glBlendEquationSeparateATI(GLenum equationRGB, GLenum equationAlpha)
{
    glBlendEquationSeparate(equationRGB, equationAlpha);
}

void glBlendEquationSeparateiARB(GLuint buf, GLenum modeRGB, GLenum modeAlpha)
{
    glBlendEquationSeparatei(buf, modeRGB, modeAlpha);
}

void glBlendEquationiARB(GLuint buf, GLenum mode)
{
    glBlendEquationi(buf, mode);
}

void glBlendFuncSeparateiARB(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
    glBlendFuncSeparatei(buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void glBlendFunciARB(GLuint buf, GLenum src, GLenum dst)
{
    glBlendFunci(buf, src, dst);
}

void glClampColorARB(GLenum target, GLenum clamp)
{
    glClampColor(target, clamp);
}

void glCompressedTexImage1DARB(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *data)
{
    glCompressedTexImage1D(target, level, internalformat, width, border, imageSize, data);
}

void glCompressedTexImage2DARB(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data)
{
    glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
}

void glCompressedTexImage3DARB(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data)
{
    glCompressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, data);
}

void glCompressedTexSubImage1DARB(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data)
{
    glCompressedTexSubImage1D(target, level, xoffset, width, format, imageSize, data);
}

void glCompressedTexSubImage2DARB(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data)
{
    glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

void glCompressedTexSubImage3DARB(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data)
{
    glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}

void glDeleteQueriesARB(GLsizei n, const GLuint *ids)
{
    glDeleteQueries(n, ids);
}

void glDeleteVertexArraysAPPLE(GLsizei n, const GLuint *arrays)
{
    glDeleteVertexArrays(n, arrays);
}

void glDrawBuffersARB(GLsizei n, const GLenum *bufs)
{
    glDrawBuffers(n, bufs);
}

void glDrawRangeElementsEXT(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices)
{
    glDrawRangeElements(mode, start, end, count, type, indices);
}

void glEndConditionalRenderNV(void)
{
    glEndConditionalRender();
}

void glEndQueryARB(GLenum target)
{
    glEndQuery(target);
}

void glEndTransformFeedbackEXT(void)
{
    glEndTransformFeedback();
}

void glFlushMappedBufferRangeAPPLE(GLenum target, GLintptr offset, GLsizeiptr size)
{
    glFlushMappedBufferRange(target, offset, size);
}

void glFogCoordPointerEXT(GLenum type, GLsizei stride, const void *pointer)
{
    glFogCoordPointer(type, stride, pointer);
}

void glFogCoorddEXT(GLdouble coord)
{
    glFogCoordd(coord);
}

void glFogCoorddvEXT(const GLdouble *coord)
{
    glFogCoorddv(coord);
}

void glFogCoordfEXT(GLfloat coord)
{
    glFogCoordf(coord);
}

void glFogCoordfvEXT(const GLfloat *coord)
{
    glFogCoordfv(coord);
}

void glFramebufferTextureEXT(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
    glFramebufferTexture(target, attachment, texture, level);
}

void glFramebufferTextureLayerEXT(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
    glFramebufferTextureLayer(target, attachment, texture, level, layer);
}

void glGenQueriesARB(GLsizei n, GLuint *ids)
{
    glGenQueries(n, ids);
}

void glGenVertexArraysAPPLE(GLsizei n, GLuint *arrays)
{
    glGenVertexArrays(n, arrays);
}

void glGetCompressedTexImageARB(GLenum target, GLint level, void *img)
{
    glGetCompressedTexImage(target, level, img);
}

GLint glGetFragDataLocationEXT(GLuint program, const GLchar *name)
{
    return glGetFragDataLocation(program, name);
}

void glGetQueryObjecti64vEXT(GLuint id, GLenum pname, GLint64 *params)
{
    glGetQueryObjecti64v(id, pname, params);
}

void glGetQueryObjectivARB(GLuint id, GLenum pname, GLint *params)
{
    glGetQueryObjectiv(id, pname, params);
}

void glGetQueryObjectui64vEXT(GLuint id, GLenum pname, GLuint64 *params)
{
    glGetQueryObjectui64v(id, pname, params);
}

void glGetQueryObjectuivARB(GLuint id, GLenum pname, GLuint *params)
{
    glGetQueryObjectuiv(id, pname, params);
}

void glGetQueryivARB(GLenum target, GLenum pname, GLint *params)
{
    glGetQueryiv(target, pname, params);
}

void glGetTexParameterIivEXT(GLenum target, GLenum pname, GLint *params)
{
    glGetTexParameterIiv(target, pname, params);
}

void glGetTexParameterIuivEXT(GLenum target, GLenum pname, GLuint *params)
{
    glGetTexParameterIuiv(target, pname, params);
}

void glGetTransformFeedbackVaryingEXT(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name)
{
    glGetTransformFeedbackVarying(program, index, bufSize, length, size, type, name);
}

void glGetUniformuivEXT(GLuint program, GLint location, GLuint *params)
{
    glGetUniformuiv(program, location, params);
}

void glGetVertexAttribIivEXT(GLuint index, GLenum pname, GLint *params)
{
    glGetVertexAttribIiv(index, pname, params);
}

void glGetVertexAttribIuivEXT(GLuint index, GLenum pname, GLuint *params)
{
    glGetVertexAttribIuiv(index, pname, params);
}

GLboolean glIsQueryARB(GLuint id)
{
    return glIsQuery(id);
}

GLboolean glIsVertexArrayAPPLE(GLuint array)
{
    return glIsVertexArray(array);
}

void glLoadTransposeMatrixdARB(const GLdouble *m)
{
    glLoadTransposeMatrixd(m);
}

void glLoadTransposeMatrixfARB(const GLfloat *m)
{
    glLoadTransposeMatrixf(m);
}

void glMinSampleShadingARB(GLfloat value)
{
    glMinSampleShading(value);
}

void glMultTransposeMatrixdARB(const GLdouble *m)
{
    glMultTransposeMatrixd(m);
}

void glMultTransposeMatrixfARB(const GLfloat *m)
{
    glMultTransposeMatrixf(m);
}

void glMultiDrawArraysEXT(GLenum mode, const GLint *first, const GLsizei *count, GLsizei primcount)
{
    glMultiDrawArrays(mode, first, count, primcount);
}

void glMultiDrawElementsEXT(GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei primcount)
{
    glMultiDrawElements(mode, count, type, indices, primcount);
}

void glPointParameterfARB(GLenum pname, GLfloat param)
{
    glPointParameterf(pname, param);
}

void glPointParameterfvARB(GLenum pname, const GLfloat *params)
{
    glPointParameterfv(pname, params);
}

void glPointParameteriNV(GLenum pname, GLint param)
{
    glPointParameteri(pname, param);
}

void glPointParameterivNV(GLenum pname, const GLint *params)
{
    glPointParameteriv(pname, params);
}

void glProgramParameteriEXT(GLuint program, GLenum pname, GLint value)
{
    glProgramParameteri(program, pname, value);
}

void glProvokingVertexEXT(GLenum mode)
{
    glProvokingVertex(mode);
}

void glSampleCoverageARB(GLfloat value, GLboolean invert)
{
    glSampleCoverage(value, invert);
}

void glSecondaryColor3bEXT(GLbyte red, GLbyte green, GLbyte blue)
{
    glSecondaryColor3b(red, green, blue);
}

void glSecondaryColor3bvEXT(const GLbyte *v)
{
    glSecondaryColor3bv(v);
}

void glSecondaryColor3dEXT(GLdouble red, GLdouble green, GLdouble blue)
{
    glSecondaryColor3d(red, green, blue);
}

void glSecondaryColor3dvEXT(const GLdouble *v)
{
    glSecondaryColor3dv(v);
}

void glSecondaryColor3fEXT(GLfloat red, GLfloat green, GLfloat blue)
{
    glSecondaryColor3f(red, green, blue);
}

void glSecondaryColor3fvEXT(const GLfloat *v)
{
    glSecondaryColor3fv(v);
}

void glSecondaryColor3iEXT(GLint red, GLint green, GLint blue)
{
    glSecondaryColor3i(red, green, blue);
}

void glSecondaryColor3ivEXT(const GLint *v)
{
    glSecondaryColor3iv(v);
}

void glSecondaryColor3sEXT(GLshort red, GLshort green, GLshort blue)
{
    glSecondaryColor3s(red, green, blue);
}

void glSecondaryColor3svEXT(const GLshort *v)
{
    glSecondaryColor3sv(v);
}

void glSecondaryColor3ubEXT(GLubyte red, GLubyte green, GLubyte blue)
{
    glSecondaryColor3ub(red, green, blue);
}

void glSecondaryColor3ubvEXT(const GLubyte *v)
{
    glSecondaryColor3ubv(v);
}

void glSecondaryColor3uiEXT(GLuint red, GLuint green, GLuint blue)
{
    glSecondaryColor3ui(red, green, blue);
}

void glSecondaryColor3uivEXT(const GLuint *v)
{
    glSecondaryColor3uiv(v);
}

void glSecondaryColor3usEXT(GLushort red, GLushort green, GLushort blue)
{
    glSecondaryColor3us(red, green, blue);
}

void glSecondaryColor3usvEXT(const GLushort *v)
{
    glSecondaryColor3usv(v);
}

void glSecondaryColorPointerEXT(GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    glSecondaryColorPointer(size, type, stride, pointer);
}

void glStencilOpSeparateATI(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass)
{
    glStencilOpSeparate(face, sfail, dpfail, dppass);
}

void glTexParameterIivEXT(GLenum target, GLenum pname, const GLint *params)
{
    glTexParameterIiv(target, pname, params);
}

void glTexParameterIuivEXT(GLenum target, GLenum pname, const GLuint *params)
{
    glTexParameterIuiv(target, pname, params);
}

void glTextureBarrierNV(void)
{
    glTextureBarrier();
}

void glTransformFeedbackVaryingsEXT(GLuint program, GLsizei count, const GLchar *const*varyings, GLenum bufferMode)
{
    glTransformFeedbackVaryings(program, count, varyings, bufferMode);
}

void glUniform1uiEXT(GLint location, GLuint v0)
{
    glUniform1ui(location, v0);
}

void glUniform1uivEXT(GLint location, GLsizei count, const GLuint *value)
{
    glUniform1uiv(location, count, value);
}

void glUniform2uiEXT(GLint location, GLuint v0, GLuint v1)
{
    glUniform2ui(location, v0, v1);
}

void glUniform2uivEXT(GLint location, GLsizei count, const GLuint *value)
{
    glUniform2uiv(location, count, value);
}

void glUniform3uiEXT(GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    glUniform3ui(location, v0, v1, v2);
}

void glUniform3uivEXT(GLint location, GLsizei count, const GLuint *value)
{
    glUniform3uiv(location, count, value);
}

void glUniform4uiEXT(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    glUniform4ui(location, v0, v1, v2, v3);
}

void glUniform4uivEXT(GLint location, GLsizei count, const GLuint *value)
{
    glUniform4uiv(location, count, value);
}

void glVertexAttribI1iEXT(GLuint index, GLint x)
{
    glVertexAttribI1i(index, x);
}

void glVertexAttribI1ivEXT(GLuint index, const GLint *v)
{
    glVertexAttribI1iv(index, v);
}

void glVertexAttribI1uiEXT(GLuint index, GLuint x)
{
    glVertexAttribI1ui(index, x);
}

void glVertexAttribI1uivEXT(GLuint index, const GLuint *v)
{
    glVertexAttribI1uiv(index, v);
}

void glVertexAttribI2iEXT(GLuint index, GLint x, GLint y)
{
    glVertexAttribI2i(index, x, y);
}

void glVertexAttribI2ivEXT(GLuint index, const GLint *v)
{
    glVertexAttribI2iv(index, v);
}

void glVertexAttribI2uiEXT(GLuint index, GLuint x, GLuint y)
{
    glVertexAttribI2ui(index, x, y);
}

void glVertexAttribI2uivEXT(GLuint index, const GLuint *v)
{
    glVertexAttribI2uiv(index, v);
}

void glVertexAttribI3iEXT(GLuint index, GLint x, GLint y, GLint z)
{
    glVertexAttribI3i(index, x, y, z);
}

void glVertexAttribI3ivEXT(GLuint index, const GLint *v)
{
    glVertexAttribI3iv(index, v);
}

void glVertexAttribI3uiEXT(GLuint index, GLuint x, GLuint y, GLuint z)
{
    glVertexAttribI3ui(index, x, y, z);
}

void glVertexAttribI3uivEXT(GLuint index, const GLuint *v)
{
    glVertexAttribI3uiv(index, v);
}

void glVertexAttribI4bvEXT(GLuint index, const GLbyte *v)
{
    glVertexAttribI4bv(index, v);
}

void glVertexAttribI4iEXT(GLuint index, GLint x, GLint y, GLint z, GLint w)
{
    glVertexAttribI4i(index, x, y, z, w);
}

void glVertexAttribI4ivEXT(GLuint index, const GLint *v)
{
    glVertexAttribI4iv(index, v);
}

void glVertexAttribI4svEXT(GLuint index, const GLshort *v)
{
    glVertexAttribI4sv(index, v);
}

void glVertexAttribI4ubvEXT(GLuint index, const GLubyte *v)
{
    glVertexAttribI4ubv(index, v);
}

void glVertexAttribI4uiEXT(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
{
    glVertexAttribI4ui(index, x, y, z, w);
}

void glVertexAttribI4uivEXT(GLuint index, const GLuint *v)
{
    glVertexAttribI4uiv(index, v);
}

void glVertexAttribI4usvEXT(GLuint index, const GLushort *v)
{
    glVertexAttribI4usv(index, v);
}

void glVertexAttribIPointerEXT(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    glVertexAttribIPointer(index, size, type, stride, pointer);
}

void glWindowPos2dARB(GLdouble x, GLdouble y)
{
    glWindowPos2d(x, y);
}

void glWindowPos2dvARB(const GLdouble *v)
{
    glWindowPos2dv(v);
}

void glWindowPos2fARB(GLfloat x, GLfloat y)
{
    glWindowPos2f(x, y);
}

void glWindowPos2fvARB(const GLfloat *v)
{
    glWindowPos2fv(v);
}

void glWindowPos2iARB(GLint x, GLint y)
{
    glWindowPos2i(x, y);
}

void glWindowPos2ivARB(const GLint *v)
{
    glWindowPos2iv(v);
}

void glWindowPos2sARB(GLshort x, GLshort y)
{
    glWindowPos2s(x, y);
}

void glWindowPos2svARB(const GLshort *v)
{
    glWindowPos2sv(v);
}

void glWindowPos3dARB(GLdouble x, GLdouble y, GLdouble z)
{
    glWindowPos3d(x, y, z);
}

void glWindowPos3dvARB(const GLdouble *v)
{
    glWindowPos3dv(v);
}

void glWindowPos3fARB(GLfloat x, GLfloat y, GLfloat z)
{
    glWindowPos3f(x, y, z);
}

void glWindowPos3fvARB(const GLfloat *v)
{
    glWindowPos3fv(v);
}

void glWindowPos3iARB(GLint x, GLint y, GLint z)
{
    glWindowPos3i(x, y, z);
}

void glWindowPos3ivARB(const GLint *v)
{
    glWindowPos3iv(v);
}

void glWindowPos3sARB(GLshort x, GLshort y, GLshort z)
{
    glWindowPos3s(x, y, z);
}

void glWindowPos3svARB(const GLshort *v)
{
    glWindowPos3sv(v);
}

// extensions MooGL does not list

void glActiveStencilFaceEXT(GLenum face)
{
    unsupported(__FUNCTION__);
}

void glBindProgramARB(GLenum target, GLuint program)
{
    unsupported(__FUNCTION__);
}

void glBufferParameteriAPPLE(GLenum target, GLenum pname, GLint param)
{
    unsupported(__FUNCTION__);
}

void glClearColorIiEXT(GLint red, GLint green, GLint blue, GLint alpha)
{
    unsupported(__FUNCTION__);
}

void glClearColorIuiEXT(GLuint red, GLuint green, GLuint blue, GLuint alpha)
{
    unsupported(__FUNCTION__);
}

void glColorSubTable(GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const void *data)
{
    unsupported(__FUNCTION__);
}

void glColorSubTableEXT(GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const void *data)
{
    unsupported(__FUNCTION__);
}

void glColorTable(GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const void *table)
{
    unsupported(__FUNCTION__);
}

void glColorTableEXT(GLenum target, GLenum internalFormat, GLsizei width, GLenum format, GLenum type, const void *table)
{
    unsupported(__FUNCTION__);
}

void glColorTableParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glColorTableParameteriv(GLenum target, GLenum pname, const GLint *params)
{
    unsupported(__FUNCTION__);
}

void glCombinerInputNV(GLenum stage, GLenum portion, GLenum variable, GLenum input, GLenum mapping, GLenum componentUsage)
{
    unsupported(__FUNCTION__);
}

void glCombinerOutputNV(GLenum stage, GLenum portion, GLenum abOutput, GLenum cdOutput, GLenum sumOutput, GLenum scale, GLenum bias, GLboolean abDotProduct, GLboolean cdDotProduct, GLboolean muxSum)
{
    unsupported(__FUNCTION__);
}

void glCombinerParameterfNV(GLenum pname, GLfloat param)
{
    unsupported(__FUNCTION__);
}

void glCombinerParameterfvNV(GLenum pname, const GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glCombinerParameteriNV(GLenum pname, GLint param)
{
    unsupported(__FUNCTION__);
}

void glCombinerParameterivNV(GLenum pname, const GLint *params)
{
    unsupported(__FUNCTION__);
}

void glCombinerStageParameterfvNV(GLenum stage, GLenum pname, const GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glCompileShaderIncludeARB(GLuint shader, GLsizei count, const GLchar *const*path, const GLint *length)
{
    unsupported(__FUNCTION__);
}

void glConvolutionFilter1D(GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const void *image)
{
    unsupported(__FUNCTION__);
}

void glConvolutionFilter2D(GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *image)
{
    unsupported(__FUNCTION__);
}

void glConvolutionParameterf(GLenum target, GLenum pname, GLfloat params)
{
    unsupported(__FUNCTION__);
}

void glConvolutionParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glConvolutionParameteri(GLenum target, GLenum pname, GLint params)
{
    unsupported(__FUNCTION__);
}

void glConvolutionParameteriv(GLenum target, GLenum pname, const GLint *params)
{
    unsupported(__FUNCTION__);
}

void glCopyColorSubTable(GLenum target, GLsizei start, GLint x, GLint y, GLsizei width)
{
    unsupported(__FUNCTION__);
}

void glCopyColorTable(GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width)
{
    unsupported(__FUNCTION__);
}

void glCopyConvolutionFilter1D(GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width)
{
    unsupported(__FUNCTION__);
}

void glCopyConvolutionFilter2D(GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height)
{
    unsupported(__FUNCTION__);
}

void glDeleteNamedStringARB(GLint namelen, const GLchar *name)
{
    unsupported(__FUNCTION__);
}

void glDeleteProgramsARB(GLsizei n, const GLuint *programs)
{
    unsupported(__FUNCTION__);
}

void glDepthBoundsEXT(GLclampd zmin, GLclampd zmax)
{
    unsupported(__FUNCTION__);
}

void glDisableVertexAttribAPPLE(GLuint index, GLenum pname)
{
    unsupported(__FUNCTION__);
}

void glEnableVertexAttribAPPLE(GLuint index, GLenum pname)
{
    unsupported(__FUNCTION__);
}

void glFinalCombinerInputNV(GLenum variable, GLenum input, GLenum mapping, GLenum componentUsage)
{
    unsupported(__FUNCTION__);
}

void glFlushVertexArrayRangeAPPLE(GLsizei length, void *pointer)
{
    unsupported(__FUNCTION__);
}

void glGenProgramsARB(GLsizei n, GLuint *programs)
{
    unsupported(__FUNCTION__);
}

void glGetColorTable(GLenum target, GLenum format, GLenum type, void *table)
{
    unsupported(__FUNCTION__);
}

void glGetColorTableEXT(GLenum target, GLenum format, GLenum type, void *data)
{
    unsupported(__FUNCTION__);
}

void glGetColorTableParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glGetColorTableParameterfvEXT(GLenum target, GLenum pname, GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glGetColorTableParameteriv(GLenum target, GLenum pname, GLint *params)
{
    unsupported(__FUNCTION__);
}

void glGetColorTableParameterivEXT(GLenum target, GLenum pname, GLint *params)
{
    unsupported(__FUNCTION__);
}

void glGetCombinerInputParameterfvNV(GLenum stage, GLenum portion, GLenum variable, GLenum pname, GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glGetCombinerInputParameterivNV(GLenum stage, GLenum portion, GLenum variable, GLenum pname, GLint *params)
{
    unsupported(__FUNCTION__);
}

void glGetCombinerOutputParameterfvNV(GLenum stage, GLenum portion, GLenum pname, GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glGetCombinerOutputParameterivNV(GLenum stage, GLenum portion, GLenum pname, GLint *params)
{
    unsupported(__FUNCTION__);
}

void glGetCombinerStageParameterfvNV(GLenum stage, GLenum pname, GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glGetConvolutionFilter(GLenum target, GLenum format, GLenum type, void *image)
{
    unsupported(__FUNCTION__);
}

void glGetConvolutionParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glGetConvolutionParameteriv(GLenum target, GLenum pname, GLint *params)
{
    unsupported(__FUNCTION__);
}

void glGetFinalCombinerInputParameterfvNV(GLenum variable, GLenum pname, GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glGetFinalCombinerInputParameterivNV(GLenum variable, GLenum pname, GLint *params)
{
    unsupported(__FUNCTION__);
}

void glGetHistogram(GLenum target, GLboolean reset, GLenum format, GLenum type, void *values)
{
    unsupported(__FUNCTION__);
}

void glGetHistogramParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glGetHistogramParameteriv(GLenum target, GLenum pname, GLint *params)
{
    unsupported(__FUNCTION__);
}

void glGetMinmax(GLenum target, GLboolean reset, GLenum format, GLenum type, void *values)
{
    unsupported(__FUNCTION__);
}

void glGetMinmaxParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glGetMinmaxParameteriv(GLenum target, GLenum pname, GLint *params)
{
    unsupported(__FUNCTION__);
}

void glGetNamedStringARB(GLint namelen, const GLchar *name, GLsizei bufSize, GLint *stringlen, GLchar *string)
{
    unsupported(__FUNCTION__);
}

void glGetNamedStringivARB(GLint namelen, const GLchar *name, GLenum pname, GLint *params)
{
    unsupported(__FUNCTION__);
}

void glGetProgramEnvParameterdvARB(GLenum target, GLuint index, GLdouble *params)
{
    unsupported(__FUNCTION__);
}

void glGetProgramEnvParameterfvARB(GLenum target, GLuint index, GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glGetProgramLocalParameterdvARB(GLenum target, GLuint index, GLdouble *params)
{
    unsupported(__FUNCTION__);
}

void glGetProgramLocalParameterfvARB(GLenum target, GLuint index, GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glGetProgramStringARB(GLenum target, GLenum pname, void *string)
{
    unsupported(__FUNCTION__);
}

void glGetSeparableFilter(GLenum target, GLenum format, GLenum type, void *row, void *column, void *span)
{
    unsupported(__FUNCTION__);
}

GLint glGetUniformBufferSizeEXT(GLuint program, GLint location)
{
    unsupported(__FUNCTION__);
    return 0;
}

GLintptr glGetUniformOffsetEXT(GLuint program, GLint location)
{
    unsupported(__FUNCTION__);
    return 0;
}

void glHistogram(GLenum target, GLsizei width, GLenum internalformat, GLboolean sink)
{
    unsupported(__FUNCTION__);
}

GLboolean glIsNamedStringARB(GLint namelen, const GLchar *name)
{
    unsupported(__FUNCTION__);
    return 0;
}

GLboolean glIsVertexAttribEnabledAPPLE(GLuint index, GLenum pname)
{
    unsupported(__FUNCTION__);
    return 0;
}

void glLockArraysEXT(GLint first, GLsizei count)
{
    unsupported(__FUNCTION__);
}

void glMapVertexAttrib1dAPPLE(GLuint index, GLuint size, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points)
{
    unsupported(__FUNCTION__);
}

void glMapVertexAttrib1fAPPLE(GLuint index, GLuint size, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points)
{
    unsupported(__FUNCTION__);
}

void glMapVertexAttrib2dAPPLE(GLuint index, GLuint size, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points)
{
    unsupported(__FUNCTION__);
}

void glMapVertexAttrib2fAPPLE(GLuint index, GLuint size, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points)
{
    unsupported(__FUNCTION__);
}

void glMinmax(GLenum target, GLenum internalformat, GLboolean sink)
{
    unsupported(__FUNCTION__);
}

void glNamedStringARB(GLenum type, GLint namelen, const GLchar *name, GLint stringlen, const GLchar *string)
{
    unsupported(__FUNCTION__);
}

void glPNTrianglesfATI(GLenum pname, GLfloat param)
{
    unsupported(__FUNCTION__);
}

void glPNTrianglesfATIX(GLenum pname, GLfloat param)
{
    unsupported(__FUNCTION__);
}

void glPNTrianglesiATI(GLenum pname, GLint param)
{
    unsupported(__FUNCTION__);
}

void glPNTrianglesiATIX(GLenum pname, GLint param)
{
    unsupported(__FUNCTION__);
}

void glPointSizePointerAPPLE(GLenum type, GLsizei stride, const GLvoid *pointer)
{
    unsupported(__FUNCTION__);
}

void glProgramEnvParameter4dARB(GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    unsupported(__FUNCTION__);
}

void glProgramEnvParameter4dvARB(GLenum target, GLuint index, const GLdouble *params)
{
    unsupported(__FUNCTION__);
}

void glProgramEnvParameter4fARB(GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    unsupported(__FUNCTION__);
}

void glProgramEnvParameter4fvARB(GLenum target, GLuint index, const GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glProgramEnvParameters4fvEXT(GLenum target, GLuint index, GLsizei count, const GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glProgramLocalParameter4dARB(GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    unsupported(__FUNCTION__);
}

void glProgramLocalParameter4dvARB(GLenum target, GLuint index, const GLdouble *params)
{
    unsupported(__FUNCTION__);
}

void glProgramLocalParameter4fARB(GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    unsupported(__FUNCTION__);
}

void glProgramLocalParameter4fvARB(GLenum target, GLuint index, const GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glProgramLocalParameters4fvEXT(GLenum target, GLuint index, GLsizei count, const GLfloat *params)
{
    unsupported(__FUNCTION__);
}

void glProgramStringARB(GLenum target, GLenum format, GLsizei len, const void *string)
{
    unsupported(__FUNCTION__);
}

void glResetHistogram(GLenum target)
{
    unsupported(__FUNCTION__);
}

void glResetMinmax(GLenum target)
{
    unsupported(__FUNCTION__);
}

void glSeparableFilter2D(GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *row, const void *column)
{
    unsupported(__FUNCTION__);
}

void glUniformBufferEXT(GLuint program, GLint location, GLuint buffer)
{
    unsupported(__FUNCTION__);
}

void glUnlockArraysEXT(void)
{
    unsupported(__FUNCTION__);
}

void glVertexArrayParameteriAPPLE(GLenum pname, GLint param)
{
    unsupported(__FUNCTION__);
}

void glVertexArrayRangeAPPLE(GLsizei length, void *pointer)
{
    unsupported(__FUNCTION__);
}

void glVertexBlendARB(GLint count)
{
    unsupported(__FUNCTION__);
}

void glVertexPointSizefAPPLE(GLfloat size)
{
    unsupported(__FUNCTION__);
}

void glWeightPointerARB(GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    unsupported(__FUNCTION__);
}

void glWeightbvARB(GLint size, const GLbyte *weights)
{
    unsupported(__FUNCTION__);
}

void glWeightdvARB(GLint size, const GLdouble *weights)
{
    unsupported(__FUNCTION__);
}

void glWeightfvARB(GLint size, const GLfloat *weights)
{
    unsupported(__FUNCTION__);
}

void glWeightivARB(GLint size, const GLint *weights)
{
    unsupported(__FUNCTION__);
}

void glWeightsvARB(GLint size, const GLshort *weights)
{
    unsupported(__FUNCTION__);
}

void glWeightubvARB(GLint size, const GLubyte *weights)
{
    unsupported(__FUNCTION__);
}

void glWeightuivARB(GLint size, const GLuint *weights)
{
    unsupported(__FUNCTION__);
}

void glWeightusvARB(GLint size, const GLushort *weights)
{
    unsupported(__FUNCTION__);
}

#endif
