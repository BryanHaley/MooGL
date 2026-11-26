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
 * wide_lines.c
 * MGL
 *
 * Metal draws lines one pixel wide. In a compatibility context a line drawn
 * wider, or a triangle drawn as lines with glPolygonMode, runs on a copy of
 * its program with a geometry stage added, which turns each line into the
 * quad GL says it covers.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glm_context.h"
#include "compat.h"
#include "mgl_log.h"

GLuint mglCreateProgram(GLMContext ctx);
void mglDeleteProgram(GLMContext ctx, GLuint program);
void mglLinkProgram(GLMContext ctx, GLuint program);
void mglAttachShader(GLMContext ctx, GLuint program, GLuint shader);
GLuint mglCreateShader(GLMContext ctx, GLenum type);
void mglDeleteShader(GLMContext ctx, GLuint shader);
void mglShaderSource(GLMContext ctx, GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
void mglCompileShader(GLMContext ctx, GLuint shader);
void mglBindAttribLocation(GLMContext ctx, GLuint program, GLuint index, const GLchar *name);
void mglBindFragDataLocationIndexed(GLMContext ctx, GLuint program, GLuint colorNumber, GLuint index, const GLchar *name);
GLint mglGetAttribLocation(GLMContext ctx, GLuint program, const GLchar *name);
GLint mglGetFragDataLocation(GLMContext ctx, GLuint program, const GLchar *name);
GLint mglGetFragDataIndex(GLMContext ctx, GLuint program, const GLchar *name);
GLint mglGetUniformLocation(GLMContext ctx, GLuint program, const GLchar *name);
void mglProgramUniform4fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLfloat *value);
void mglProgramUniform2iv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLint *value);
void mglEnable(GLMContext ctx, GLenum cap);
void mglDisable(GLMContext ctx, GLenum cap);
void mglFrontFace(GLMContext ctx, GLenum mode);
void mglPolygonMode(GLMContext ctx, GLenum face, GLenum mode);
extern Program *findProgram(GLMContext ctx, GLuint program);

#define MGL_MAX_LINE_WIDTH 16.0f

static void addShader(GLMContext ctx, GLuint prog, GLenum type, const char *src, GLuint *made)
{
    *made = mglCreateShader(ctx, type);
    mglShaderSource(ctx, *made, 1, &src, NULL);
    mglCompileShader(ctx, *made);
    mglAttachShader(ctx, prog, *made);
}

static Program *makeTwin(GLMContext ctx, Program *p, bool triangles)
{
    char *gs = mglWideLineGeometry(p->stage_src[_VERTEX_SHADER], triangles);
    GLuint name, shaders[2] = { 0 };
    Program *twin;

    if (gs == NULL)
        return NULL;

    name = mglCreateProgram(ctx);
    twin = findProgram(ctx, name);

    if (twin == NULL)
    {
        free(gs);
        return NULL;
    }

    // its inputs and outputs share names, so only the link's rewrite can take it
    twin->wide_line_gs = gs;

    // the program's own shaders while it still has them, since a stage may be
    // split across several; otherwise the source it was linked from
    static const int stages[2] = { _VERTEX_SHADER, _FRAGMENT_SHADER };

    for (int i = 0; i < 2; i++)
    {
        int s = stages[i];

        if (p->shader_slots[s])
        {
            mglAttachShader(ctx, name, p->shader_slots[s]->name);
            for (int k = 0; k < MGL_STAGE_EXTRA_UNITS && p->also_attached[s][k]; k++)
                mglAttachShader(ctx, name, p->also_attached[s][k]->name);
        }
        else if (p->stage_src[s])
            addShader(ctx, name, s == _VERTEX_SHADER ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER, p->stage_src[s],
                      &shaders[i]);
    }

    // the same attribute and output locations, wherever they came from
    SpirvResourceList *in = &p->spirv_resources_list[_VERTEX_SHADER][SPVC_RESOURCE_TYPE_STAGE_INPUT];
    SpirvResourceList *out = &p->spirv_resources_list[_FRAGMENT_SHADER][SPVC_RESOURCE_TYPE_STAGE_OUTPUT];

    for (GLuint i = 0; i < in->count; i++)
    {
        const char *n = in->list[i].name;
        GLint loc = n ? mglGetAttribLocation(ctx, p->name, n) : -1;

        if (loc >= 0)
            mglBindAttribLocation(ctx, name, (GLuint)loc, n);
    }

    for (GLuint i = 0; i < out->count; i++)
    {
        const char *n = out->list[i].name;
        GLint loc = n ? mglGetFragDataLocation(ctx, p->name, n) : -1;

        if (loc >= 0)
            mglBindFragDataLocationIndexed(ctx, name, (GLuint)loc, (GLuint)mglGetFragDataIndex(ctx, p->name, n), n);
    }

    mglLinkProgram(ctx, name);

    for (int i = 0; i < 2; i++)
        if (shaders[i])
            mglDeleteShader(ctx, shaders[i]);

    if (twin->link_status != GL_TRUE)
    {
        MGL_DEBUG("wide lines: program %u did not link with a geometry stage:\n%s\n", p->name,
                  twin->log ? twin->log : "");
        mglDeleteProgram(ctx, name);
        return NULL;
    }

    return twin;
}

