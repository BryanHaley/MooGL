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
 * mgl_compat.h
 * MGL
 *
 */

#ifndef mgl_compat_h
#define mgl_compat_h

/*
 * Bits of the compatibility profile that shipped games still reach for.
 *
 * The alpha test and GL_CLAMP answer in every context. Everything else the
 * core profile removed (glBegin, the matrix stacks, fixed-function lighting
 * and texturing, display lists, selection, the raster position) answers only
 * in a compatibility profile context, so a core context keeps the errors the
 * conformance suite expects. Build with -DMGL_NO_COMPAT_PROFILE for a driver
 * with none of it.
 */

#if !defined(MGL_NO_COMPAT_PROFILE) && !defined(MGL_COMPAT_PROFILE)
#define MGL_COMPAT_PROFILE 1
#endif

#include "mgl_legacy_enums.h"

// compatibility profile contexts, which only the desktop library makes
#if defined(MGL_COMPAT_PROFILE) && defined(MGL_GL_CORE)
#define MGL_COMPAT_CONTEXTS 1
#endif

#ifdef MGL_COMPAT_CONTEXTS

// The legacy arrays are generic attributes, at the slots NVIDIA's drivers
// use, so glColor and glVertexAttrib(3) are the same value.
#define MGL_ATTR_VERTEX     0
#define MGL_ATTR_NORMAL     2
#define MGL_ATTR_COLOR      3
#define MGL_ATTR_SECONDARY  4
#define MGL_ATTR_FOG        5
#define MGL_ATTR_TEXCOORD0  8

#define MGL_COMPAT_UNITS    8
#define MGL_COMPAT_LIGHTS   8
#define MGL_COMPAT_PLANES   8

struct MGLCompat_t;
typedef struct MGLCompat_t MGLCompat;

// --- set up and tear down (compat_state.c) ---
struct GLMContextRec_t;
void mglCompatInit(struct GLMContextRec_t *ctx);
void mglCompatFree(struct GLMContextRec_t *ctx);

void mglCompatFreeListTable(void *table);

// GLSL 1.10 to 1.30, or a compatibility profile shader, as GLSL 4.10 reading
// the driver's own uniforms; NULL when the source needs no change
char *mglCompatRewriteGLSL(const char *src, GLenum type);

// a vertex array that may point into client memory: the default one, or
// immediate mode's own
GLboolean mglCompatClientArraysAllowed(struct GLMContextRec_t *ctx);

#endif /* MGL_COMPAT_CONTEXTS */

#endif /* mgl_compat_h */
