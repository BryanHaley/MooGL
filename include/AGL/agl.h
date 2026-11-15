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
 * agl.h
 *
 * Apple's AGL, which left the macOS SDK in 10.14, for code that still calls
 * it. libmoogl implements it over CGL. The constants are Apple's. Carbon is
 * gone, so the QuickDraw, WindowRef and HIView calls are not here: an AGL
 * context draws offscreen, into framebuffer objects.
 */

#ifndef MOOGL_AGL_H
#define MOOGL_AGL_H

#include <OpenGL/gltypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *AGLDevice;                        /* a GDHandle */
typedef void *AGLDrawable;                      /* a CGrafPtr */
typedef struct __AGLPixelFormatRec *AGLPixelFormat;
typedef struct __AGLContextRec *AGLContext;

#define AGL_VERSION_3_0             1

/* pixel format attributes */
#define AGL_NONE                    0
#define AGL_ALL_RENDERERS           1
#define AGL_BUFFER_SIZE             2
#define AGL_LEVEL                   3
#define AGL_RGBA                    4
#define AGL_DOUBLEBUFFER            5
#define AGL_STEREO                  6
#define AGL_AUX_BUFFERS             7
#define AGL_RED_SIZE                8
#define AGL_GREEN_SIZE              9
#define AGL_BLUE_SIZE               10
#define AGL_ALPHA_SIZE              11
#define AGL_DEPTH_SIZE              12
#define AGL_STENCIL_SIZE            13
#define AGL_ACCUM_RED_SIZE          14
#define AGL_ACCUM_GREEN_SIZE        15
#define AGL_ACCUM_BLUE_SIZE         16
#define AGL_ACCUM_ALPHA_SIZE        17
#define AGL_PIXEL_SIZE              50
#define AGL_MINIMUM_POLICY          51
#define AGL_MAXIMUM_POLICY          52
#define AGL_OFFSCREEN               53
#define AGL_FULLSCREEN              54
#define AGL_SAMPLE_BUFFERS_ARB      55
#define AGL_SAMPLES_ARB             56
#define AGL_AUX_DEPTH_STENCIL       57
#define AGL_COLOR_FLOAT             58
#define AGL_MULTISAMPLE             59
#define AGL_SUPERSAMPLE             60
#define AGL_SAMPLE_ALPHA            61
#define AGL_RENDERER_ID             70
#define AGL_SINGLE_RENDERER         71
#define AGL_NO_RECOVERY             72
#define AGL_ACCELERATED             73
#define AGL_CLOSEST_POLICY          74
#define AGL_ROBUST                  75
#define AGL_BACKING_STORE           76
#define AGL_MP_SAFE                 78
#define AGL_WINDOW                  80
#define AGL_MULTISCREEN             81
#define AGL_VIRTUAL_SCREEN          82
#define AGL_COMPLIANT               83
#define AGL_DISPLAY_MASK            84
#define AGL_PBUFFER                 90
#define AGL_REMOTE_PBUFFER          91
#define AGL_ALLOW_OFFLINE_RENDERERS 96

/* context enables and integer parameters */
#define AGL_SWAP_RECT               200
#define AGL_BUFFER_RECT             202
#define AGL_SWAP_LIMIT              203
#define AGL_COLORMAP_TRACKING       210
#define AGL_COLORMAP_ENTRY          212
#define AGL_RASTERIZATION           220
#define AGL_SWAP_INTERVAL           222
#define AGL_STATE_VALIDATION        230
#define AGL_BUFFER_NAME             231
#define AGL_ORDER_CONTEXT_TO_FRONT  232
#define AGL_CONTEXT_SURFACE_ID      233
#define AGL_CONTEXT_DISPLAY_ID      234
#define AGL_SURFACE_ORDER           235
#define AGL_SURFACE_OPACITY         236
#define AGL_CLIP_REGION             254
#define AGL_FS_CAPTURE_SINGLE       255
#define AGL_SURFACE_BACKING_SIZE    304
#define AGL_ENABLE_SURFACE_BACKING_SIZE 305
#define AGL_SURFACE_VOLATILE        306

/* aglConfigure options */
#define AGL_FORMAT_CACHE_SIZE       501
#define AGL_CLEAR_FORMAT_CACHE      502
#define AGL_RETAIN_RENDERERS        503

/* errors */
#define AGL_NO_ERROR                0
#define AGL_BAD_ATTRIBUTE           10000
#define AGL_BAD_PROPERTY            10001
#define AGL_BAD_PIXELFMT            10002
#define AGL_BAD_RENDINFO            10003
#define AGL_BAD_CONTEXT             10004
#define AGL_BAD_DRAWABLE            10005
#define AGL_BAD_GDEV                10006
#define AGL_BAD_STATE               10007
#define AGL_BAD_VALUE               10008
#define AGL_BAD_MATCH               10009
#define AGL_BAD_ENUM                10010
#define AGL_BAD_OFFSCREEN           10011
#define AGL_BAD_FULLSCREEN          10012
#define AGL_BAD_WINDOW              10013
#define AGL_BAD_POINTER             10014
#define AGL_BAD_MODULE              10015
#define AGL_BAD_ALLOC               10016
#define AGL_BAD_CONNECTION          10017
#define AGL_INVALID_FUNCTION        10018

AGLPixelFormat aglChoosePixelFormat(const void *gdevs, GLint ndev, const GLint *attribs);
AGLPixelFormat aglCreatePixelFormat(const GLint *attribs);
void aglDestroyPixelFormat(AGLPixelFormat pix);
AGLPixelFormat aglNextPixelFormat(AGLPixelFormat pix);
GLboolean aglDescribePixelFormat(AGLPixelFormat pix, GLint attrib, GLint *value);

AGLContext aglCreateContext(AGLPixelFormat pix, AGLContext share);
GLboolean aglDestroyContext(AGLContext ctx);
GLboolean aglCopyContext(AGLContext src, AGLContext dst, GLuint mask);
GLboolean aglUpdateContext(AGLContext ctx);
GLboolean aglSetCurrentContext(AGLContext ctx);
AGLContext aglGetCurrentContext(void);

/* only NULL, which leaves the context drawing offscreen */
GLboolean aglSetDrawable(AGLContext ctx, AGLDrawable draw);
AGLDrawable aglGetDrawable(AGLContext ctx);

GLboolean aglSetVirtualScreen(AGLContext ctx, GLint screen);
GLint aglGetVirtualScreen(AGLContext ctx);
void aglGetVersion(GLint *major, GLint *minor);
GLboolean aglConfigure(GLenum pname, GLuint param);
void aglSwapBuffers(AGLContext ctx);

GLboolean aglEnable(AGLContext ctx, GLenum pname);
GLboolean aglDisable(AGLContext ctx, GLenum pname);
GLboolean aglIsEnabled(AGLContext ctx, GLenum pname);
GLboolean aglSetInteger(AGLContext ctx, GLenum pname, const GLint *params);
GLboolean aglGetInteger(AGLContext ctx, GLenum pname, GLint *params);

GLenum aglGetError(void);
const GLubyte *aglErrorString(GLenum code);
void aglResetLibrary(void);

/* the CGL objects underneath, as void * so this header needs no CGL */
GLboolean aglGetCGLContext(AGLContext ctx, void **cgl_ctx);
GLboolean aglGetCGLPixelFormat(AGLPixelFormat pix, void **cgl_pix);

#ifdef __cplusplus
}
#endif

#endif /* MOOGL_AGL_H */
