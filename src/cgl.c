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
 * Apple's CGL calls, making MooGL contexts. A program that links libmoogl
 * rather than OpenGL.framework gets these, and so does NSOpenGLContext when
 * MooGL.framework stands in for OpenGL.framework. A context draws offscreen
 * until CGLSetSurface puts it in a window (cgl_window.m).
 */

#ifdef MGL_GL_CORE

// these are Apple's CGL names, so its deprecation notes are about this file
#define GL_SILENCE_DEPRECATION

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLIOSurface.h>

#include "mgl_log.h"

typedef struct GLMContextRec_t *GLMContext;

GLMContext createGLMContextProfile(GLenum format, GLenum type, GLenum depth_format, GLenum depth_type,
                                   GLenum stencil_format, GLenum stencil_type, GLMContext share,
                                   GLbitfield profile);
void destroyGLMContext(GLMContext ctx);
GLMContext MGLgetCurrentContext(void);
void MGLsetCurrentContext(GLMContext ctx);
void MGLswapBuffers(GLMContext ctx);
void MGLsetSwapInterval(GLMContext ctx, int interval);
void mglSetPresentOnFlush(GLMContext ctx, bool on);
void mglSetReportedVersion(GLMContext ctx, int major, int minor);
void *CppCreateMGLRendererHeadless(void *glm_ctx);
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels);
void glFinish(void);

void mglCGLWatchViews(void);
bool mglCGLAttachWindow(GLMContext glm, int window_number, void **view);
bool mglCGLTexImageIOSurface(GLMContext ctx, GLenum target, GLenum internal_format, GLsizei width,
                             GLsizei height, GLenum format, GLenum type, IOSurfaceRef surface, GLuint plane);
void mglCGLDeviceInfo(uint64_t *registry_id, uint64_t *memory);

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
    GLint swap_rect[4], backing_size[2];
    GLint connection, window, surface;  // what CGLSetSurface was given
    void *view;                         // the NSView drawn into, retained
    GLsizei off_width, off_height;      // CGLSetOffScreen's client memory
    GLint off_rowbytes;
    void *off_base;
    CGLPBufferObj pbuffer;
    GLenum pbuffer_face;
    GLint pbuffer_level;
};

struct _CGLPBufferObject {
    GLuint refs;
    GLsizei width, height;
    GLenum target, internal_format;
    GLint max_level;
};

// the one renderer MooGL has: Apple's GPU, as Apple's driver describes it
#define MGL_RENDERER_ID 0x1027f00

struct _CGLRendererInfoObject {
    GLint count;
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

    mglCGLWatchViews();

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
    mglSetPresentOnFlush(c->glm, !pix->double_buffer);
    if (pix->profile == kCGLOGLPVersion_Legacy)
        mglSetReportedVersion(c->glm, 2, 1);

    MGL_DEBUG("CGLCreateContext(%p, %p) = %p\n", (void *)pix, (void *)share, (void *)c);
    *ctx = c;
    return kCGLNoError;
}

static void freeContext(CGLContextObj ctx)
{
    if (current == ctx)
        CGLSetCurrentContext(NULL);

    if (ctx->view)
        mglCGLAttachWindow(ctx->glm, 0, &ctx->view);
    if (ctx->pbuffer)
        CGLReleasePBuffer(ctx->pbuffer);

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

    // offscreen, the frame goes to the application's memory, bottom row last
    if (ctx->off_base && ctx->glm == MGLgetCurrentContext())
    {
        for (GLsizei y = 0; y < ctx->off_height; y++)
            glReadPixels(0, ctx->off_height - 1 - y, ctx->off_width, 1, MGL_BGRA, MGL_UNSIGNED_INT_8_8_8_8_REV,
                         (char *)ctx->off_base + (size_t)y * ctx->off_rowbytes);
        return kCGLNoError;
    }

    MGLswapBuffers(ctx->glm);
    return kCGLNoError;
}

