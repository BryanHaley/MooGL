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

#include "glcorearb.h"

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

#endif
