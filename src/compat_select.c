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
 * compat_select.c
 * MGL
 *
 * Selection mode. Nothing is drawn: each primitive's vertices go through the
 * matrices on the CPU and are clipped to the view volume and the user clip
 * planes, as a draw would be, and whatever survives makes a hit for the
 * names on the stack, with the depth range it covers. Feedback mode is
 * accepted and returns nothing.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "glm_context.h"
#include "compat.h"

#ifdef MGL_COMPAT_CONTEXTS

#define C (ctx->compat)

typedef struct {
    GLfloat clip[4];
    GLfloat eye[4];
} SelVert;

#define MAX_POLY 24

// --- hit records ------------------------------------------------------------------

static void putWord(GLMContext ctx, GLuint w)
{
    if (C->select_used < (GLuint)C->select_size)
        C->select_buffer[C->select_used] = w;
    else
        C->select_overflow = GL_TRUE;

    C->select_used++;
}

static GLuint depthWord(GLfloat z)
{
    double d = fmin(fmax(z, 0.0), 1.0) * 4294967295.0;

    return (GLuint)d;
}

static void writeHit(GLMContext ctx)
{
    if (!C->hit)
        return;

    if (C->select_used + 3 + C->name_depth > (GLuint)C->select_size)
        C->select_overflow = GL_TRUE;

    if (!C->select_overflow)
    {
        putWord(ctx, C->name_depth);
        putWord(ctx, depthWord(C->hit_min));
        putWord(ctx, depthWord(C->hit_max));

        for (GLuint i = 0; i < C->name_depth; i++)
            putWord(ctx, C->names[i]);

        C->select_hits++;
    }

    C->hit = GL_FALSE;
    C->hit_min = 1.0f;
    C->hit_max = 0.0f;
}

static void hitAt(GLMContext ctx, GLfloat z)
{
    if (!C->hit)
    {
        C->hit_min = 1.0f;
        C->hit_max = 0.0f;
    }

    C->hit = GL_TRUE;
    C->hit_min = fminf(C->hit_min, z);
    C->hit_max = fmaxf(C->hit_max, z);
}

void mglCompatSelectRaster(GLMContext ctx)
{
    if (C->render_mode == GL_SELECT)
        hitAt(ctx, C->raster_pos[2]);
}

// --- clipping --------------------------------------------------------------------

// distance to clip plane p: the six of the view volume, then the user's
static GLfloat planeDistance(GLMContext ctx, const SelVert *v, int p)
{
    switch (p)
    {
        case 0: return v->clip[3] + v->clip[0];
        case 1: return v->clip[3] - v->clip[0];
        case 2: return v->clip[3] + v->clip[1];
        case 3: return v->clip[3] - v->clip[1];
        case 4: return v->clip[3] + v->clip[2];
        case 5: return v->clip[3] - v->clip[2];
    }

    const GLfloat *pl = C->clip_planes[p - 6];

    return v->eye[0] * pl[0] + v->eye[1] * pl[1] + v->eye[2] * pl[2] + v->eye[3] * pl[3];
}

static bool planeOn(GLMContext ctx, int p)
{
    return p < 6 || STATE(caps.clip_distances[p - 6]);
}

static SelVert lerp(const SelVert *a, const SelVert *b, GLfloat t)
{
    SelVert r;

    for (int i = 0; i < 4; i++)
    {
        r.clip[i] = a->clip[i] + (b->clip[i] - a->clip[i]) * t;
        r.eye[i] = a->eye[i] + (b->eye[i] - a->eye[i]) * t;
    }

    return r;
}

static int clipPolygon(GLMContext ctx, SelVert *poly, int n)
{
    SelVert tmp[MAX_POLY];

    for (int p = 0; p < 6 + MGL_COMPAT_PLANES && n > 0; p++)
    {
        int m = 0;

        if (!planeOn(ctx, p))
            continue;

        for (int i = 0; i < n; i++)
        {
            SelVert *a = &poly[i], *b = &poly[(i + 1) % n];
            GLfloat da = planeDistance(ctx, a, p), db = planeDistance(ctx, b, p);

            if (da >= 0 && m < MAX_POLY)
                tmp[m++] = *a;

            if ((da >= 0) != (db >= 0) && m < MAX_POLY)
                tmp[m++] = lerp(a, b, da / (da - db));
        }

        memcpy(poly, tmp, (size_t)m * sizeof(SelVert));
        n = m;
    }

    return n;
}

