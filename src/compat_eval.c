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
 * compat_eval.c
 * MGL
 *
 * Evaluators: glMap1/glMap2, glMapGrid, glEvalCoord, glEvalPoint and
 * glEvalMesh. Points are worked out on the CPU and sent through immediate
 * mode, the way glVertex would send them.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "glm_context.h"
#include "compat.h"

#ifdef MGL_COMPAT_CONTEXTS

#define C (ctx->compat)

void mglVertexAttrib4f(GLMContext ctx, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);

enum { MAP_COLOR, MAP_INDEX, MAP_NORMAL, MAP_TC1, MAP_TC2, MAP_TC3, MAP_TC4, MAP_V3, MAP_V4 };

static const int map_size[MGL_EVAL_MAPS] = { 4, 1, 3, 1, 2, 3, 4, 3, 4 };
static const GLfloat map_default[MGL_EVAL_MAPS][4] = {
    { 1, 1, 1, 1 }, { 1 }, { 0, 0, 1 }, { 0 }, { 0, 0 }, { 0, 0, 0 }, { 0, 0, 0, 1 }, { 0, 0, 0 }, { 0, 0, 0, 1 },
};

enum { OP_MAP1, OP_MAP2, OP_GRID1, OP_GRID2, OP_COORD1, OP_COORD2, OP_POINT1, OP_POINT2, OP_MESH1, OP_MESH2 };

static MGLEvalMap *mapFor(GLMContext ctx, GLenum target, int *index, int *dims)
{
    if (target >= GL_MAP1_COLOR_4 && target <= GL_MAP1_VERTEX_4)
    {
        *index = target - GL_MAP1_COLOR_4;
        *dims = 1;
        return &C->map1[*index];
    }

    if (target >= GL_MAP2_COLOR_4 && target <= GL_MAP2_VERTEX_4)
    {
        *index = target - GL_MAP2_COLOR_4;
        *dims = 2;
        return &C->map2[*index];
    }

    return NULL;
}

static GLuint bitsOf(GLfloat f) { GLuint u; memcpy(&u, &f, 4); return u; }
static GLfloat floatOf(GLuint u) { GLfloat f; memcpy(&f, &u, 4); return f; }

// Records the call when a list is being compiled. Returns whether it should
// run now as well.
static bool listed(GLMContext ctx, const GLuint *args, int n, const GLfloat *points, size_t count)
{
    void *blob = NULL;

    if (!mglCompatRecording(ctx))
        return true;

    if (count)
    {
        blob = malloc(count * sizeof(GLfloat));
        if (blob == NULL)
            return mglCompatExecuting(ctx);
        memcpy(blob, points, count * sizeof(GLfloat));
    }

    mglCompatRecordEval(ctx, args, n, blob);
    return mglCompatExecuting(ctx);
}

// --- the maps ---------------------------------------------------------------------

static void setMap(GLMContext ctx, MGLEvalMap *m, GLfloat u1, GLfloat u2, GLint uorder, GLfloat v1, GLfloat v2,
                   GLint vorder, const GLfloat *packed, int k)
{
    size_t n = (size_t)uorder * vorder * k;
    GLfloat *points = (GLfloat *)malloc(n * sizeof(GLfloat));

    if (points == NULL)
    {
        ERROR_RETURN(GL_OUT_OF_MEMORY);
    }

    memcpy(points, packed, n * sizeof(GLfloat));
    free(m->points);
    m->points = points;
    m->u1 = u1; m->u2 = u2; m->uorder = uorder;
    m->v1 = v1; m->v2 = v2; m->vorder = vorder;
}

