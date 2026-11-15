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
 * cgl.c
 * MGL
 *
 * Apple's CGL context calls, making MooGL contexts. A program that links
 * libmoogl rather than OpenGL.framework gets these. The contexts draw
 * offscreen, as CGL's own do until a window is attached; attaching one
 * (CGLSetSurface, through NSOpenGLContext) is Phase 10.
 */

#ifdef MGL_GL_CORE

// these are Apple's CGL names, so its deprecation notes are about this file
#define GL_SILENCE_DEPRECATION

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <OpenGL/OpenGL.h>

typedef struct GLMContextRec_t *GLMContext;

GLMContext createGLMContextProfile(GLenum format, GLenum type, GLenum depth_format, GLenum depth_type,
                                   GLenum stencil_format, GLenum stencil_type, GLMContext share,
                                   GLbitfield profile);
void destroyGLMContext(GLMContext ctx);
GLMContext MGLgetCurrentContext(void);
void MGLsetCurrentContext(GLMContext ctx);
void MGLswapBuffers(GLMContext ctx);
void MGLsetSwapInterval(GLMContext ctx, int interval);
void *CppCreateMGLRendererHeadless(void *glm_ctx);

#define MGL_BGRA                        0x80E1
#define MGL_UNSIGNED_INT_8_8_8_8_REV    0x8367
#define MGL_DEPTH_COMPONENT             0x1902
#define MGL_FLOAT                       0x1406
#define MGL_STENCIL_INDEX8              0x8D48
#define MGL_UNSIGNED_BYTE               0x1401
#define MGL_CORE_PROFILE                0x1
#define MGL_COMPATIBILITY_PROFILE       0x2

struct _CGLPixelFormatObject {
    GLuint refs;
    GLint depth, stencil, profile;
    GLboolean double_buffer;
};

struct _CGLContextObject {
    GLuint refs;
    GLMContext glm;
    CGLPixelFormatObj pix;
    CGLShareGroupObj group;
    pthread_mutex_t lock;
    GLint swap_interval, surface_opacity, surface_order;
    GLuint enabled;             // a bit per CGLContextEnable that is on
};

static __thread CGLContextObj current;

// the attributes that are followed by a value
static int takesValue(CGLPixelFormatAttribute a)
{
    switch (a)
    {
        case kCGLPFAAuxBuffers:
        case kCGLPFAColorSize:
        case kCGLPFAAlphaSize:
        case kCGLPFADepthSize:
        case kCGLPFAStencilSize:
        case kCGLPFAAccumSize:
        case kCGLPFARendererID:
        case kCGLPFADisplayMask:
        case kCGLPFASampleBuffers:
        case kCGLPFASamples:
        case kCGLPFAOpenGLProfile:
        case kCGLPFAVirtualScreenCount:
            return 1;
        default:
            return 0;
    }
}

CGLError CGLChoosePixelFormat(const CGLPixelFormatAttribute *attribs, CGLPixelFormatObj *pix, GLint *npix)
{
    struct _CGLPixelFormatObject *p;

    if (pix == NULL)
        return kCGLBadAddress;

    *pix = NULL;
    if (npix)
        *npix = 0;

    if ((p = calloc(1, sizeof *p)) == NULL)
        return kCGLBadAlloc;

    p->refs = 1;
    p->profile = kCGLOGLPVersion_Legacy;

    for (const CGLPixelFormatAttribute *a = attribs; a && *a; a++)
    {
        GLint value = takesValue(*a) ? (GLint)a[1] : 1;

        switch (*a)
        {
            case kCGLPFADepthSize: p->depth = value; break;
            case kCGLPFAStencilSize: p->stencil = value; break;
            case kCGLPFADoubleBuffer: p->double_buffer = 1; break;
            case kCGLPFAOpenGLProfile:
                if (value != kCGLOGLPVersion_Legacy && value != kCGLOGLPVersion_GL3_Core &&
                    value != kCGLOGLPVersion_GL4_Core)
                {
                    free(p);
                    return kCGLBadAttribute;
                }
                p->profile = value;
                break;
            // MooGL has none of these, and a program that needs one would get
            // a context that quietly lacks it
            case kCGLPFAStereo:
            case kCGLPFAOffScreen:
            case kCGLPFAFullScreen:
            case kCGLPFAPBuffer:
            case kCGLPFARemotePBuffer:
                free(p);
                return kCGLNoError;
            default:
                break;
        }

        if (takesValue(*a))
            a++;
    }

    *pix = p;
    if (npix)
        *npix = 1;
    return kCGLNoError;
}

