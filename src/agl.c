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
 * agl.c
 * MGL
 *
 * AGL over MooGL's CGL (cgl.c), as Apple's was over its own.
 */

#ifdef MGL_GL_CORE

// these are Apple's CGL names, so its deprecation notes are about this file
#define GL_SILENCE_DEPRECATION

#include <stdlib.h>

#include <OpenGL/OpenGL.h>

#include "AGL/agl.h"

#ifndef GL_TRUE
#define GL_TRUE 1
#define GL_FALSE 0
#endif

struct __AGLPixelFormatRec {
    CGLPixelFormatObj cgl;
};

struct __AGLContextRec {
    CGLContextObj cgl;
};

static __thread GLenum agl_error;
static __thread AGLContext agl_current;

static GLboolean fail(GLenum error)
{
    agl_error = error;
    return GL_FALSE;
}

static GLenum fromCGL(CGLError e)
{
    // CGL's errors kept AGL's numbers
    return e == kCGLNoError ? AGL_NO_ERROR : (GLenum)e;
}

// the attributes that are followed by a value
static int takesValue(GLint a)
{
    switch (a)
    {
        case AGL_BUFFER_SIZE: case AGL_LEVEL: case AGL_AUX_BUFFERS:
        case AGL_RED_SIZE: case AGL_GREEN_SIZE: case AGL_BLUE_SIZE: case AGL_ALPHA_SIZE:
        case AGL_DEPTH_SIZE: case AGL_STENCIL_SIZE:
        case AGL_ACCUM_RED_SIZE: case AGL_ACCUM_GREEN_SIZE: case AGL_ACCUM_BLUE_SIZE: case AGL_ACCUM_ALPHA_SIZE:
        case AGL_PIXEL_SIZE: case AGL_SAMPLE_BUFFERS_ARB: case AGL_SAMPLES_ARB:
        case AGL_RENDERER_ID: case AGL_VIRTUAL_SCREEN: case AGL_DISPLAY_MASK:
            return 1;
        default:
            return 0;
    }
}

AGLPixelFormat aglCreatePixelFormat(const GLint *attribs)
{
    CGLPixelFormatAttribute cgl[16];
    int n = 0;
    CGLPixelFormatObj pix = NULL;
    GLint npix = 0;
    struct __AGLPixelFormatRec *p;

    for (const GLint *a = attribs; a && *a != AGL_NONE; a++)
    {
        GLint value = takesValue(*a) ? a[1] : 1;

        switch (*a)
        {
            case AGL_DOUBLEBUFFER: cgl[n++] = kCGLPFADoubleBuffer; break;
            case AGL_ACCELERATED: cgl[n++] = kCGLPFAAccelerated; break;
            case AGL_DEPTH_SIZE:
                cgl[n++] = kCGLPFADepthSize;
                cgl[n++] = (CGLPixelFormatAttribute)value;
                break;
            case AGL_STENCIL_SIZE:
                cgl[n++] = kCGLPFAStencilSize;
                cgl[n++] = (CGLPixelFormatAttribute)value;
                break;
            // a level other than the main plane, stereo, and the drawables
            // AGL could only reach through Carbon have no match here
            case AGL_LEVEL:
                if (value != 0)
                    return NULL;
                break;
            case AGL_STEREO:
            case AGL_OFFSCREEN:
            case AGL_FULLSCREEN:
            case AGL_PBUFFER:
            case AGL_REMOTE_PBUFFER:
                return NULL;
            default:
                break;
        }

        if (takesValue(*a))
            a++;

        if (n > 12)
            break;
    }

    cgl[n] = (CGLPixelFormatAttribute)0;

    CGLError e = CGLChoosePixelFormat(cgl, &pix, &npix);

    if (e != kCGLNoError || pix == NULL)
    {
        fail(e != kCGLNoError ? fromCGL(e) : AGL_NO_ERROR);
        return NULL;
    }

    if ((p = calloc(1, sizeof *p)) == NULL)
    {
        CGLReleasePixelFormat(pix);
        fail(AGL_BAD_ALLOC);
        return NULL;
    }

    p->cgl = pix;
    return p;
}