static GLfloat windowZ(GLMContext ctx, const SelVert *v)
{
    GLdouble n = STATE(depth_range[0].znear), f = STATE(depth_range[0].zfar);

    return (GLfloat)(n + (v->clip[2] / v->clip[3] + 1.0) * (f - n) * 0.5);
}

static void hitPolygon(GLMContext ctx, SelVert *poly, int n)
{
    n = clipPolygon(ctx, poly, n);

    for (int i = 0; i < n; i++)
        hitAt(ctx, windowZ(ctx, &poly[i]));
}

// whether a triangle faces away and culling removes it
static bool culled(GLMContext ctx, const SelVert *a, const SelVert *b, const SelVert *c)
{
    GLfloat ax, ay, bx, by, cx, cy, area;
    bool front;

    if (!STATE(caps.cull_face))
        return false;

    if (STATE_VAR(cull_face_mode) == GL_FRONT_AND_BACK)
        return true;

    // the sign of the area only needs the perspective divide
    if (a->clip[3] <= 0 || b->clip[3] <= 0 || c->clip[3] <= 0)
        return false;

    ax = a->clip[0] / a->clip[3]; ay = a->clip[1] / a->clip[3];
    bx = b->clip[0] / b->clip[3]; by = b->clip[1] / b->clip[3];
    cx = c->clip[0] / c->clip[3]; cy = c->clip[1] / c->clip[3];
    area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
    front = STATE_VAR(front_face) == GL_CW ? area < 0 : area > 0;

    return STATE_VAR(cull_face_mode) == GL_BACK ? !front : front;
}

static void triangle(GLMContext ctx, const SelVert *a, const SelVert *b, const SelVert *c)
{
    SelVert poly[MAX_POLY] = { *a, *b, *c };

    if (!culled(ctx, a, b, c))
        hitPolygon(ctx, poly, 3);
}

static void line(GLMContext ctx, const SelVert *a, const SelVert *b)
{
    SelVert poly[MAX_POLY] = { *a, *b };

    // a segment clips like a polygon of two vertices
    hitPolygon(ctx, poly, 2);
}

static void point(GLMContext ctx, const SelVert *a)
{
    for (int p = 0; p < 6 + MGL_COMPAT_PLANES; p++)
        if (planeOn(ctx, p) && planeDistance(ctx, a, p) < 0)
            return;

    hitAt(ctx, windowZ(ctx, a));
}

void mglCompatSelectVertices(GLMContext ctx, GLenum mode, const GLfloat *verts, GLsizei count)
{
    SelVert *v;
    GLfloat mvp[16];

    if (C->render_mode != GL_SELECT || count <= 0)
        return;

    v = (SelVert *)malloc((size_t)count * sizeof(SelVert));
    if (v == NULL)
        return;

    mglMatMul(mvp, mglCompatProjection(ctx), mglCompatModelview(ctx));

    for (GLsizei i = 0; i < count; i++)
    {
        const GLfloat *p = verts + (size_t)i * MAX_ATTRIBS * 4;

        mglMatTransform(mglCompatModelview(ctx), p, v[i].eye);
        mglMatTransform(mvp, p, v[i].clip);
    }

    switch (mode)
    {
        case GL_POINTS:
            for (GLsizei i = 0; i < count; i++)
                point(ctx, &v[i]);
            break;

        case GL_LINES:
            for (GLsizei i = 0; i + 1 < count; i += 2)
                line(ctx, &v[i], &v[i + 1]);
            break;

        case GL_LINE_STRIP:
        case GL_LINE_LOOP:
            for (GLsizei i = 0; i + 1 < count; i++)
                line(ctx, &v[i], &v[i + 1]);
            if (mode == GL_LINE_LOOP && count > 2)
                line(ctx, &v[count - 1], &v[0]);
            break;

        case GL_TRIANGLES:
            for (GLsizei i = 0; i + 2 < count; i += 3)
                triangle(ctx, &v[i], &v[i + 1], &v[i + 2]);
            break;

        case GL_TRIANGLE_STRIP:
            for (GLsizei i = 0; i + 2 < count; i++)
            {
                if (i & 1)
                    triangle(ctx, &v[i + 1], &v[i], &v[i + 2]);
                else
                    triangle(ctx, &v[i], &v[i + 1], &v[i + 2]);
            }
            break;

        case GL_TRIANGLE_FAN:
        case GL_POLYGON:
            for (GLsizei i = 1; i + 1 < count; i++)
                triangle(ctx, &v[0], &v[i], &v[i + 1]);
            break;

        case GL_QUADS:
            for (GLsizei i = 0; i + 3 < count; i += 4)
            {
                triangle(ctx, &v[i], &v[i + 1], &v[i + 2]);
                triangle(ctx, &v[i], &v[i + 2], &v[i + 3]);
            }
            break;

        case GL_QUAD_STRIP:
            for (GLsizei i = 0; i + 3 < count; i += 2)
            {
                triangle(ctx, &v[i], &v[i + 1], &v[i + 3]);
                triangle(ctx, &v[i], &v[i + 3], &v[i + 2]);
            }
            break;
    }

    free(v);
}

