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
 * apple.c
 * MGL
 *
 * The GLU calls only Apple's libGLU has. The CTX ones take the context to
 * draw with first; the CFM ones took Classic Mac OS function pointers, which
 * on arm64 are plain ones.
 */

#include <GL/glu.h>

typedef struct _CGLContextObject *CGLContextObj;
CGLContextObj CGLGetCurrentContext(void);
int CGLSetCurrentContext(CGLContextObj ctx);

#define WITH(ctx, call) do { CGLContextObj was_ = CGLGetCurrentContext(); CGLSetCurrentContext(ctx); call; CGLSetCurrentContext(was_); } while (0)
#define WITH_RESULT(type, ctx, call) do { type r_; WITH(ctx, r_ = call); return r_; } while (0)

GLAPI GLint gluBuild1DMipmapsCTX(CGLContextObj ctx, GLenum target, GLint internalFormat, GLsizei width,
                                 GLenum format, GLenum type, const void *data)
{
    WITH_RESULT(GLint, ctx, gluBuild1DMipmaps(target, internalFormat, width, format, type, data));
}

GLAPI GLint gluBuild2DMipmapsCTX(CGLContextObj ctx, GLenum target, GLint internalFormat, GLsizei width,
                                 GLsizei height, GLenum format, GLenum type, const void *data)
{
    WITH_RESULT(GLint, ctx, gluBuild2DMipmaps(target, internalFormat, width, height, format, type, data));
}

GLAPI GLint gluBuild3DMipmapsCTX(CGLContextObj ctx, GLenum target, GLint internalFormat, GLsizei width,
                                 GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *data)
{
    WITH_RESULT(GLint, ctx, gluBuild3DMipmaps(target, internalFormat, width, height, depth, format, type, data));
}

GLAPI GLint gluBuild1DMipmapLevelsCTX(CGLContextObj ctx, GLenum target, GLint internalFormat, GLsizei width,
                                      GLenum format, GLenum type, GLint level, GLint base, GLint max,
                                      const void *data)
{
    WITH_RESULT(GLint, ctx, gluBuild1DMipmapLevels(target, internalFormat, width, format, type, level, base, max, data));
}

GLAPI GLint gluBuild2DMipmapLevelsCTX(CGLContextObj ctx, GLenum target, GLint internalFormat, GLsizei width,
                                      GLsizei height, GLenum format, GLenum type, GLint level, GLint base,
                                      GLint max, const void *data)
{
    WITH_RESULT(GLint, ctx, gluBuild2DMipmapLevels(target, internalFormat, width, height, format, type, level, base,
                                                   max, data));
}

GLAPI GLint gluBuild3DMipmapLevelsCTX(CGLContextObj ctx, GLenum target, GLint internalFormat, GLsizei width,
                                      GLsizei height, GLsizei depth, GLenum format, GLenum type, GLint level,
                                      GLint base, GLint max, const void *data)
{
    WITH_RESULT(GLint, ctx, gluBuild3DMipmapLevels(target, internalFormat, width, height, depth, format, type, level,
                                                   base, max, data));
}

GLAPI GLint gluScaleImageCTX(CGLContextObj ctx, GLenum format, GLsizei wIn, GLsizei hIn, GLenum typeIn,
                             const void *dataIn, GLsizei wOut, GLsizei hOut, GLenum typeOut, GLvoid *dataOut)
{
    WITH_RESULT(GLint, ctx, gluScaleImage(format, wIn, hIn, typeIn, dataIn, wOut, hOut, typeOut, dataOut));
}

GLAPI void gluLookAtCTX(CGLContextObj ctx, GLdouble eyeX, GLdouble eyeY, GLdouble eyeZ, GLdouble centerX,
                        GLdouble centerY, GLdouble centerZ, GLdouble upX, GLdouble upY, GLdouble upZ)
{
    WITH(ctx, gluLookAt(eyeX, eyeY, eyeZ, centerX, centerY, centerZ, upX, upY, upZ));
}

GLAPI void gluOrtho2DCTX(CGLContextObj ctx, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top)
{
    WITH(ctx, gluOrtho2D(left, right, bottom, top));
}

GLAPI void gluPerspectiveCTX(CGLContextObj ctx, GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
    WITH(ctx, gluPerspective(fovy, aspect, zNear, zFar));
}

GLAPI void gluPickMatrixCTX(CGLContextObj ctx, GLdouble x, GLdouble y, GLdouble delX, GLdouble delY, GLint *viewport)
{
    WITH(ctx, gluPickMatrix(x, y, delX, delY, viewport));
}

GLAPI GLUnurbs *gluNewNurbsRendererCTX(CGLContextObj ctx)
{
    WITH_RESULT(GLUnurbs *, ctx, gluNewNurbsRenderer());
}

GLAPI GLUquadric *gluNewQuadricCTX(CGLContextObj ctx)
{
    WITH_RESULT(GLUquadric *, ctx, gluNewQuadric());
}

GLAPI GLUtesselator *gluNewTessCTX(CGLContextObj ctx)
{
    WITH_RESULT(GLUtesselator *, ctx, gluNewTess());
}

GLAPI void gluCylinderCTX(CGLContextObj ctx, GLUquadric *quad, GLdouble base, GLdouble top, GLdouble height,
                          GLint slices, GLint stacks)
{
    WITH(ctx, gluCylinder(quad, base, top, height, slices, stacks));
}

GLAPI void gluDiskCTX(CGLContextObj ctx, GLUquadric *quad, GLdouble inner, GLdouble outer, GLint slices, GLint loops)
{
    WITH(ctx, gluDisk(quad, inner, outer, slices, loops));
}

GLAPI void gluPartialDiskCTX(CGLContextObj ctx, GLUquadric *quad, GLdouble inner, GLdouble outer, GLint slices,
                             GLint loops, GLdouble start, GLdouble sweep)
{
    WITH(ctx, gluPartialDisk(quad, inner, outer, slices, loops, start, sweep));
}

GLAPI void gluSphereCTX(CGLContextObj ctx, GLUquadric *quad, GLdouble radius, GLint slices, GLint stacks)
{
    WITH(ctx, gluSphere(quad, radius, slices, stacks));
}

GLAPI void gluNurbsCallbackCFM(GLUnurbs *nurb, GLenum which, _GLUfuncptr fn)
{
    gluNurbsCallback(nurb, which, fn);
}

GLAPI void gluQuadricCallbackCFM(GLUquadric *quad, GLenum which, _GLUfuncptr fn)
{
    gluQuadricCallback(quad, which, fn);
}

GLAPI void gluTessCallbackCFM(GLUtesselator *tess, GLenum which, _GLUfuncptr fn)
{
    gluTessCallback(tess, which, fn);
}