#define MAP_BODY(T)                                                                                  \
    int index, dims, k;                                                                              \
    MGLEvalMap *m = mapFor(ctx, target, &index, &dims);                                              \
                                                                                                     \
    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);                                          \
    ERROR_CHECK_RETURN(m != NULL && dims == DIMS, GL_INVALID_ENUM);                                  \
    k = map_size[index];                                                                             \
    ERROR_CHECK_RETURN(u1 != u2 && v1 != v2, GL_INVALID_VALUE);                                      \
    ERROR_CHECK_RETURN(uorder >= 1 && uorder <= MGL_MAX_EVAL_ORDER, GL_INVALID_VALUE);               \
    ERROR_CHECK_RETURN(vorder >= 1 && vorder <= MGL_MAX_EVAL_ORDER, GL_INVALID_VALUE);               \
    ERROR_CHECK_RETURN(ustride >= k && vstride >= k, GL_INVALID_VALUE);                              \
    ERROR_CHECK_RETURN(points != NULL, GL_INVALID_VALUE);                                            \
                                                                                                     \
    GLfloat *packed = (GLfloat *)malloc((size_t)uorder * vorder * k * sizeof(GLfloat));              \
    if (packed == NULL)                                                                              \
    {                                                                                                \
        ERROR_RETURN(GL_OUT_OF_MEMORY);                                                              \
    }                                                                                                \
    for (GLint i = 0; i < uorder; i++)                                                               \
        for (GLint j = 0; j < vorder; j++)                                                           \
            for (int c = 0; c < k; c++)                                                              \
                packed[(i * vorder + j) * k + c] = (GLfloat)((const T *)points)[i * ustride + j * vstride + c]; \
                                                                                                     \
    GLuint args[8] = { DIMS == 1 ? OP_MAP1 : OP_MAP2, target, bitsOf((GLfloat)u1), bitsOf((GLfloat)u2),   \
                       (GLuint)uorder, bitsOf((GLfloat)v1), bitsOf((GLfloat)v2), (GLuint)vorder };   \
    if (listed(ctx, args, 8, packed, (size_t)uorder * vorder * k))                                   \
        setMap(ctx, m, (GLfloat)u1, (GLfloat)u2, uorder, (GLfloat)v1, (GLfloat)v2, vorder, packed, k); \
    free(packed);

#define DIMS 1
static void cMap1f(GLMContext ctx, GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder,
                   const GLfloat *points)
{
    GLfloat v1 = 0, v2 = 1;
    GLint vorder = 1, vstride = ustride > 0 ? ustride : 1;
    MAP_BODY(GLfloat)
}

static void cMap1d(GLMContext ctx, GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder,
                   const GLdouble *points)
{
    GLdouble v1 = 0, v2 = 1;
    GLint vorder = 1, vstride = ustride > 0 ? ustride : 1;
    MAP_BODY(GLdouble)
}
#undef DIMS

#define DIMS 2
static void cMap2f(GLMContext ctx, GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder,
                   GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points)
{
    MAP_BODY(GLfloat)
}

static void cMap2d(GLMContext ctx, GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder,
                   GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points)
{
    MAP_BODY(GLdouble)
}
#undef DIMS

static void grid1(GLMContext ctx, GLint n, GLfloat u1, GLfloat u2)
{
    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(n > 0, GL_INVALID_VALUE);

    GLuint args[4] = { OP_GRID1, (GLuint)n, bitsOf(u1), bitsOf(u2) };
    if (!listed(ctx, args, 4, NULL, 0))
        return;

    C->grid1_n = n;
    C->grid1[0] = u1;
    C->grid1[1] = u2;
}

static void grid2(GLMContext ctx, GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2)
{
    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(un > 0 && vn > 0, GL_INVALID_VALUE);

    GLuint args[7] = { OP_GRID2, (GLuint)un, bitsOf(u1), bitsOf(u2), (GLuint)vn, bitsOf(v1), bitsOf(v2) };
    if (!listed(ctx, args, 7, NULL, 0))
        return;

    C->grid2_n[0] = un;
    C->grid2_n[1] = vn;
    C->grid2[0] = u1; C->grid2[1] = u2;
    C->grid2[2] = v1; C->grid2[3] = v2;
}

static void cMapGrid1f(GLMContext ctx, GLint n, GLfloat u1, GLfloat u2) { grid1(ctx, n, u1, u2); }
static void cMapGrid1d(GLMContext ctx, GLint n, GLdouble u1, GLdouble u2) { grid1(ctx, n, (GLfloat)u1, (GLfloat)u2); }
static void cMapGrid2f(GLMContext ctx, GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2)
{ grid2(ctx, un, u1, u2, vn, v1, v2); }
static void cMapGrid2d(GLMContext ctx, GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2)
{ grid2(ctx, un, (GLfloat)u1, (GLfloat)u2, vn, (GLfloat)v1, (GLfloat)v2); }

// --- evaluation -------------------------------------------------------------------