AGLPixelFormat aglChoosePixelFormat(const void *gdevs, GLint ndev, const GLint *attribs)
{
    (void)gdevs;
    (void)ndev;
    return aglCreatePixelFormat(attribs);
}

void aglDestroyPixelFormat(AGLPixelFormat pix)
{
    if (pix == NULL)
    {
        fail(AGL_BAD_PIXELFMT);
        return;
    }

    CGLReleasePixelFormat(pix->cgl);
    free(pix);
}

// MooGL has one renderer, so one format
AGLPixelFormat aglNextPixelFormat(AGLPixelFormat pix)
{
    (void)pix;
    return NULL;
}

GLboolean aglDescribePixelFormat(AGLPixelFormat pix, GLint attrib, GLint *value)
{
    if (pix == NULL)
        return fail(AGL_BAD_PIXELFMT);
    if (value == NULL)
        return fail(AGL_BAD_POINTER);

    switch (attrib)
    {
        case AGL_RGBA: *value = 1; return GL_TRUE;
        case AGL_RED_SIZE: case AGL_GREEN_SIZE: case AGL_BLUE_SIZE: case AGL_ALPHA_SIZE: *value = 8; return GL_TRUE;
        case AGL_PIXEL_SIZE: case AGL_BUFFER_SIZE: *value = 32; return GL_TRUE;
        case AGL_DOUBLEBUFFER:
            CGLDescribePixelFormat(pix->cgl, 0, kCGLPFADoubleBuffer, value);
            return GL_TRUE;
        case AGL_DEPTH_SIZE:
            CGLDescribePixelFormat(pix->cgl, 0, kCGLPFADepthSize, value);
            return GL_TRUE;
        case AGL_STENCIL_SIZE:
            CGLDescribePixelFormat(pix->cgl, 0, kCGLPFAStencilSize, value);
            return GL_TRUE;
        case AGL_ACCELERATED: *value = 1; return GL_TRUE;
        default: *value = 0; return GL_TRUE;
    }
}

AGLContext aglCreateContext(AGLPixelFormat pix, AGLContext share)
{
    struct __AGLContextRec *c;
    CGLContextObj cgl = NULL;

    if (pix == NULL)
    {
        fail(AGL_BAD_PIXELFMT);
        return NULL;
    }

    CGLError e = CGLCreateContext(pix->cgl, share ? share->cgl : NULL, &cgl);

    if (e != kCGLNoError)
    {
        fail(fromCGL(e));
        return NULL;
    }

    if ((c = calloc(1, sizeof *c)) == NULL)
    {
        CGLReleaseContext(cgl);
        fail(AGL_BAD_ALLOC);
        return NULL;
    }

    c->cgl = cgl;
    return c;
}

GLboolean aglDestroyContext(AGLContext ctx)
{
    if (ctx == NULL)
        return fail(AGL_BAD_CONTEXT);

    if (agl_current == ctx)
        aglSetCurrentContext(NULL);

    CGLReleaseContext(ctx->cgl);
    free(ctx);
    return GL_TRUE;
}

GLboolean aglCopyContext(AGLContext src, AGLContext dst, GLuint mask)
{
    (void)mask;
    if (src == NULL || dst == NULL)
        return fail(AGL_BAD_CONTEXT);

    // glPushAttrib's groups copied between contexts: nothing calls it now
    return fail(AGL_BAD_STATE);
}

GLboolean aglUpdateContext(AGLContext ctx)
{
    return ctx ? GL_TRUE : fail(AGL_BAD_CONTEXT);
}

GLboolean aglSetCurrentContext(AGLContext ctx)
{
    agl_current = ctx;
    CGLSetCurrentContext(ctx ? ctx->cgl : NULL);
    return GL_TRUE;
}

AGLContext aglGetCurrentContext(void)
{
    return agl_current;
}

GLboolean aglSetDrawable(AGLContext ctx, AGLDrawable draw)
{
    if (ctx == NULL)
        return fail(AGL_BAD_CONTEXT);

    // a QuickDraw port: Carbon is gone, so there is none to draw into
    return draw == NULL ? GL_TRUE : fail(AGL_BAD_DRAWABLE);
}

AGLDrawable aglGetDrawable(AGLContext ctx)
{
    if (ctx == NULL)
        fail(AGL_BAD_CONTEXT);
    return NULL;
}