CGLPixelFormatObj CGLRetainPixelFormat(CGLPixelFormatObj pix)
{
    if (pix)
        pix->refs++;
    return pix;
}

void CGLReleasePixelFormat(CGLPixelFormatObj pix)
{
    if (pix && --pix->refs == 0)
        free(pix);
}

CGLError CGLDestroyPixelFormat(CGLPixelFormatObj pix)
{
    if (pix == NULL)
        return kCGLBadPixelFormat;

    CGLReleasePixelFormat(pix);
    return kCGLNoError;
}

GLuint CGLGetPixelFormatRetainCount(CGLPixelFormatObj pix)
{
    return pix ? pix->refs : 0;
}

// what MooGL gives, which is not always what was asked for
CGLError CGLDescribePixelFormat(CGLPixelFormatObj pix, GLint pix_num, CGLPixelFormatAttribute attrib, GLint *value)
{
    if (pix == NULL)
        return kCGLBadPixelFormat;
    if (pix_num != 0 || value == NULL)
        return kCGLBadValue;

    switch (attrib)
    {
        case kCGLPFAColorSize: *value = 24; break;
        case kCGLPFAAlphaSize: *value = 8; break;
        case kCGLPFADepthSize: *value = pix->depth > 0 ? 32 : 0; break;
        case kCGLPFAStencilSize: *value = pix->stencil > 0 ? 8 : 0; break;
        case kCGLPFADoubleBuffer: *value = pix->double_buffer; break;
        case kCGLPFAOpenGLProfile: *value = pix->profile; break;
        case kCGLPFAAccelerated: *value = 1; break;
        case kCGLPFAVirtualScreenCount: *value = 1; break;
        default: *value = 0; break;
    }
    return kCGLNoError;
}

CGLError CGLCreateContext(CGLPixelFormatObj pix, CGLContextObj share, CGLContextObj *ctx)
{
    struct _CGLContextObject *c;
    pthread_mutexattr_t attr;
    GLMContext was = MGLgetCurrentContext();

    if (ctx == NULL)
        return kCGLBadAddress;
    *ctx = NULL;

    if (pix == NULL)
        return kCGLBadPixelFormat;

    // GL shares only between contexts of one kind
    if (share && (share->pix->profile == kCGLOGLPVersion_Legacy) != (pix->profile == kCGLOGLPVersion_Legacy))
        return kCGLBadMatch;

    if ((c = calloc(1, sizeof *c)) == NULL)
        return kCGLBadAlloc;

    c->glm = createGLMContextProfile(MGL_BGRA, MGL_UNSIGNED_INT_8_8_8_8_REV,
                                     pix->depth > 0 ? MGL_DEPTH_COMPONENT : 0, pix->depth > 0 ? MGL_FLOAT : 0,
                                     pix->stencil > 0 ? MGL_STENCIL_INDEX8 : 0,
                                     pix->stencil > 0 ? MGL_UNSIGNED_BYTE : 0,
                                     share ? share->glm : NULL,
                                     pix->profile == kCGLOGLPVersion_Legacy ? MGL_COMPATIBILITY_PROFILE
                                                                            : MGL_CORE_PROFILE);
    if (c->glm == NULL)
    {
        free(c);
        return kCGLBadAlloc;
    }

    MGLsetCurrentContext(c->glm);
    CppCreateMGLRendererHeadless(c->glm);
    MGLsetCurrentContext(was);

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&c->lock, &attr);
    pthread_mutexattr_destroy(&attr);

    c->refs = 1;
    c->pix = CGLRetainPixelFormat(pix);
    c->group = share ? share->group : (CGLShareGroupObj)c;
    c->swap_interval = 1;
    c->surface_opacity = 1;
    c->surface_order = 1;
    MGLsetSwapInterval(c->glm, 1);

    *ctx = c;
    return kCGLNoError;
}