// Bernstein basis of the given order at t, and its derivative in t
static void bernstein(int order, double t, double *b, double *db)
{
    double lower[MGL_MAX_EVAL_ORDER];
    int n = order - 1;

    b[0] = 1;
    db[0] = 0;

    for (int d = 1; d <= n; d++)
    {
        if (d == n)
            memcpy(lower, b, (size_t)n * sizeof(double));

        b[d] = t * b[d - 1];
        for (int i = d - 1; i > 0; i--)
            b[i] = (1 - t) * b[i] + t * b[i - 1];
        b[0] = (1 - t) * b[0];
    }

    for (int i = 0; i <= n && n > 0; i++)
        db[i] = n * ((i > 0 ? lower[i - 1] : 0) - (i < n ? lower[i] : 0));
}

// The map's value at (u, v), and for a 2D map its derivatives in u and v
static void evalMap(const MGLEvalMap *m, int index, double u, double v, double *p, double *du, double *dv)
{
    double bu[MGL_MAX_EVAL_ORDER], dbu[MGL_MAX_EVAL_ORDER];
    double bv[MGL_MAX_EVAL_ORDER], dbv[MGL_MAX_EVAL_ORDER];
    int k = map_size[index];

    if (m->points == NULL)
    {
        for (int c = 0; c < k; c++)
        {
            p[c] = map_default[index][c];
            du[c] = dv[c] = 0;
        }
        return;
    }

    bernstein(m->uorder, (u - m->u1) / (m->u2 - m->u1), bu, dbu);
    bernstein(m->vorder, (v - m->v1) / (m->v2 - m->v1), bv, dbv);

    for (int c = 0; c < k; c++)
        p[c] = du[c] = dv[c] = 0;

    for (int i = 0; i < m->uorder; i++)
        for (int j = 0; j < m->vorder; j++)
        {
            const GLfloat *pt = m->points + (i * m->vorder + j) * k;

            for (int c = 0; c < k; c++)
            {
                p[c] += bu[i] * bv[j] * pt[c];
                du[c] += dbu[i] * bv[j] * pt[c];
                dv[c] += bu[i] * dbv[j] * pt[c];
            }
        }

    for (int c = 0; c < k; c++)
    {
        du[c] /= m->u2 - m->u1;
        dv[c] /= m->v2 - m->v1;
    }
}

static struct GLMDispatchTable *live(GLMContext ctx)
{
    return C->compiling ? &C->list_live : &ctx->dispatch;
}

static void evalCoord(GLMContext ctx, int dims, double u, double v)
{
    MGLEvalMap *maps = dims == 1 ? C->map1 : C->map2;
    GLbitfield on = dims == 1 ? C->map1_on : C->map2_on;
    double p[4], du[4], dv[4];
    GLfloat saved[3][4];
    static const GLuint slots[3] = { MGL_ATTR_COLOR, MGL_ATTR_NORMAL, MGL_ATTR_TEXCOORD0 };
    int vmap;

    if (on & (1u << MAP_V4))
        vmap = MAP_V4;
    else if (on & (1u << MAP_V3))
        vmap = MAP_V3;
    else
        return;

    // evaluated values go with this vertex only; the current ones stay
    for (int s = 0; s < 3; s++)
        memcpy(saved[s], mglCompatCurrent(ctx, slots[s]), sizeof(saved[s]));

    if (on & (1u << MAP_COLOR))
    {
        evalMap(&maps[MAP_COLOR], MAP_COLOR, u, v, p, du, dv);
        mglVertexAttrib4f(ctx, MGL_ATTR_COLOR, (GLfloat)p[0], (GLfloat)p[1], (GLfloat)p[2], (GLfloat)p[3]);
    }

    for (int t = MAP_TC4; t >= MAP_TC1; t--)
    {
        if (!(on & (1u << t)))
            continue;

        double tc[4] = { 0, 0, 0, 1 };

        evalMap(&maps[t], t, u, v, p, du, dv);
        memcpy(tc, p, (size_t)map_size[t] * sizeof(double));
        mglVertexAttrib4f(ctx, MGL_ATTR_TEXCOORD0, (GLfloat)tc[0], (GLfloat)tc[1], (GLfloat)tc[2], (GLfloat)tc[3]);
        break;
    }

    if (dims == 1 || !C->auto_normal)
    {
        if (on & (1u << MAP_NORMAL))
        {
            evalMap(&maps[MAP_NORMAL], MAP_NORMAL, u, v, p, du, dv);
            mglVertexAttrib4f(ctx, MGL_ATTR_NORMAL, (GLfloat)p[0], (GLfloat)p[1], (GLfloat)p[2], 1);
        }
    }

    evalMap(&maps[vmap], vmap, u, v, p, du, dv);

    if (dims == 2 && C->auto_normal)
    {
        // a rational surface: the derivatives of x/w, y/w, z/w, less the
        // common 1/w^2
        if (vmap == MAP_V4)
            for (int c = 0; c < 3; c++)
            {
                du[c] = du[c] * p[3] - p[c] * du[3];
                dv[c] = dv[c] * p[3] - p[c] * dv[3];
            }

        double n[3] = {
            du[1] * dv[2] - du[2] * dv[1],
            du[2] * dv[0] - du[0] * dv[2],
            du[0] * dv[1] - du[1] * dv[0],
        };
        double len = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);

        if (len > 0)
            mglVertexAttrib4f(ctx, MGL_ATTR_NORMAL, (GLfloat)(n[0] / len), (GLfloat)(n[1] / len),
                              (GLfloat)(n[2] / len), 1);
    }

    mglCompatEmitVertex(ctx, (GLfloat)p[0], (GLfloat)p[1], (GLfloat)p[2], vmap == MAP_V4 ? (GLfloat)p[3] : 1);

    for (int s = 0; s < 3; s++)
        mglVertexAttrib4f(ctx, slots[s], saved[s][0], saved[s][1], saved[s][2], saved[s][3]);
}