GLboolean aglSetVirtualScreen(AGLContext ctx, GLint screen)
{
    if (ctx == NULL)
        return fail(AGL_BAD_CONTEXT);
    return screen == 0 ? GL_TRUE : fail(AGL_BAD_VALUE);
}

GLint aglGetVirtualScreen(AGLContext ctx)
{
    if (ctx == NULL)
        fail(AGL_BAD_CONTEXT);
    return 0;
}

void aglGetVersion(GLint *major, GLint *minor)
{
    if (major)
        *major = 3;
    if (minor)
        *minor = 0;
}

GLboolean aglConfigure(GLenum pname, GLuint param)
{
    (void)param;
    switch (pname)
    {
        case AGL_FORMAT_CACHE_SIZE:
        case AGL_CLEAR_FORMAT_CACHE:
        case AGL_RETAIN_RENDERERS:
            return GL_TRUE;
        default:
            return fail(AGL_BAD_ENUM);
    }
}

void aglSwapBuffers(AGLContext ctx)
{
    if (ctx == NULL)
    {
        fail(AGL_BAD_CONTEXT);
        return;
    }
    CGLFlushDrawable(ctx->cgl);
}

// AGL's enables and parameters share CGL's numbers
GLboolean aglEnable(AGLContext ctx, GLenum pname)
{
    if (ctx == NULL)
        return fail(AGL_BAD_CONTEXT);

    CGLError e = CGLEnable(ctx->cgl, (CGLContextEnable)pname);
    return e == kCGLNoError ? GL_TRUE : fail(fromCGL(e));
}

GLboolean aglDisable(AGLContext ctx, GLenum pname)
{
    if (ctx == NULL)
        return fail(AGL_BAD_CONTEXT);

    CGLError e = CGLDisable(ctx->cgl, (CGLContextEnable)pname);
    return e == kCGLNoError ? GL_TRUE : fail(fromCGL(e));
}

GLboolean aglIsEnabled(AGLContext ctx, GLenum pname)
{
    GLint on = 0;

    if (ctx == NULL)
        return fail(AGL_BAD_CONTEXT);

    CGLError e = CGLIsEnabled(ctx->cgl, (CGLContextEnable)pname, &on);
    return e == kCGLNoError ? (GLboolean)(on != 0) : fail(fromCGL(e));
}

GLboolean aglSetInteger(AGLContext ctx, GLenum pname, const GLint *params)
{
    if (ctx == NULL)
        return fail(AGL_BAD_CONTEXT);

    CGLError e = CGLSetParameter(ctx->cgl, (CGLContextParameter)pname, params);
    return e == kCGLNoError ? GL_TRUE : fail(fromCGL(e));
}

GLboolean aglGetInteger(AGLContext ctx, GLenum pname, GLint *params)
{
    if (ctx == NULL)
        return fail(AGL_BAD_CONTEXT);

    CGLError e = CGLGetParameter(ctx->cgl, (CGLContextParameter)pname, params);
    return e == kCGLNoError ? GL_TRUE : fail(fromCGL(e));
}

GLenum aglGetError(void)
{
    GLenum e = agl_error;

    agl_error = AGL_NO_ERROR;
    return e;
}

const GLubyte *aglErrorString(GLenum code)
{
    if (code == AGL_INVALID_FUNCTION)
        return (const GLubyte *)"invalid function";
    return (const GLubyte *)CGLErrorString((CGLError)code);
}

void aglResetLibrary(void)
{
}

GLboolean aglGetCGLContext(AGLContext ctx, void **cgl_ctx)
{
    if (ctx == NULL)
        return fail(AGL_BAD_CONTEXT);
    if (cgl_ctx == NULL)
        return fail(AGL_BAD_POINTER);

    *cgl_ctx = ctx->cgl;
    return GL_TRUE;
}

GLboolean aglGetCGLPixelFormat(AGLPixelFormat pix, void **cgl_pix)
{
    if (pix == NULL)
        return fail(AGL_BAD_PIXELFMT);
    if (cgl_pix == NULL)
        return fail(AGL_BAD_POINTER);

    *cgl_pix = pix->cgl;
    return GL_TRUE;
}

#endif /* MGL_GL_CORE */