static void freeContext(CGLContextObj ctx)
{
    if (current == ctx)
        CGLSetCurrentContext(NULL);

    destroyGLMContext(ctx->glm);
    CGLReleasePixelFormat(ctx->pix);
    pthread_mutex_destroy(&ctx->lock);
    free(ctx);
}

CGLError CGLDestroyContext(CGLContextObj ctx)
{
    if (ctx == NULL)
        return kCGLBadContext;

    freeContext(ctx);
    return kCGLNoError;
}

CGLContextObj CGLRetainContext(CGLContextObj ctx)
{
    if (ctx)
        ctx->refs++;
    return ctx;
}

void CGLReleaseContext(CGLContextObj ctx)
{
    if (ctx && --ctx->refs == 0)
        freeContext(ctx);
}

GLuint CGLGetContextRetainCount(CGLContextObj ctx)
{
    return ctx ? ctx->refs : 0;
}

CGLPixelFormatObj CGLGetPixelFormat(CGLContextObj ctx)
{
    return ctx ? ctx->pix : NULL;
}

CGLShareGroupObj CGLGetShareGroup(CGLContextObj ctx)
{
    return ctx ? ctx->group : NULL;
}

CGLError CGLSetCurrentContext(CGLContextObj ctx)
{
    current = ctx;
    MGLsetCurrentContext(ctx ? ctx->glm : NULL);
    return kCGLNoError;
}

CGLContextObj CGLGetCurrentContext(void)
{
    return current;
}

CGLError CGLFlushDrawable(CGLContextObj ctx)
{
    if (ctx == NULL)
        return kCGLBadContext;

    MGLswapBuffers(ctx->glm);
    return kCGLNoError;
}

CGLError CGLClearDrawable(CGLContextObj ctx)
{
    return ctx ? kCGLNoError : kCGLBadContext;
}

CGLError CGLUpdateContext(CGLContextObj ctx)
{
    return ctx ? kCGLNoError : kCGLBadContext;
}

CGLError CGLSetParameter(CGLContextObj ctx, CGLContextParameter pname, const GLint *params)
{
    if (ctx == NULL)
        return kCGLBadContext;
    if (params == NULL)
        return kCGLBadAddress;

    switch (pname)
    {
        case kCGLCPSwapInterval:
            ctx->swap_interval = params[0];
            MGLsetSwapInterval(ctx->glm, params[0]);
            return kCGLNoError;
        case kCGLCPSurfaceOpacity:
            ctx->surface_opacity = params[0];
            return kCGLNoError;
        case kCGLCPSurfaceOrder:
            ctx->surface_order = params[0];
            return kCGLNoError;
        default:
            return kCGLBadEnumeration;
    }
}

CGLError CGLGetParameter(CGLContextObj ctx, CGLContextParameter pname, GLint *params)
{
    if (ctx == NULL)
        return kCGLBadContext;
    if (params == NULL)
        return kCGLBadAddress;

    switch (pname)
    {
        case kCGLCPSwapInterval: params[0] = ctx->swap_interval; return kCGLNoError;
        case kCGLCPSurfaceOpacity: params[0] = ctx->surface_opacity; return kCGLNoError;
        case kCGLCPSurfaceOrder: params[0] = ctx->surface_order; return kCGLNoError;
        case kCGLCPHasDrawable: params[0] = 0; return kCGLNoError;
        default: return kCGLBadEnumeration;
    }
}