// grid step i of n between a and b, landing exactly on b at the end
static double gridAt(GLint i, GLint n, GLfloat a, GLfloat b)
{
    return i == n ? b : a + (double)i * (b - a) / n;
}

static void point1(GLMContext ctx, GLint i)
{
    evalCoord(ctx, 1, gridAt(i, C->grid1_n, C->grid1[0], C->grid1[1]), 0);
}

static void point2(GLMContext ctx, GLint i, GLint j)
{
    evalCoord(ctx, 2, gridAt(i, C->grid2_n[0], C->grid2[0], C->grid2[1]),
              gridAt(j, C->grid2_n[1], C->grid2[2], C->grid2[3]));
}

static void mesh1(GLMContext ctx, GLenum mode, GLint i1, GLint i2)
{
    live(ctx)->begin(ctx, mode == GL_POINT ? GL_POINTS : GL_LINE_STRIP);
    for (GLint i = i1; i <= i2; i++)
        point1(ctx, i);
    live(ctx)->end(ctx);
}

static void mesh2(GLMContext ctx, GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
    struct GLMDispatchTable *t = live(ctx);

    switch (mode)
    {
        case GL_POINT:
            t->begin(ctx, GL_POINTS);
            for (GLint j = j1; j <= j2; j++)
                for (GLint i = i1; i <= i2; i++)
                    point2(ctx, i, j);
            t->end(ctx);
            break;

        case GL_LINE:
            for (GLint j = j1; j <= j2; j++)
            {
                t->begin(ctx, GL_LINE_STRIP);
                for (GLint i = i1; i <= i2; i++)
                    point2(ctx, i, j);
                t->end(ctx);
            }
            for (GLint i = i1; i <= i2; i++)
            {
                t->begin(ctx, GL_LINE_STRIP);
                for (GLint j = j1; j <= j2; j++)
                    point2(ctx, i, j);
                t->end(ctx);
            }
            break;

        case GL_FILL:
            for (GLint j = j1; j < j2; j++)
            {
                t->begin(ctx, GL_QUAD_STRIP);
                for (GLint i = i1; i <= i2; i++)
                {
                    point2(ctx, i, j);
                    point2(ctx, i, j + 1);
                }
                t->end(ctx);
            }
            break;
    }
}

// runs an evaluation without recording what it draws a second time
#define RUN(call) do { C->no_record++; call; C->no_record--; } while (0)

static void coord(GLMContext ctx, int dims, GLfloat u, GLfloat v)
{
    GLuint args[3] = { dims == 1 ? OP_COORD1 : OP_COORD2, bitsOf(u), bitsOf(v) };

    if (listed(ctx, args, 3, NULL, 0))
        RUN(evalCoord(ctx, dims, u, v));
}

