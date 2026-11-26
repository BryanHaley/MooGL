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
 * cgl_window.m
 * MGL
 *
 * The AppKit side of CGL: putting a CGL context in a window, and textures
 * made from an IOSurface. NSOpenGLContext gives CGL a window number through
 * the private CGLSetSurface; the view is whatever setView: was handed.
 */

#ifdef MGL_GL_CORE

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <IOSurface/IOSurface.h>
#import <objc/runtime.h>

#include "glm_context.h"
#include "mgl_log.h"

// the GL calls here are MooGL's, not the ones Apple's headers deprecate
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

void MGLsetContextView(GLMContext ctx, void *view);
void mtlSetSublayerView(GLMContext glm_ctx, void *view);
Texture *findTexture(GLMContext ctx, GLuint texture);

@interface NSObject (MGLCGLInternal)
- (bool) bindMTLTexture:(Texture *)tex;
@end

// the view NSOpenGLContext's setView: is attaching, while it runs
static __thread void *pending_view;
static IMP original_set_view;

static void setViewWatched(id self, SEL cmd, NSView *view)
{
    void *was = pending_view;

    pending_view = (__bridge void *)view;
    ((void (*)(id, SEL, NSView *))original_set_view)(self, cmd, view);
    pending_view = was;
}

void mglCGLWatchViews(void)
{
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        Class c = NSClassFromString(@"NSOpenGLContext");
        Method m = c ? class_getInstanceMethod(c, @selector(setView:)) : NULL;

        if (m)
            original_set_view = method_setImplementation(m, (IMP)setViewWatched);
    });
}

// NSCGLSurface copies the GL surface into a layer of its own after every
// flush. MooGL's layer is on top, so that copy only costs a draw a frame.
static void flushRectSkipped(id self, SEL cmd, NSRect rect)
{
}

static void skipSurfaceCopies(void)
{
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        Class c = NSClassFromString(@"NSCGLSurface");
        Method m = c ? class_getInstanceMethod(c, @selector(flushRect:)) : NULL;

        if (m)
            method_setImplementation(m, (IMP)flushRectSkipped);
    });
}

static void onMain(dispatch_block_t block)
{
    if ([NSThread isMainThread])
        block();
    else
        dispatch_sync(dispatch_get_main_queue(), block);
}

// *view is the context's view, retained; NULL detaches
bool mglCGLAttachWindow(GLMContext glm, int window_number, void **view)
{
    __block NSView *v = (__bridge NSView *)pending_view;

    if (window_number == 0)
    {
        onMain(^{ MGLsetContextView(glm, NULL); });
        if (*view)
            CFBridgingRelease(*view);
        *view = NULL;
        return true;
    }

    if (v == nil && *view)
        v = (__bridge NSView *)*view;

    // only AppKit calls CGLSetSurface, so AppKit's CGL is MooGL's here
    skipSurfaceCopies();

    // the number is often a surface window AppKit made for the view, not
    // the view's own, so it only finds a view when setView: gave none
    onMain(^{
        if (v == nil)
            v = [[NSApp windowWithWindowNumber: window_number] contentView];
        if (v)
            mtlSetSublayerView(glm, (__bridge void *)v);
    });

    MGL_NSDEBUG(@"CGL window %d: view %@", window_number, v);
    if (v == nil)
        return false;

    if (*view != (__bridge void *)v)
    {
        if (*view)
            CFBridgingRelease(*view);
        *view = (void *)CFBridgingRetain(v);
    }
    return true;
}

static MTLPixelFormat surfaceFormat(IOSurfaceRef surface, size_t plane)
{
    switch (IOSurfaceGetPixelFormat(surface))
    {
        case 'BGRA': return MTLPixelFormatBGRA8Unorm;
        case 'RGBA': return MTLPixelFormatRGBA8Unorm;
        case 'l10r': return MTLPixelFormatBGR10A2Unorm;
        case 'RGhA': return MTLPixelFormatRGBA16Float;
        case 'RGfA': return MTLPixelFormatRGBA32Float;
        case 'L008': return MTLPixelFormatR8Unorm;
        case 'L016': return MTLPixelFormatR16Unorm;
        case '2C08': return MTLPixelFormatRG8Unorm;
        default:
            // one byte per channel by the bytes in an element, as for video planes
            switch (IOSurfaceGetBytesPerElementOfPlane(surface, plane))
            {
                case 1: return MTLPixelFormatR8Unorm;
                case 2: return MTLPixelFormatRG8Unorm;
                case 4: return MTLPixelFormatBGRA8Unorm;
                default: return MTLPixelFormatInvalid;
            }
    }
}

// The bound texture gets GL storage of this size, then its Metal texture is
// swapped for one that reads and writes the IOSurface itself.
bool mglCGLTexImageIOSurface(GLMContext ctx, GLenum target, GLenum internal_format, GLsizei width,
                             GLsizei height, GLenum format, GLenum type, IOSurfaceRef surface, GLuint plane)
{
    GLenum binding = target == GL_TEXTURE_RECTANGLE ? GL_TEXTURE_BINDING_RECTANGLE : GL_TEXTURE_BINDING_2D;
    GLint name = 0;
    Texture *tex;

    if (surface == NULL || (target != GL_TEXTURE_RECTANGLE && target != GL_TEXTURE_2D))
        return false;

    glTexImage2D(target, 0, internal_format, width, height, 0, format, type, NULL);
    glGetIntegerv(binding, &name);

    if (name == 0 || (tex = findTexture(ctx, name)) == NULL)
        return false;

    @autoreleasepool {
        id renderer = (__bridge id)ctx->mtl_funcs.mtlObj;
        MTLPixelFormat pf = surfaceFormat(surface, plane);
        MTLTextureDescriptor *desc;
        id<MTLTexture> t;

        if (pf == MTLPixelFormatInvalid || ![renderer bindMTLTexture: tex])
            return false;

        desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat: pf width: width height: height mipmapped: NO];
        desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
        desc.storageMode = MTLStorageModeShared;

        t = [[(__bridge id<MTLTexture>)tex->mtl_data device] newTextureWithDescriptor: desc iosurface: surface plane: plane];
        if (t == nil)
            return false;

        if (tex->mtl_sample_view)
        {
            CFBridgingRelease(tex->mtl_sample_view);
            tex->mtl_sample_view = NULL;
            tex->mtl_sample_base = NULL;
        }
        if (tex->mtl_data)
            CFBridgingRelease(tex->mtl_data);
        tex->mtl_data = (void *)CFBridgingRetain(t);
        tex->dirty_bits &= ~(DIRTY_TEXTURE_DATA | DIRTY_TEXTURE_LEVEL);
    }
    return true;
}

// the GPU MooGL draws with, for renderer info
void mglCGLDeviceInfo(uint64_t *registry_id, uint64_t *memory)
{
    id<MTLDevice> d = MTLCreateSystemDefaultDevice();

    *registry_id = d ? d.registryID : 0;
    *memory = d ? d.recommendedMaxWorkingSetSize : 0;
}

#endif