static int enableBit(CGLContextEnable pname)
{
    switch (pname)
    {
        case kCGLCESwapRectangle: return 0;
        case kCGLCESwapLimit: return 1;
        case kCGLCERasterization: return 2;
        case kCGLCEStateValidation: return 3;
        case kCGLCESurfaceBackingSize: return 4;
        case kCGLCEDisplayListOptimization: return 5;
        case kCGLCEMPEngine: return 6;
        case kCGLCECrashOnRemovedFunctions: return 7;
        default: return -1;
    }
}

CGLError CGLEnable(CGLContextObj ctx, CGLContextEnable pname)
{
    int bit = enableBit(pname);

    if (ctx == NULL)
        return kCGLBadContext;
    if (bit < 0)
        return kCGLBadEnumeration;

    ctx->enabled |= 1u << bit;
    return kCGLNoError;
}

CGLError CGLDisable(CGLContextObj ctx, CGLContextEnable pname)
{
    int bit = enableBit(pname);

    if (ctx == NULL)
        return kCGLBadContext;
    if (bit < 0)
        return kCGLBadEnumeration;

    ctx->enabled &= ~(1u << bit);
    return kCGLNoError;
}

CGLError CGLIsEnabled(CGLContextObj ctx, CGLContextEnable pname, GLint *enable)
{
    int bit = enableBit(pname);

    if (ctx == NULL)
        return kCGLBadContext;
    if (bit < 0)
        return kCGLBadEnumeration;
    if (enable == NULL)
        return kCGLBadAddress;

    *enable = (ctx->enabled >> bit) & 1;
    return kCGLNoError;
}

CGLError CGLSetVirtualScreen(CGLContextObj ctx, GLint screen)
{
    if (ctx == NULL)
        return kCGLBadContext;
    return screen == 0 ? kCGLNoError : kCGLBadValue;
}

CGLError CGLGetVirtualScreen(CGLContextObj ctx, GLint *screen)
{
    if (ctx == NULL)
        return kCGLBadContext;
    if (screen == NULL)
        return kCGLBadAddress;

    *screen = 0;
    return kCGLNoError;
}

CGLError CGLLockContext(CGLContextObj ctx)
{
    if (ctx == NULL)
        return kCGLBadContext;

    pthread_mutex_lock(&ctx->lock);
    return kCGLNoError;
}

CGLError CGLUnlockContext(CGLContextObj ctx)
{
    if (ctx == NULL)
        return kCGLBadContext;

    pthread_mutex_unlock(&ctx->lock);
    return kCGLNoError;
}

void CGLGetVersion(GLint *majorvers, GLint *minorvers)
{
    // what OpenGL.framework reported last
    if (majorvers)
        *majorvers = 1;
    if (minorvers)
        *minorvers = 2;
}

const char *CGLErrorString(CGLError error)
{
    switch (error)
    {
        case kCGLNoError: return "no error";
        case kCGLBadAttribute: return "invalid pixel format attribute";
        case kCGLBadProperty: return "invalid renderer property";
        case kCGLBadPixelFormat: return "invalid pixel format";
        case kCGLBadRendererInfo: return "invalid renderer info";
        case kCGLBadContext: return "invalid context";
        case kCGLBadDrawable: return "invalid drawable";
        case kCGLBadDisplay: return "invalid graphics device";
        case kCGLBadState: return "invalid context state";
        case kCGLBadValue: return "invalid numerical value";
        case kCGLBadMatch: return "invalid share context";
        case kCGLBadEnumeration: return "invalid enumerant";
        case kCGLBadOffScreen: return "invalid offscreen drawable";
        case kCGLBadFullScreen: return "invalid fullscreen drawable";
        case kCGLBadWindow: return "invalid window";
        case kCGLBadAddress: return "invalid pointer";
        case kCGLBadCodeModule: return "invalid code module";
        case kCGLBadAlloc: return "invalid memory allocation";
        case kCGLBadConnection: return "invalid CoreGraphics connection";
        default: return "unknown error";
    }
}

#endif /* MGL_GL_CORE */