CGLError CGLClearDrawable(CGLContextObj ctx)
{
    if (ctx == NULL)
        return kCGLBadContext;

    if (ctx->view)
        mglCGLAttachWindow(ctx->glm, 0, &ctx->view);
    ctx->connection = ctx->window = ctx->surface = 0;
    ctx->off_base = NULL;
    if (ctx->pbuffer)
        CGLReleasePBuffer(ctx->pbuffer);
    ctx->pbuffer = NULL;
    return kCGLNoError;
}

// Apple's private call that gives a context a window. The connection and
// surface are WindowServer's; MooGL only needs the window.
CGLError CGLSetSurface(CGLContextObj ctx, GLint connection, GLint window, GLint surface)
{
    if (ctx == NULL)
        return kCGLBadContext;

    MGL_DEBUG("CGLSetSurface(%p, %d, %d, %d)\n", (void *)ctx, connection, window, surface);
    if (!mglCGLAttachWindow(ctx->glm, window, &ctx->view))
        return kCGLBadWindow;

    ctx->connection = connection;
    ctx->window = window;
    ctx->surface = surface;
    return kCGLNoError;
}

CGLError CGLGetSurface(CGLContextObj ctx, GLint *connection, GLint *window, GLint *surface)
{
    if (ctx == NULL)
        return kCGLBadContext;

    if (connection)
        *connection = ctx->connection;
    if (window)
        *window = ctx->window;
    if (surface)
        *surface = ctx->surface;
    return kCGLNoError;
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
        case kCGLCPSwapRectangle:
            memcpy(ctx->swap_rect, params, sizeof ctx->swap_rect);
            return kCGLNoError;
        case kCGLCPSurfaceBackingSize:
            memcpy(ctx->backing_size, params, sizeof ctx->backing_size);
            return kCGLNoError;
        // hints MooGL has nothing to do with
        case kCGLCPSurfaceSurfaceVolatile:
        case kCGLCPReclaimResources:
        case kCGLCPMPSwapsInFlight:
        case kCGLCPAbortOnGPURestartStatusDenied:
        case kCGLCPContextPriorityRequest:
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
        case kCGLCPHasDrawable: params[0] = ctx->view != NULL || ctx->off_base != NULL; return kCGLNoError;
        case kCGLCPSwapRectangle: memcpy(params, ctx->swap_rect, sizeof ctx->swap_rect); return kCGLNoError;
        case kCGLCPSurfaceBackingSize: memcpy(params, ctx->backing_size, sizeof ctx->backing_size); return kCGLNoError;
        case kCGLCPCurrentRendererID: params[0] = MGL_RENDERER_ID; return kCGLNoError;
        case kCGLCPGPUVertexProcessing:
        case kCGLCPGPUFragmentProcessing:
        case kCGLCPSupportSeparateAddressSpace: params[0] = 1; return kCGLNoError;
        case kCGLCPGPURestartStatus:
        case kCGLCPSupportGPURestart:
        case kCGLCPSurfaceSurfaceVolatile: params[0] = 0; return kCGLNoError;
        case kCGLCPMPSwapsInFlight: params[0] = 1; return kCGLNoError;
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

// --- renderer info -----------------------------------------------------------

CGLError CGLQueryRendererInfo(GLuint display_mask, CGLRendererInfoObj *rend, GLint *nrend)
{
    if (rend == NULL || nrend == NULL)
        return kCGLBadAddress;

    if ((*rend = calloc(1, sizeof **rend)) == NULL)
        return kCGLBadAlloc;

    (*rend)->count = 1;
    *nrend = 1;
    return kCGLNoError;
}

CGLError CGLDestroyRendererInfo(CGLRendererInfoObj rend)
{
    if (rend == NULL)
        return kCGLBadRendererInfo;

    free(rend);
    return kCGLNoError;
}

CGLError CGLDescribeRenderer(CGLRendererInfoObj rend, GLint rend_num, CGLRendererProperty prop, GLint *value)
{
    uint64_t registry, memory;

    if (rend == NULL)
        return kCGLBadRendererInfo;
    if (value == NULL)
        return kCGLBadAddress;
    if (rend_num != 0)
        return kCGLBadValue;

    mglCGLDeviceInfo(&registry, &memory);

    switch ((int)prop)
    {
        case kCGLRPRendererID: *value = MGL_RENDERER_ID; break;
        case kCGLRPRendererCount: *value = 1; break;
        case kCGLRPDisplayMask: *value = 0x1f; break;
        case kCGLRPVideoMemoryMegabytes:
        case kCGLRPTextureMemoryMegabytes: *value = (GLint)(memory >> 20); break;
        case kCGLRPVideoMemory:
        case kCGLRPTextureMemory: *value = memory > 0x7fffffff ? 0x7fffffff : (GLint)memory; break;
        case kCGLRPMajorGLVersion: *value = 4; break;
        case kCGLRPBufferModes: *value = 13; break;
        case kCGLRPColorModes: *value = 0xa848000; break;
        case kCGLRPAccumModes: *value = 0x8000000; break;
        case kCGLRPDepthModes: *value = 0x1001; break;
        case kCGLRPStencilModes: *value = 0x81; break;
        case kCGLRPMaxAuxBuffers: *value = 2; break;
        case kCGLRPMaxSampleBuffers: *value = 1; break;
        case kCGLRPMaxSamples: *value = 4; break;
        case kCGLRPSampleModes: *value = 3; break;
        case kCGLRPRegistryIDLow: *value = (GLint)(uint32_t)registry; break;
        case kCGLRPRegistryIDHigh: *value = (GLint)(uint32_t)(registry >> 32); break;
        case kCGLRPOffScreen:
        case kCGLRPRemovable: *value = 0; break;
        case kCGLRPFullScreen:
        case kCGLRPAccelerated:
        case kCGLRPOnline:
        case kCGLRPWindow:
        case kCGLRPCompliant:
        case kCGLRPBackingStore:
        case kCGLRPAcceleratedCompute:
        case kCGLRPSampleAlpha:
        case kCGLRPGPUVertProcCapable:
        case kCGLRPGPUFragProcCapable:
        case kCGLRPRobust:
        case kCGLRPMPSafe:
        case kCGLRPMultiScreen: *value = 1; break;
        default: return kCGLBadProperty;
    }
    return kCGLNoError;
}

// Apple's driver gives an OpenCL device here; MooGL has no OpenCL
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
cl_device_id CGLGetDeviceFromGLRenderer(GLint rendererID)
{
    return NULL;
}
#pragma clang diagnostic pop

// --- contexts ----------------------------------------------------------------

CGLError CGLAreContextsShared(CGLContextObj ctx1, CGLContextObj ctx2, GLboolean *shared)
{
    if (ctx1 == NULL || ctx2 == NULL)
        return kCGLBadContext;
    if (shared == NULL)
        return kCGLBadAddress;

    *shared = ctx1->group == ctx2->group;
    return kCGLNoError;
}

// GL 1.x's glCopyContext of attribute groups, which no modern driver honours
CGLError CGLCopyContext(CGLContextObj src, CGLContextObj dst, GLbitfield mask)
{
    return src && dst ? kCGLNoError : kCGLBadContext;
}

// the client memory CGLFlushDrawable copies the frame into
CGLError CGLSetOffScreen(CGLContextObj ctx, GLsizei width, GLsizei height, GLint rowbytes, void *baseaddr)
{
    if (ctx == NULL)
        return kCGLBadContext;
    if (width < 0 || height < 0 || (baseaddr && rowbytes < width * 4))
        return kCGLBadValue;

    ctx->off_width = width;
    ctx->off_height = height;
    ctx->off_rowbytes = rowbytes;
    ctx->off_base = baseaddr;
    return kCGLNoError;
}

CGLError CGLGetOffScreen(CGLContextObj ctx, GLsizei *width, GLsizei *height, GLint *rowbytes, void **baseaddr)
{
    if (ctx == NULL)
        return kCGLBadContext;

    if (width)
        *width = ctx->off_width;
    if (height)
        *height = ctx->off_height;
    if (rowbytes)
        *rowbytes = ctx->off_rowbytes;
    if (baseaddr)
        *baseaddr = ctx->off_base;
    return kCGLNoError;
}

// gone from Apple's driver since 10.7 too: a full-screen window is the way
CGLError CGLSetFullScreen(CGLContextObj ctx)
{
    return ctx ? kCGLBadFullScreen : kCGLBadContext;
}

CGLError CGLSetFullScreenOnDisplay(CGLContextObj ctx, GLuint display_mask)
{
    return ctx ? kCGLBadFullScreen : kCGLBadContext;
}

CGLError CGLTexImageIOSurface2D(CGLContextObj ctx, GLenum target, GLenum internal_format, GLsizei width,
                                GLsizei height, GLenum format, GLenum type, IOSurfaceRef ioSurface, GLuint plane)
{
    GLMContext was = MGLgetCurrentContext();
    bool ok;

    if (ctx == NULL)
        return kCGLBadContext;

    MGLsetCurrentContext(ctx->glm);
    ok = mglCGLTexImageIOSurface(ctx->glm, target, internal_format, width, height, format, type, ioSurface, plane);
    MGLsetCurrentContext(was);
    return ok ? kCGLNoError : kCGLBadValue;
}

// --- options -----------------------------------------------------------------

static GLint global_options[8];

static GLint *globalOption(CGLGlobalOption pname)
{
    int i = (int)pname - kCGLGOFormatCacheSize;

    return i >= 0 && i < 8 ? &global_options[i] : NULL;
}

CGLError CGLSetGlobalOption(CGLGlobalOption pname, const GLint *params)
{
    GLint *o = globalOption(pname);

    if (o == NULL)
        return kCGLBadEnumeration;
    if (params == NULL)
        return kCGLBadAddress;

    *o = params[0];
    return kCGLNoError;
}

CGLError CGLGetGlobalOption(CGLGlobalOption pname, GLint *params)
{
    GLint *o = globalOption(pname);

    if (o == NULL)
        return kCGLBadEnumeration;
    if (params == NULL)
        return kCGLBadAddress;

    params[0] = *o;
    return kCGLNoError;
}

CGLError CGLSetOption(CGLGlobalOption pname, GLint param)
{
    return CGLSetGlobalOption(pname, &param);
}

CGLError CGLGetOption(CGLGlobalOption pname, GLint *param)
{
    return CGLGetGlobalOption(pname, param);
}

// --- pbuffers ----------------------------------------------------------------
// Kept and described as CGL says. A context does not draw into one: render to
// a framebuffer object instead, which is what Apple has said since 10.7.

CGLError CGLCreatePBuffer(GLsizei width, GLsizei height, GLenum target, GLenum internalFormat, GLint max_level,
                          CGLPBufferObj *pbuffer)
{
    if (pbuffer == NULL)
        return kCGLBadAddress;
    *pbuffer = NULL;

    if (width <= 0 || height <= 0 || max_level < 0)
        return kCGLBadValue;

    if ((*pbuffer = calloc(1, sizeof **pbuffer)) == NULL)
        return kCGLBadAlloc;

    (*pbuffer)->refs = 1;
    (*pbuffer)->width = width;
    (*pbuffer)->height = height;
    (*pbuffer)->target = target;
    (*pbuffer)->internal_format = internalFormat;
    (*pbuffer)->max_level = max_level;
    return kCGLNoError;
}

CGLError CGLDescribePBuffer(CGLPBufferObj obj, GLsizei *width, GLsizei *height, GLenum *target,
                            GLenum *internalFormat, GLint *mipmap)
{
    if (obj == NULL)
        return kCGLBadAddress;

    if (width)
        *width = obj->width;
    if (height)
        *height = obj->height;
    if (target)
        *target = obj->target;
    if (internalFormat)
        *internalFormat = obj->internal_format;
    if (mipmap)
        *mipmap = obj->max_level;
    return kCGLNoError;
}

CGLPBufferObj CGLRetainPBuffer(CGLPBufferObj pbuffer)
{
    if (pbuffer)
        pbuffer->refs++;
    return pbuffer;
}

void CGLReleasePBuffer(CGLPBufferObj pbuffer)
{
    if (pbuffer && --pbuffer->refs == 0)
        free(pbuffer);
}

CGLError CGLDestroyPBuffer(CGLPBufferObj pbuffer)
{
    if (pbuffer == NULL)
        return kCGLBadAddress;

    CGLReleasePBuffer(pbuffer);
    return kCGLNoError;
}

GLuint CGLGetPBufferRetainCount(CGLPBufferObj pbuffer)
{
    return pbuffer ? pbuffer->refs : 0;
}

CGLError CGLSetPBuffer(CGLContextObj ctx, CGLPBufferObj pbuffer, GLenum face, GLint level, GLint screen)
{
    if (ctx == NULL)
        return kCGLBadContext;
    if (pbuffer == NULL)
        return kCGLBadAddress;

    CGLRetainPBuffer(pbuffer);
    if (ctx->pbuffer)
        CGLReleasePBuffer(ctx->pbuffer);
    ctx->pbuffer = pbuffer;
    ctx->pbuffer_face = face;
    ctx->pbuffer_level = level;
    return kCGLNoError;
}

CGLError CGLGetPBuffer(CGLContextObj ctx, CGLPBufferObj *pbuffer, GLenum *face, GLint *level, GLint *screen)
{
    if (ctx == NULL)
        return kCGLBadContext;

    if (pbuffer)
        *pbuffer = ctx->pbuffer;
    if (face)
        *face = ctx->pbuffer_face;
    if (level)
        *level = ctx->pbuffer_level;
    if (screen)
        *screen = 0;
    return kCGLNoError;
}

CGLError CGLTexImagePBuffer(CGLContextObj ctx, CGLPBufferObj pbuffer, GLenum source)
{
    if (ctx == NULL)
        return kCGLBadContext;
    return pbuffer ? kCGLNoError : kCGLBadAddress;
}

// --- Apple's private calls ---------------------------------------------------
// Exported because Apple's framework exports them and a program bound to one
// would not launch without it. Nothing documents what they take.

CGLError CGLSetPBufferVolatileState(void) { return kCGLNoError; }
CGLError CGLRegisterDestroyPBufferCallback(void) { return kCGLNoError; }
CGLError CGLUnregisterDestroyPBufferCallback(void) { return kCGLNoError; }
CGLContextObj CGLGetNextContext(void) { return NULL; }
CGLError CGLBackDispatch(void) { return kCGLNoError; }
CGLError CGLFrontDispatch(void) { return kCGLNoError; }
CGLError CGLRestoreDispatch(void) { return kCGLNoError; }
CGLError CGLRestoreDispatchFunction(void) { return kCGLNoError; }
CGLError CGLSelectDispatch(void) { return kCGLNoError; }
CGLError CGLSelectDispatchBounded(void) { return kCGLNoError; }
CGLError CGLSelectDispatchFunction(void) { return kCGLNoError; }
void CGLOpenCLMuxLockDown(void) {}
void cglBadApplicationNotMuxAwareLockDown(void) {}
CGLError GLCDescribePixelFormat(void) { return kCGLBadPixelFormat; }
CGLError GLCGetPBuffer(void) { return kCGLBadAddress; }
CGLError GLCGetParameter(void) { return kCGLBadEnumeration; }
CGLError GLCGetSurface(void) { return kCGLBadDrawable; }
CGLError GLCGetVirtualScreen(void) { return kCGLBadValue; }
void *GLCGetProfilerStorage(void) { return NULL; }
void GLCSetProfilerStorage(void) {}
void glcDebugListener(void) {}
void *glcGetIOAccelService(void) { return NULL; }
void glcNoop(void) {}
CGLError glcPluginConnect(void) { return kCGLBadCodeModule; }
GLint glcPluginCount(void) { return 0; }
void glcPluginDisconnect(void) {}
void glcRecordError(void) {}

#endif /* MGL_GL_CORE */