static void dropTwin(GLMContext ctx, Program **twin, GLuint *serial)
{
    if (*twin)
    {
        ctx->error_suppress++;
        mglDeleteProgram(ctx, (*twin)->name);
        ctx->error_suppress--;
    }

    *twin = NULL;
    *serial = 0;
}

void mglDropWideTwin(GLMContext ctx, Program *p)
{
    dropTwin(ctx, &p->wide_twin, &p->wide_twin_serial);
    dropTwin(ctx, &p->wide_tri_twin, &p->wide_tri_twin_serial);
}

void mglWideLinesEnter(GLMContext ctx, GLenum mode, MglWideDraw *wd)
{
    wd->was = NULL;

#ifdef MGL_COMPAT_CONTEXTS
    Program *p = STATE(program);
    TransformFeedback *tf = STATE(transform_feedback);
    float width = fminf(roundf(STATE_VAR(line_width)), MGL_MAX_LINE_WIDTH);

    if (ctx->compat == NULL)
        return;

    GLsizei split_tris = 0;
    GLenum split = mglCompatSplitMode(ctx, &split_tris);
    bool lines = mode == GL_LINES || mode == GL_LINE_STRIP || mode == GL_LINE_LOOP;
    bool triangles = (mode == GL_TRIANGLES || mode == GL_TRIANGLE_STRIP || mode == GL_TRIANGLE_FAN) &&
                     STATE_VAR(polygon_mode) == GL_LINE;

    // a split quad's wireframe needs the edges inside it left out, even 1 pixel wide
    if (!(lines && width > 1.0f) && !(triangles && (width > 1.0f || split)))
        return;

    // a stage of its own already decides what is drawn, and feedback records lines
    if (p == NULL || !p->executable || p->stage_src[_VERTEX_SHADER] == NULL || mglProgramHasGeometry(p) ||
        p->tess.active || mglProgramIsMesh(p) || (tf && tf->active))
        return;

    GLenum err = STATE(error);

    ctx->error_suppress++;

    Program **slot = triangles ? &p->wide_tri_twin : &p->wide_twin;
    GLuint *serial = triangles ? &p->wide_tri_twin_serial : &p->wide_twin_serial;

    if (*serial != p->link_serial)
    {
        dropTwin(ctx, slot, serial);
        *slot = makeTwin(ctx, p, triangles);
        *serial = p->link_serial;
    }

    Program *twin = *slot;

    if (twin)
    {
        ViewportRect *vp = &STATE(viewport[0]);
        GLfloat line[4] = { vp->w > 0 ? vp->w : 1, vp->h > 0 ? vp->h : 1, width,
                            STATE_VAR(front_face) == GL_CW ? 1.0f : 0.0f };

        mglCopyProgramState(ctx, p, twin);
        mglCompatWriteBuiltins(ctx, twin);
        mglProgramUniform4fv(ctx, twin->name, mglGetUniformLocation(ctx, twin->name, "mglWideLine"), 1, line);

        if (triangles)
        {
            GLint shape[2] = { split == GL_QUADS ? 1 : split == GL_QUAD_STRIP ? 2 : split == GL_POLYGON ? 3 : 0,
                               split_tris };

            mglProgramUniform2iv(ctx, twin->name, mglGetUniformLocation(ctx, twin->name, "mglWideShape"), 1, shape);
        }

        // Lines are never culled and face the front. A triangle's edges are
        // wound as it faces, against a front face of counter-clockwise.
        wd->was = p;
        wd->cull = STATE(caps.cull_face);
        wd->offset_fill = STATE(caps.polygon_offset_fill);
        wd->front_face = STATE_VAR(front_face);
        wd->polygon_mode = STATE_VAR(polygon_mode);

        if (lines)
            mglDisable(ctx, GL_CULL_FACE);
        (STATE(caps.polygon_offset_line) ? mglEnable : mglDisable)(ctx, GL_POLYGON_OFFSET_FILL);
        mglFrontFace(ctx, GL_CCW);
        mglPolygonMode(ctx, GL_FRONT_AND_BACK, GL_FILL);

        STATE(program) = twin;
        STATE(dirty_bits) |= DIRTY_PROGRAM;
    }

    ctx->error_suppress--;
    STATE(error) = err;
#else
    (void)mode;
#endif
}

void mglWideLinesLeave(GLMContext ctx, MglWideDraw *wd)
{
    if (wd->was == NULL)
        return;

    GLenum err = STATE(error);

    STATE(program) = wd->was;
    STATE(dirty_bits) |= DIRTY_PROGRAM;

    (wd->cull ? mglEnable : mglDisable)(ctx, GL_CULL_FACE);
    (wd->offset_fill ? mglEnable : mglDisable)(ctx, GL_POLYGON_OFFSET_FILL);
    mglFrontFace(ctx, wd->front_face);
    mglPolygonMode(ctx, GL_FRONT_AND_BACK, wd->polygon_mode);

    STATE(error) = err;
}