static void cEvalCoord1f(GLMContext ctx, GLfloat u) { coord(ctx, 1, u, 0); }
static void cEvalCoord1fv(GLMContext ctx, const GLfloat *u) { coord(ctx, 1, u[0], 0); }
static void cEvalCoord1d(GLMContext ctx, GLdouble u) { coord(ctx, 1, (GLfloat)u, 0); }
static void cEvalCoord1dv(GLMContext ctx, const GLdouble *u) { coord(ctx, 1, (GLfloat)u[0], 0); }
static void cEvalCoord2f(GLMContext ctx, GLfloat u, GLfloat v) { coord(ctx, 2, u, v); }
static void cEvalCoord2fv(GLMContext ctx, const GLfloat *u) { coord(ctx, 2, u[0], u[1]); }
static void cEvalCoord2d(GLMContext ctx, GLdouble u, GLdouble v) { coord(ctx, 2, (GLfloat)u, (GLfloat)v); }
static void cEvalCoord2dv(GLMContext ctx, const GLdouble *u) { coord(ctx, 2, (GLfloat)u[0], (GLfloat)u[1]); }

static void cEvalPoint1(GLMContext ctx, GLint i)
{
    GLuint args[2] = { OP_POINT1, (GLuint)i };

    if (listed(ctx, args, 2, NULL, 0))
        RUN(point1(ctx, i));
}

static void cEvalPoint2(GLMContext ctx, GLint i, GLint j)
{
    GLuint args[3] = { OP_POINT2, (GLuint)i, (GLuint)j };

    if (listed(ctx, args, 3, NULL, 0))
        RUN(point2(ctx, i, j));
}

static void cEvalMesh1(GLMContext ctx, GLenum mode, GLint i1, GLint i2)
{
    ERROR_CHECK_RETURN(mode == GL_POINT || mode == GL_LINE, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);

    GLuint args[4] = { OP_MESH1, mode, (GLuint)i1, (GLuint)i2 };
    if (listed(ctx, args, 4, NULL, 0))
        RUN(mesh1(ctx, mode, i1, i2));
}

static void cEvalMesh2(GLMContext ctx, GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
    ERROR_CHECK_RETURN(mode == GL_POINT || mode == GL_LINE || mode == GL_FILL, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);

    GLuint args[6] = { OP_MESH2, mode, (GLuint)i1, (GLuint)i2, (GLuint)j1, (GLuint)j2 };
    if (listed(ctx, args, 6, NULL, 0))
        RUN(mesh2(ctx, mode, i1, i2, j1, j2));
}

void mglCompatEvalReplay(GLMContext ctx, const GLuint *a, const void *blob)
{
    int index, dims;
    MGLEvalMap *m;

    switch (a[0])
    {
        case OP_MAP1:
        case OP_MAP2:
            m = mapFor(ctx, a[1], &index, &dims);
            if (m && blob)
                setMap(ctx, m, floatOf(a[2]), floatOf(a[3]), (GLint)a[4], floatOf(a[5]), floatOf(a[6]),
                       (GLint)a[7], (const GLfloat *)blob, map_size[index]);
            break;
        case OP_GRID1: grid1(ctx, (GLint)a[1], floatOf(a[2]), floatOf(a[3])); break;
        case OP_GRID2:
            grid2(ctx, (GLint)a[1], floatOf(a[2]), floatOf(a[3]), (GLint)a[4], floatOf(a[5]), floatOf(a[6]));
            break;
        case OP_COORD1: evalCoord(ctx, 1, floatOf(a[1]), 0); break;
        case OP_COORD2: evalCoord(ctx, 2, floatOf(a[1]), floatOf(a[2])); break;
        case OP_POINT1: point1(ctx, (GLint)a[1]); break;
        case OP_POINT2: point2(ctx, (GLint)a[1], (GLint)a[2]); break;
        case OP_MESH1: mesh1(ctx, a[1], (GLint)a[2], (GLint)a[3]); break;
        case OP_MESH2: mesh2(ctx, a[1], (GLint)a[2], (GLint)a[3], (GLint)a[4], (GLint)a[5]); break;
    }
}

// --- queries ----------------------------------------------------------------------