// --- the entry points ---------------------------------------------------------------

static void cSelectBuffer(GLMContext ctx, GLsizei size, GLuint *buffer)
{
    ERROR_CHECK_RETURN(size >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(C->render_mode != GL_SELECT, GL_INVALID_OPERATION);

    C->select_buffer = buffer;
    C->select_size = size;
}

static void cFeedbackBuffer(GLMContext ctx, GLsizei size, GLenum type, GLfloat *buffer)
{
    ERROR_CHECK_RETURN(size >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(type == GL_2D || type == GL_3D || type == GL_3D_COLOR || type == GL_3D_COLOR_TEXTURE ||
                       type == GL_4D_COLOR_TEXTURE, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(C->render_mode != GL_FEEDBACK, GL_INVALID_OPERATION);

    C->feedback_buffer = buffer;
    C->feedback_size = size;
    C->feedback_type = type;
}

static GLint cRenderMode(GLMContext ctx, GLenum mode)
{
    GLint result = 0;

    ERROR_CHECK_RETURN_VALUE(mode == GL_RENDER || mode == GL_SELECT || mode == GL_FEEDBACK, GL_INVALID_ENUM, 0);
    ERROR_CHECK_RETURN_VALUE(!C->in_begin, GL_INVALID_OPERATION, 0);
    ERROR_CHECK_RETURN_VALUE(mode != GL_SELECT || C->select_buffer, GL_INVALID_OPERATION, 0);
    ERROR_CHECK_RETURN_VALUE(mode != GL_FEEDBACK || C->feedback_buffer, GL_INVALID_OPERATION, 0);

    if (C->render_mode == GL_SELECT)
    {
        writeHit(ctx);
        result = C->select_overflow ? -1 : (GLint)C->select_hits;
    }

    C->render_mode = mode;
    C->select_used = 0;
    C->select_hits = 0;
    C->select_overflow = GL_FALSE;
    C->name_depth = 0;
    C->hit = GL_FALSE;

    return result;
}

static void cInitNames(GLMContext ctx)
{
    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);

    if (C->render_mode != GL_SELECT)
        return;

    writeHit(ctx);
    C->name_depth = 0;
}

static void cPushName(GLMContext ctx, GLuint name)
{
    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);

    if (C->render_mode != GL_SELECT)
        return;

    ERROR_CHECK_RETURN(C->name_depth < MGL_NAME_DEPTH, GL_STACK_OVERFLOW);

    writeHit(ctx);
    C->names[C->name_depth++] = name;
}

static void cPopName(GLMContext ctx)
{
    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);

    if (C->render_mode != GL_SELECT)
        return;

    ERROR_CHECK_RETURN(C->name_depth > 0, GL_STACK_UNDERFLOW);

    writeHit(ctx);
    C->name_depth--;
}

static void cLoadName(GLMContext ctx, GLuint name)
{
    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);

    if (C->render_mode != GL_SELECT)
        return;

    ERROR_CHECK_RETURN(C->name_depth > 0, GL_INVALID_OPERATION);

    writeHit(ctx);
    C->names[C->name_depth - 1] = name;
}

static void cPassThrough(GLMContext ctx, GLfloat token)
{
    (void)token;
    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);
}

#define SET(field, fn) ctx->dispatch.field = fn

void mglCompatInstallSelect(GLMContext ctx)
{
    SET(select_buffer, cSelectBuffer);
    SET(feedback_buffer, cFeedbackBuffer);
    SET(render_mode, cRenderMode);
    SET(init_names, cInitNames);
    SET(push_name, cPushName);
    SET(pop_name, cPopName);
    SET(load_name, cLoadName);
    SET(pass_through, cPassThrough);
}

#endif /* MGL_COMPAT_CONTEXTS */
