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
 * mgl_es.h
 * MGL
 *
 * OpenGL ES 3.1 contexts, made by the same library as desktop ones. An ES
 * context runs the desktop driver underneath, with its own strings, limits,
 * errors and shading language rules on top.
 */

#ifndef mgl_es_h
#define mgl_es_h

#include "glm_dispatch.h"

#ifdef MGL_OGL_ES

#ifndef MGL_COMPAT_CONTEXTS
#error "OpenGL ES contexts need the compatibility profile"
#endif

// the ES version every ES request gets
#define MGL_ES_MAJOR 3
#define MGL_ES_MINOR 1

typedef struct MGLES_t {
    // the desktop entry points the ES ones wrap
    struct GLMDispatchTable next;

    char *extension_string;
} MGLES;

struct GLMContextRec_t;

void mglESInit(struct GLMContextRec_t *ctx);
void mglESFree(struct GLMContextRec_t *ctx);

// For a shader in an ES context: NULL when it compiles, with *out the source
// to compile in its place (NULL to use src as it is), or else the info log,
// which the caller frees.
char *mglESShaderSource(struct GLMContextRec_t *ctx, const char *src, GLenum type, char **out);

// a colour format an ES framebuffer can render to
bool mglESColorRenderable(GLenum internalformat);

// the precision an ES shader gave an in or out variable: 'h', 'm' or 'l',
// or 0 when it cannot tell
char mglESVaryingPrecision(const char *src, const char *name, bool fragment);

#endif /* MGL_OGL_ES */

#endif /* mgl_es_h */