// glGetMap's values as doubles; returns how many, or -1 for a bad name
static int getMap(GLMContext ctx, GLenum target, GLenum query, GLdouble *out)
{
    int index, dims;
    MGLEvalMap *m = mapFor(ctx, target, &index, &dims);
    int k, n = 0;

    if (m == NULL)
        return -1;

    k = map_size[index];

    switch (query)
    {
        case GL_ORDER:
            out[n++] = m->uorder;
            if (dims == 2)
                out[n++] = m->vorder;
            return n;

        case GL_DOMAIN:
            out[n++] = m->u1;
            out[n++] = m->u2;
            if (dims == 2)
            {
                out[n++] = m->v1;
                out[n++] = m->v2;
            }
            return n;

        case GL_COEFF:
            for (int i = 0; i < m->uorder * m->vorder * k; i++)
                out[n++] = m->points ? m->points[i] : map_default[index][i];
            return n;
    }

    return -1;
}

#define GET_MAP(name, T)                                                                   \
static void name(GLMContext ctx, GLenum target, GLenum query, T *v)                        \
{                                                                                          \
    GLdouble d[MGL_MAX_EVAL_ORDER * MGL_MAX_EVAL_ORDER * 4];                               \
    int n = getMap(ctx, target, query, d);                                                 \
                                                                                           \
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_ENUM);                                           \
    for (int i = 0; i < n; i++)                                                            \
        v[i] = (T)d[i];                                                                    \
}

GET_MAP(cGetMapdv, GLdouble)
GET_MAP(cGetMapfv, GLfloat)

static void cGetMapiv(GLMContext ctx, GLenum target, GLenum query, GLint *v)
{
    GLdouble d[MGL_MAX_EVAL_ORDER * MGL_MAX_EVAL_ORDER * 4];
    int n = getMap(ctx, target, query, d);

    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_ENUM);
    for (int i = 0; i < n; i++)
        v[i] = (GLint)lround(d[i]);
}

int mglCompatEvalGet(GLMContext ctx, GLenum pname, GLdouble *out)
{
    switch (pname)
    {
        case GL_MAX_EVAL_ORDER: out[0] = MGL_MAX_EVAL_ORDER; return 1;
        case GL_MAP1_GRID_SEGMENTS: out[0] = C->grid1_n; return 1;
        case GL_MAP1_GRID_DOMAIN: out[0] = C->grid1[0]; out[1] = C->grid1[1]; return 2;
        case GL_MAP2_GRID_SEGMENTS: out[0] = C->grid2_n[0]; out[1] = C->grid2_n[1]; return 2;
        case GL_MAP2_GRID_DOMAIN:
            for (int i = 0; i < 4; i++)
                out[i] = C->grid2[i];
            return 4;
    }

    return 0;
}

#define SET(field, fn) ctx->dispatch.field = fn

void mglCompatInstallEval(GLMContext ctx)
{
    for (int i = 0; i < MGL_EVAL_MAPS; i++)
    {
        C->map1[i] = (MGLEvalMap){ .uorder = 1, .vorder = 1, .u1 = 0, .u2 = 1, .v1 = 0, .v2 = 1 };
        C->map2[i] = C->map1[i];
    }

    C->grid1_n = 1;
    C->grid1[1] = 1;
    C->grid2_n[0] = C->grid2_n[1] = 1;
    C->grid2[1] = C->grid2[3] = 1;

    SET(map1f, cMap1f); SET(map1d, cMap1d);
    SET(map2f, cMap2f); SET(map2d, cMap2d);
    SET(map_grid1f, cMapGrid1f); SET(map_grid1d, cMapGrid1d);
    SET(map_grid2f, cMapGrid2f); SET(map_grid2d, cMapGrid2d);
    SET(eval_coord1f, cEvalCoord1f); SET(eval_coord1fv, cEvalCoord1fv);
    SET(eval_coord1d, cEvalCoord1d); SET(eval_coord1dv, cEvalCoord1dv);
    SET(eval_coord2f, cEvalCoord2f); SET(eval_coord2fv, cEvalCoord2fv);
    SET(eval_coord2d, cEvalCoord2d); SET(eval_coord2dv, cEvalCoord2dv);
    SET(eval_point1, cEvalPoint1); SET(eval_point2, cEvalPoint2);
    SET(eval_mesh1, cEvalMesh1); SET(eval_mesh2, cEvalMesh2);
    SET(get_mapdv, cGetMapdv); SET(get_mapfv, cGetMapfv); SET(get_mapiv, cGetMapiv);
}

void mglCompatFreeEval(GLMContext ctx)
{
    for (int i = 0; i < MGL_EVAL_MAPS; i++)
    {
        free(C->map1[i].points);
        free(C->map2[i].points);
    }
}

#endif /* MGL_COMPAT_CONTEXTS */
