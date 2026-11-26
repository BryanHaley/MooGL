/*
 * test_cgl.c
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
 * Contexts made the old way, through CGL and AGL, before any GL call: what
 * screen savers, visualisers and early ports did. Linked to libmoogl alone,
 * so every CGL, AGL and GL call here lands in MooGL.
 */

#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <AGL/agl.h>

#include <stdio.h>
#include <string.h>

#ifndef GL_CONTEXT_PROFILE_MASK
#define GL_CONTEXT_PROFILE_MASK 0x9126
#define GL_CONTEXT_CORE_PROFILE_BIT 0x00000001
#endif

static int fails;

static void check(int ok, const char *what)
{
    printf("  [%s] %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

static int near(GLubyte a, int b) { return a + 3 >= b && a <= b + 3; }

static int pixelIs(int x, int y, int r, int g, int b)
{
    GLubyte p[4];

    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
    return near(p[0], r) && near(p[1], g) && near(p[2], b);
}

// the default framebuffer, red, with a green quad over its left half
static int drawsWithBeginEnd(void)
{
    GLint vp[4];

    glGetIntegerv(GL_VIEWPORT, vp);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(0, 1, 0);
    glBegin(GL_QUADS);
    glVertex2f(-1, -1); glVertex2f(0, -1); glVertex2f(0, 1); glVertex2f(-1, 1);
    glEnd();

    return pixelIs(vp[2] / 4, vp[3] / 2, 0, 255, 0) && pixelIs(3 * vp[2] / 4, vp[3] / 2, 255, 0, 0) &&
           glGetError() == GL_NO_ERROR;
}

static void cgl(void)
{
    CGLPixelFormatAttribute legacy[] = {
        kCGLPFADoubleBuffer, kCGLPFAAccelerated, kCGLPFADepthSize, (CGLPixelFormatAttribute)24,
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_Legacy, (CGLPixelFormatAttribute)0
    };
    CGLPixelFormatAttribute core[] = {
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core, (CGLPixelFormatAttribute)0
    };
    CGLPixelFormatAttribute stereo[] = { kCGLPFAStereo, (CGLPixelFormatAttribute)0 };
    CGLPixelFormatObj pix = NULL, core_pix = NULL, none = NULL;
    CGLContextObj ctx = NULL, shared = NULL, core_ctx = NULL, mixed = NULL;
    GLint npix = 0, depth = 0, interval = -1;
    GLuint tex = 0;

    printf("CGL\n");

    check(CGLChoosePixelFormat(legacy, &pix, &npix) == kCGLNoError && pix && npix == 1,
          "CGLChoosePixelFormat finds a legacy format");
    CGLDescribePixelFormat(pix, 0, kCGLPFADepthSize, &depth);
    check(depth >= 24, "with the depth buffer it asked for");

    check(CGLCreateContext(pix, NULL, &ctx) == kCGLNoError && ctx, "CGLCreateContext makes a context");
    check(CGLSetCurrentContext(ctx) == kCGLNoError && CGLGetCurrentContext() == ctx, "it can be made current");

    const char *version = (const char *)glGetString(GL_VERSION);
    printf("  %s\n", version ? version : "(none)");
    // Apple's legacy contexts said 2.1, and old programs choose their paths by it
    check(version && strncmp(version, "2.1 ", 4) == 0, "it reports 2.1, as Apple's legacy contexts did");
    check(drawsWithBeginEnd(), "glBegin draws in it and glReadPixels reads it back");
    check(CGLFlushDrawable(ctx) == kCGLNoError, "CGLFlushDrawable");

    GLint zero = 0;
    CGLSetParameter(ctx, kCGLCPSwapInterval, &zero);
    CGLGetParameter(ctx, kCGLCPSwapInterval, &interval);
    check(interval == 0, "kCGLCPSwapInterval is kept");

    check(CGLRetainContext(ctx) == ctx && CGLGetContextRetainCount(ctx) == 2, "contexts are retained");
    CGLReleaseContext(ctx);

    // a context made to share sees the first one's textures
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    check(CGLCreateContext(pix, ctx, &shared) == kCGLNoError && shared, "a context can share with it");
    CGLSetCurrentContext(shared);
    check(glIsTexture(tex) && CGLGetShareGroup(shared) == CGLGetShareGroup(ctx), "and sees its texture");

    check(CGLChoosePixelFormat(core, &core_pix, &npix) == kCGLNoError && core_pix,
          "a GL 4 core format");
    check(CGLCreateContext(core_pix, NULL, &core_ctx) == kCGLNoError, "makes a context");
    CGLSetCurrentContext(core_ctx);
    GLint mask = 0;
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &mask);
    version = (const char *)glGetString(GL_VERSION);
    check(version && !strncmp(version, "4.6", 3) && mask == GL_CONTEXT_CORE_PROFILE_BIT, "that is a 4.6 core context");

    check(CGLCreateContext(core_pix, ctx, &mixed) == kCGLBadMatch && mixed == NULL,
          "a core and a legacy context can't share");

    check(CGLChoosePixelFormat(stereo, &none, &npix) == kCGLNoError && none == NULL && npix == 0,
          "a stereo format matches nothing");

    CGLSetCurrentContext(NULL);
    check(CGLGetCurrentContext() == NULL, "no context is current after CGLSetCurrentContext(NULL)");

    CGLDestroyContext(core_ctx);
    CGLDestroyContext(shared);
    CGLDestroyContext(ctx);
    CGLDestroyPixelFormat(core_pix);
    CGLDestroyPixelFormat(pix);
}

static void agl(void)
{
    const GLint attribs[] = { AGL_RGBA, AGL_DOUBLEBUFFER, AGL_DEPTH_SIZE, 16, AGL_NONE };
    GLint interval = -1, one = 1, major = 0, minor = 0;
    void *cgl_ctx = NULL;

    printf("AGL\n");

    aglGetVersion(&major, &minor);
    check(major == 3, "aglGetVersion says 3.0");

    AGLPixelFormat pix = aglChoosePixelFormat(NULL, 0, attribs);
    check(pix != NULL, "aglChoosePixelFormat finds a format");

    AGLContext ctx = aglCreateContext(pix, NULL);
    check(ctx != NULL, "aglCreateContext makes a context");
    check(aglSetCurrentContext(ctx) && aglGetCurrentContext() == ctx, "it can be made current");
    check(drawsWithBeginEnd(), "glBegin draws in it");
    aglSwapBuffers(ctx);
    check(aglGetError() == AGL_NO_ERROR, "aglSwapBuffers");

    aglSetInteger(ctx, AGL_SWAP_INTERVAL, &one);
    aglGetInteger(ctx, AGL_SWAP_INTERVAL, &interval);
    check(interval == 1, "AGL_SWAP_INTERVAL is kept");

    check(aglGetCGLContext(ctx, &cgl_ctx) && cgl_ctx == CGLGetCurrentContext(), "it is a CGL context underneath");

    check(!aglSetDrawable(ctx, (AGLDrawable)&interval) && aglGetError() == AGL_BAD_DRAWABLE,
          "a QuickDraw drawable is refused, as Carbon is gone");

    aglSetCurrentContext(NULL);
    check(aglDestroyContext(ctx), "aglDestroyContext");
    aglDestroyPixelFormat(pix);
}

int main(void)
{
    cgl();
    agl();
    printf("%s\n", fails ? "FAILED" : "passed");
    return fails ? 1 : 0;
}
