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
 * compat_lists.c
 * MGL
 *
 * Display lists record GL calls, not Metal work, so a list that reads state
 * set at replay time behaves as GL says. Between glNewList and glEndList the
 * listable entries in the dispatch table are swapped for recorders; each
 * keeps the entry's place in the table and its arguments, and playback calls
 * the entry again. Calls with many variants (glColor*, glVertex*, glRasterPos*,
 * the array draws, glBitmap) are recorded where they all end up instead.
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "glm_context.h"
#include "compat.h"
#include "hash_table.h"

#ifdef MGL_COMPAT_CONTEXTS

#define C (ctx->compat)

// --- the list table, one per share group ----------------------------------------

typedef struct {
    HashTable names;
    GLuint next;
} ListTable;

typedef struct {
    uint16_t sig;
    uint16_t count;
    uint32_t offset;     // the entry's place in the dispatch table
    union {
        GLuint u[16];
        GLfloat f[16];
        GLdouble d[8];
    } a;
    void *blob;
} Cmd;

struct MGLDisplayList_t {
    Cmd *cmds;
    size_t count, cap;
};

enum {
    SIG_V, SIG_U, SIG_UU, SIG_UUU, SIG_UUUU, SIG_UUUUU,
    SIG_F, SIG_FF, SIG_FFF, SIG_FFFF, SIG_D, SIG_DDD, SIG_DDDD, SIG_DDDDDD,
    SIG_UF, SIG_UUF, SIG_UFV, SIG_UUFV, SIG_UIV, SIG_UUIV, SIG_UDV, SIG_FV, SIG_DV,
    // the sinks
    SIG_CURRENT, SIG_VERTEX, SIG_RECORDS, SIG_RASTER, SIG_WINDOW, SIG_BITMAP, SIG_PIXELS, SIG_CALL_LISTS, SIG_EVAL,
};

static ListTable *listTable(GLMContext ctx)
{
    MGLShareGroup *g = ctx->state.share;

    if (g->compat_lists == NULL)
    {
        ListTable *t = (ListTable *)calloc(1, sizeof(ListTable));

        if (t == NULL)
            return NULL;

        initHashTable(&t->names, 32);
        t->next = 1;
        g->compat_lists = t;
    }

    return (ListTable *)g->compat_lists;
}

static void freeList(MGLDisplayList *l)
{
    if (l == NULL)
        return;

    for (size_t i = 0; i < l->count; i++)
        free(l->cmds[i].blob);

    free(l->cmds);
    free(l);
}

void mglCompatFreeListTable(void *table)
{
    ListTable *t = (ListTable *)table;

    if (t == NULL)
        return;

    for (size_t k = 0; k < t->names.size; k++)
        if (t->names.keys[k].data)
            freeList((MGLDisplayList *)t->names.keys[k].data);

    free(t->names.keys);
    free(t->names.free_names);
    free(t);
}

static MGLDisplayList *findList(GLMContext ctx, GLuint name)
{
    ListTable *t = listTable(ctx);

    return t && name ? (MGLDisplayList *)searchHashTable(&t->names, name) : NULL;
}

// --- recording -------------------------------------------------------------------

bool mglCompatRecording(GLMContext ctx)
{
    return C->compiling && C->no_record == 0;
}

bool mglCompatExecuting(GLMContext ctx)
{
    return !mglCompatRecording(ctx) || C->list_mode == GL_COMPILE_AND_EXECUTE;
}

static Cmd *rec(GLMContext ctx, int sig, size_t offset)
{
    MGLDisplayList *l = C->compiling;

    if (l->count == l->cap)
    {
        size_t cap = l->cap ? l->cap * 2 : 64;
        Cmd *grown = (Cmd *)realloc(l->cmds, cap * sizeof(Cmd));

        if (grown == NULL)
            return NULL;

        l->cmds = grown;
        l->cap = cap;
    }

    Cmd *c = &l->cmds[l->count++];

    memset(c, 0, sizeof(*c));
    c->sig = (uint16_t)sig;
    c->offset = (uint32_t)offset;
    return c;
}

static bool exec(GLMContext ctx)
{
    return C->list_mode == GL_COMPILE_AND_EXECUTE;
}

#define LIVE(field) (C->list_live.field)
#define OFF(field) offsetof(struct GLMDispatchTable, field)

#define R_V(field)                                                                          \
static void r_##field(GLMContext ctx)                                                       \
{ if (!rec(ctx, SIG_V, OFF(field))) return; if (exec(ctx)) LIVE(field)(ctx); }

#define R_U(field, T)                                                                       \
static void r_##field(GLMContext ctx, T a)                                                  \
{ Cmd *c = rec(ctx, SIG_U, OFF(field)); if (!c) return; c->a.u[0] = (GLuint)a;             \
  if (exec(ctx)) LIVE(field)(ctx, a); }

#define R_UU(field, T1, T2)                                                                 \
static void r_##field(GLMContext ctx, T1 a, T2 b)                                           \
{ Cmd *c = rec(ctx, SIG_UU, OFF(field)); if (!c) return; c->a.u[0] = (GLuint)a; c->a.u[1] = (GLuint)b; \
  if (exec(ctx)) LIVE(field)(ctx, a, b); }

#define R_UUU(field, T1, T2, T3)                                                            \
static void r_##field(GLMContext ctx, T1 a, T2 b, T3 d)                                     \
{ Cmd *c = rec(ctx, SIG_UUU, OFF(field)); if (!c) return;                                  \
  c->a.u[0] = (GLuint)a; c->a.u[1] = (GLuint)b; c->a.u[2] = (GLuint)d;                    \
  if (exec(ctx)) LIVE(field)(ctx, a, b, d); }

#define R_UUUU(field, T1, T2, T3, T4)                                                       \
static void r_##field(GLMContext ctx, T1 a, T2 b, T3 d, T4 e)                               \
{ Cmd *c = rec(ctx, SIG_UUUU, OFF(field)); if (!c) return;                                 \
  c->a.u[0] = (GLuint)a; c->a.u[1] = (GLuint)b; c->a.u[2] = (GLuint)d; c->a.u[3] = (GLuint)e; \
  if (exec(ctx)) LIVE(field)(ctx, a, b, d, e); }

#define R_F(field)                                                                          \
static void r_##field(GLMContext ctx, GLfloat a)                                            \
{ Cmd *c = rec(ctx, SIG_F, OFF(field)); if (!c) return; c->a.f[0] = a;                     \
  if (exec(ctx)) LIVE(field)(ctx, a); }

#define R_FF(field)                                                                         \
static void r_##field(GLMContext ctx, GLfloat a, GLfloat b)                                 \
{ Cmd *c = rec(ctx, SIG_FF, OFF(field)); if (!c) return; c->a.f[0] = a; c->a.f[1] = b;     \
  if (exec(ctx)) LIVE(field)(ctx, a, b); }

#define R_FFF(field)                                                                        \
static void r_##field(GLMContext ctx, GLfloat a, GLfloat b, GLfloat d)                      \
{ Cmd *c = rec(ctx, SIG_FFF, OFF(field)); if (!c) return;                                  \
  c->a.f[0] = a; c->a.f[1] = b; c->a.f[2] = d; if (exec(ctx)) LIVE(field)(ctx, a, b, d); }

#define R_FFFF(field)                                                                       \
static void r_##field(GLMContext ctx, GLfloat a, GLfloat b, GLfloat d, GLfloat e)           \
{ Cmd *c = rec(ctx, SIG_FFFF, OFF(field)); if (!c) return;                                 \
  c->a.f[0] = a; c->a.f[1] = b; c->a.f[2] = d; c->a.f[3] = e;                             \
  if (exec(ctx)) LIVE(field)(ctx, a, b, d, e); }

#define R_D(field)                                                                          \
static void r_##field(GLMContext ctx, GLdouble a)                                           \
{ Cmd *c = rec(ctx, SIG_D, OFF(field)); if (!c) return; c->a.d[0] = a;                     \
  if (exec(ctx)) LIVE(field)(ctx, a); }

#define R_DDD(field)                                                                        \
static void r_##field(GLMContext ctx, GLdouble a, GLdouble b, GLdouble d)                   \
{ Cmd *c = rec(ctx, SIG_DDD, OFF(field)); if (!c) return;                                  \
  c->a.d[0] = a; c->a.d[1] = b; c->a.d[2] = d; if (exec(ctx)) LIVE(field)(ctx, a, b, d); }

#define R_DDDD(field)                                                                       \
static void r_##field(GLMContext ctx, GLdouble a, GLdouble b, GLdouble d, GLdouble e)       \
{ Cmd *c = rec(ctx, SIG_DDDD, OFF(field)); if (!c) return;                                 \
  c->a.d[0] = a; c->a.d[1] = b; c->a.d[2] = d; c->a.d[3] = e;                             \
  if (exec(ctx)) LIVE(field)(ctx, a, b, d, e); }

#define R_D6(field)                                                                         \
static void r_##field(GLMContext ctx, GLdouble a, GLdouble b, GLdouble d, GLdouble e,       \
                      GLdouble g, GLdouble h)                                               \
{ Cmd *c = rec(ctx, SIG_DDDDDD, OFF(field)); if (!c) return;                               \
  c->a.d[0] = a; c->a.d[1] = b; c->a.d[2] = d; c->a.d[3] = e; c->a.d[4] = g; c->a.d[5] = h; \
  if (exec(ctx)) LIVE(field)(ctx, a, b, d, e, g, h); }

#define R_UF(field)                                                                         \
static void r_##field(GLMContext ctx, GLenum a, GLfloat b)                                  \
{ Cmd *c = rec(ctx, SIG_UF, OFF(field)); if (!c) return; c->a.u[0] = a; c->a.f[1] = b;     \
  if (exec(ctx)) LIVE(field)(ctx, a, b); }

#define R_UUF(field)                                                                        \
static void r_##field(GLMContext ctx, GLenum a, GLenum b, GLfloat d)                        \
{ Cmd *c = rec(ctx, SIG_UUF, OFF(field)); if (!c) return;                                  \
  c->a.u[0] = a; c->a.u[1] = b; c->a.f[2] = d; if (exec(ctx)) LIVE(field)(ctx, a, b, d); }

// how many values a vector parameter carries
static int vecCount(GLenum pname)
{
    switch (pname)
    {
        case GL_AMBIENT: case GL_DIFFUSE: case GL_SPECULAR: case GL_POSITION: case GL_EMISSION:
        case GL_AMBIENT_AND_DIFFUSE: case GL_TEXTURE_ENV_COLOR: case GL_OBJECT_PLANE: case GL_EYE_PLANE:
        case GL_FOG_COLOR: case GL_LIGHT_MODEL_AMBIENT: case GL_TEXTURE_BORDER_COLOR:
            return 4;
        case GL_SPOT_DIRECTION: case GL_COLOR_INDEXES:
            return 3;
    }

    return 1;
}

#define R_UUFV(field, T, SIG)                                                               \
static void r_##field(GLMContext ctx, GLenum a, GLenum b, const T *v)                       \
{ Cmd *c = rec(ctx, SIG, OFF(field)); if (!c) return; int n = vecCount(b);                 \
  c->a.u[0] = a; c->a.u[1] = b; memcpy(&c->a.u[2], v, (size_t)n * sizeof(T));              \
  if (exec(ctx)) LIVE(field)(ctx, a, b, v); }

#define R_UFV(field, T, SIG)                                                                \
static void r_##field(GLMContext ctx, GLenum a, const T *v)                                 \
{ Cmd *c = rec(ctx, SIG, OFF(field)); if (!c) return; int n = vecCount(a);                 \
  c->a.u[0] = a; memcpy(&c->a.u[1], v, (size_t)n * sizeof(T));                             \
  if (exec(ctx)) LIVE(field)(ctx, a, v); }

#define R_FV(field)                                                                         \
static void r_##field(GLMContext ctx, const GLfloat *m)                                     \
{ Cmd *c = rec(ctx, SIG_FV, OFF(field)); if (!c) return; memcpy(c->a.f, m, 16 * sizeof(GLfloat)); \
  if (exec(ctx)) LIVE(field)(ctx, m); }

#define R_DV(field)                                                                         \
static void r_##field(GLMContext ctx, const GLdouble *m)                                    \
{ Cmd *c = rec(ctx, SIG_DV, OFF(field)); if (!c) return;                                   \
  c->blob = malloc(16 * sizeof(GLdouble)); if (c->blob) memcpy(c->blob, m, 16 * sizeof(GLdouble)); \
  if (exec(ctx)) LIVE(field)(ctx, m); }

R_V(end) R_V(push_matrix) R_V(pop_matrix) R_V(load_identity) R_V(pop_attrib) R_V(init_names) R_V(pop_name)
R_U(begin, GLenum) R_U(matrix_mode, GLenum) R_U(enable, GLenum) R_U(disable, GLenum) R_U(shade_model, GLenum)
R_U(cull_face, GLenum) R_U(front_face, GLenum) R_U(depth_func, GLenum) R_U(push_attrib, GLbitfield)
R_U(active_texture, GLenum) R_U(list_base, GLuint) R_U(load_name, GLuint) R_U(push_name, GLuint)
R_U(clear, GLbitfield) R_U(depth_mask, GLboolean) R_U(edge_flag, GLboolean) R_U(use_program, GLuint)
R_U(stencil_mask, GLuint) R_U(blend_equation, GLenum) R_U(clear_stencil, GLint) R_U(logic_op, GLenum)
R_U(draw_buffer, GLenum) R_U(read_buffer, GLenum)
R_UU(bind_texture, GLenum, GLuint) R_UU(blend_func, GLenum, GLenum) R_UU(polygon_mode, GLenum, GLenum)
R_UU(hint, GLenum, GLenum) R_UU(color_material, GLenum, GLenum) R_UU(line_stipple, GLint, GLushort)
R_UU(light_modeli, GLenum, GLint) R_UU(fogi, GLenum, GLint) R_UU(blend_equation_separate, GLenum, GLenum)
R_UUU(lighti, GLenum, GLenum, GLint) R_UUU(materiali, GLenum, GLenum, GLint) R_UUU(tex_envi, GLenum, GLenum, GLint)
R_UUU(tex_geni, GLenum, GLenum, GLint) R_UUU(tex_parameteri, GLenum, GLenum, GLint)
R_UUU(stencil_func, GLenum, GLint, GLuint) R_UUU(stencil_op, GLenum, GLenum, GLenum)
R_UUUU(color_mask, GLboolean, GLboolean, GLboolean, GLboolean)
R_UUUU(blend_func_separate, GLenum, GLenum, GLenum, GLenum)
R_UUUU(viewport, GLint, GLint, GLsizei, GLsizei) R_UUUU(scissor, GLint, GLint, GLsizei, GLsizei)
R_UF(alpha_func) R_UF(fogf) R_UF(light_modelf)
R_UUF(lightf) R_UUF(materialf) R_UUF(tex_envf) R_UUF(tex_genf) R_UUF(tex_parameterf)
R_F(line_width) R_F(point_size)
R_FF(polygon_offset) R_FF(pixel_zoom)
R_FFF(translatef) R_FFF(scalef)
R_FFFF(rotatef) R_FFFF(clear_color) R_FFFF(blend_color)
R_D(clear_depth)
R_DDD(translated) R_DDD(scaled)
R_DDDD(rotated)
R_D6(ortho) R_D6(frustum)
R_UUFV(lightfv, GLfloat, SIG_UUFV) R_UUFV(materialfv, GLfloat, SIG_UUFV) R_UUFV(tex_envfv, GLfloat, SIG_UUFV)
R_UUFV(tex_genfv, GLfloat, SIG_UUFV)
R_UUFV(lightiv, GLint, SIG_UUIV) R_UUFV(materialiv, GLint, SIG_UUIV) R_UUFV(tex_enviv, GLint, SIG_UUIV)
R_UUFV(tex_geniv, GLint, SIG_UUIV)
R_UFV(light_modelfv, GLfloat, SIG_UFV) R_UFV(fogfv, GLfloat, SIG_UFV)
R_UFV(light_modeliv, GLint, SIG_UIV) R_UFV(fogiv, GLint, SIG_UIV)
R_FV(load_matrixf) R_FV(mult_matrixf) R_FV(load_transpose_matrixf) R_FV(mult_transpose_matrixf)
R_DV(load_matrixd) R_DV(mult_matrixd) R_DV(load_transpose_matrixd) R_DV(mult_transpose_matrixd)

static void r_clip_plane(GLMContext ctx, GLenum plane, const GLdouble *eq)
{
    Cmd *c = rec(ctx, SIG_UDV, OFF(clip_plane));

    if (!c)
        return;

    c->a.u[0] = plane;
    c->blob = malloc(4 * sizeof(GLdouble));
    if (c->blob)
        memcpy(c->blob, eq, 4 * sizeof(GLdouble));

    if (exec(ctx))
        LIVE(clip_plane)(ctx, plane, eq);
}

static void r_copy_pixels(GLMContext ctx, GLint x, GLint y, GLsizei w, GLsizei h, GLenum type)
{
    Cmd *c = rec(ctx, SIG_UUUUU, OFF(copy_pixels));

    if (!c)
        return;

    c->a.u[0] = (GLuint)x; c->a.u[1] = (GLuint)y; c->a.u[2] = (GLuint)w; c->a.u[3] = (GLuint)h; c->a.u[4] = type;

    if (exec(ctx))
        LIVE(copy_pixels)(ctx, x, y, w, h, type);
}

static void cCallList(GLMContext ctx, GLuint list);

static void r_call_list(GLMContext ctx, GLuint list)
{
    Cmd *c = rec(ctx, SIG_U, OFF(call_list));

    if (!c)
        return;

    c->a.u[0] = list;

    if (exec(ctx))
        cCallList(ctx, list);
}

static GLuint listNameAt(GLenum type, const void *lists, GLsizei i, bool *ok)
{
    *ok = true;

    switch (type)
    {
        case GL_BYTE: return (GLuint)(GLint)((const GLbyte *)lists)[i];
        case GL_UNSIGNED_BYTE: return ((const GLubyte *)lists)[i];
        case GL_SHORT: return (GLuint)(GLint)((const GLshort *)lists)[i];
        case GL_UNSIGNED_SHORT: return ((const GLushort *)lists)[i];
        case GL_INT: return (GLuint)((const GLint *)lists)[i];
        case GL_UNSIGNED_INT: return ((const GLuint *)lists)[i];
        case GL_FLOAT: return (GLuint)((const GLfloat *)lists)[i];
        case GL_2_BYTES: { const GLubyte *b = (const GLubyte *)lists + 2 * i; return b[0] * 256u + b[1]; }
        case GL_3_BYTES: { const GLubyte *b = (const GLubyte *)lists + 3 * i; return (b[0] * 256u + b[1]) * 256u + b[2]; }
        case GL_4_BYTES:
        {
            const GLubyte *b = (const GLubyte *)lists + 4 * i;
            return ((b[0] * 256u + b[1]) * 256u + b[2]) * 256u + b[3];
        }
    }

    *ok = false;
    return 0;
}

static void cCallLists(GLMContext ctx, GLsizei n, GLenum type, const void *lists);

static void r_call_lists(GLMContext ctx, GLsizei n, GLenum type, const void *lists)
{
    Cmd *c;
    GLuint *names;
    bool ok = true;

    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    names = (GLuint *)malloc((size_t)(n ? n : 1) * sizeof(GLuint));
    ERROR_CHECK_RETURN(names, GL_OUT_OF_MEMORY);

    for (GLsizei i = 0; i < n && ok; i++)
        names[i] = listNameAt(type, lists, i, &ok);

    if (!ok)
    {
        free(names);
        ERROR_RETURN(GL_INVALID_ENUM);
    }

    c = rec(ctx, SIG_CALL_LISTS, 0);
    if (!c)
    {
        free(names);
        return;
    }

    c->a.u[0] = (GLuint)n;
    c->blob = names;

    if (exec(ctx))
        cCallLists(ctx, n, GL_UNSIGNED_INT, names);
}

// --- the sinks the other compat files call -------------------------------------

void mglCompatRecordCurrent(GLMContext ctx, GLuint slot, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    Cmd *c = rec(ctx, SIG_CURRENT, 0);

    if (c)
    {
        c->a.u[0] = slot;
        c->a.f[1] = x; c->a.f[2] = y; c->a.f[3] = z; c->a.f[4] = w;
    }
}

void mglCompatRecordVertex(GLMContext ctx, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    Cmd *c = rec(ctx, SIG_VERTEX, 0);

    if (c)
    {
        c->a.f[0] = x; c->a.f[1] = y; c->a.f[2] = z; c->a.f[3] = w;
    }
}

// takes blob, which may be NULL
void mglCompatRecordEval(GLMContext ctx, const GLuint *args, int n, void *blob)
{
    Cmd *c = rec(ctx, SIG_EVAL, 0);

    if (c == NULL)
    {
        free(blob);
        return;
    }

    memcpy(c->a.u, args, (size_t)n * sizeof(GLuint));
    c->blob = blob;
}

void mglCompatRecordRecords(GLMContext ctx, GLenum mode, const GLfloat *records, GLsizei count)
{
    size_t bytes = (size_t)count * MAX_ATTRIBS * 4 * sizeof(GLfloat);
    Cmd *c = rec(ctx, SIG_RECORDS, 0);

    if (c == NULL)
        return;

    c->a.u[0] = mode;
    c->a.u[1] = (GLuint)count;
    c->blob = malloc(bytes ? bytes : 1);
    if (c->blob)
        memcpy(c->blob, records, bytes);
}

void mglCompatRecordRasterPos(GLMContext ctx, GLboolean window, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    Cmd *c = rec(ctx, window ? SIG_WINDOW : SIG_RASTER, 0);

    if (c)
    {
        c->a.f[0] = x; c->a.f[1] = y; c->a.f[2] = z; c->a.f[3] = w;
    }
}

void mglCompatRecordBitmap(GLMContext ctx, GLsizei w, GLsizei h, GLfloat xo, GLfloat yo, GLfloat xm, GLfloat ym,
                           const GLubyte *packed)
{
    size_t bytes = (size_t)((w + 7) / 8) * (size_t)h;
    Cmd *c = rec(ctx, SIG_BITMAP, 0);

    if (c == NULL)
        return;

    c->a.u[0] = (GLuint)w; c->a.u[1] = (GLuint)h;
    c->a.f[2] = xo; c->a.f[3] = yo; c->a.f[4] = xm; c->a.f[5] = ym;
    c->blob = malloc(bytes ? bytes : 1);
    if (c->blob && packed)
        memcpy(c->blob, packed, bytes);
}

void mglCompatRecordDrawPixels(GLMContext ctx, GLsizei w, GLsizei h, GLenum format, GLenum type,
                               const void *packed, size_t bytes)
{
    Cmd *c = rec(ctx, SIG_PIXELS, 0);

    if (c == NULL)
        return;

    c->a.u[0] = (GLuint)w; c->a.u[1] = (GLuint)h; c->a.u[2] = format; c->a.u[3] = type;
    c->blob = malloc(bytes ? bytes : 1);
    if (c->blob)
        memcpy(c->blob, packed, bytes);
}

// --- playback --------------------------------------------------------------------

#define FN(T) ((T)(*(void **)((char *)table + c->offset)))

static void replay(GLMContext ctx, MGLDisplayList *l)
{
    struct GLMDispatchTable *table = C->compiling ? &C->list_live : &ctx->dispatch;

    C->no_record++;

    for (size_t i = 0; i < l->count; i++)
    {
        Cmd *c = &l->cmds[i];

        switch (c->sig)
        {
            case SIG_V: FN(void (*)(GLMContext))(ctx); break;
            case SIG_U: FN(void (*)(GLMContext, GLuint))(ctx, c->a.u[0]); break;
            case SIG_UU: FN(void (*)(GLMContext, GLuint, GLuint))(ctx, c->a.u[0], c->a.u[1]); break;
            case SIG_UUU: FN(void (*)(GLMContext, GLuint, GLuint, GLuint))(ctx, c->a.u[0], c->a.u[1], c->a.u[2]); break;
            case SIG_UUUU:
                FN(void (*)(GLMContext, GLuint, GLuint, GLuint, GLuint))(ctx, c->a.u[0], c->a.u[1], c->a.u[2], c->a.u[3]);
                break;
            case SIG_UUUUU:
                FN(void (*)(GLMContext, GLuint, GLuint, GLuint, GLuint, GLuint))(ctx, c->a.u[0], c->a.u[1], c->a.u[2],
                                                                                 c->a.u[3], c->a.u[4]);
                break;
            case SIG_F: FN(void (*)(GLMContext, GLfloat))(ctx, c->a.f[0]); break;
            case SIG_FF: FN(void (*)(GLMContext, GLfloat, GLfloat))(ctx, c->a.f[0], c->a.f[1]); break;
            case SIG_FFF: FN(void (*)(GLMContext, GLfloat, GLfloat, GLfloat))(ctx, c->a.f[0], c->a.f[1], c->a.f[2]); break;
            case SIG_FFFF:
                FN(void (*)(GLMContext, GLfloat, GLfloat, GLfloat, GLfloat))(ctx, c->a.f[0], c->a.f[1], c->a.f[2], c->a.f[3]);
                break;
            case SIG_D: FN(void (*)(GLMContext, GLdouble))(ctx, c->a.d[0]); break;
            case SIG_DDD: FN(void (*)(GLMContext, GLdouble, GLdouble, GLdouble))(ctx, c->a.d[0], c->a.d[1], c->a.d[2]); break;
            case SIG_DDDD:
                FN(void (*)(GLMContext, GLdouble, GLdouble, GLdouble, GLdouble))(ctx, c->a.d[0], c->a.d[1], c->a.d[2],
                                                                                 c->a.d[3]);
                break;
            case SIG_DDDDDD:
                FN(void (*)(GLMContext, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble))(
                    ctx, c->a.d[0], c->a.d[1], c->a.d[2], c->a.d[3], c->a.d[4], c->a.d[5]);
                break;
            case SIG_UF: FN(void (*)(GLMContext, GLuint, GLfloat))(ctx, c->a.u[0], c->a.f[1]); break;
            case SIG_UUF: FN(void (*)(GLMContext, GLuint, GLuint, GLfloat))(ctx, c->a.u[0], c->a.u[1], c->a.f[2]); break;
            case SIG_UUFV: FN(void (*)(GLMContext, GLuint, GLuint, const GLfloat *))(ctx, c->a.u[0], c->a.u[1], &c->a.f[2]); break;
            case SIG_UUIV:
                FN(void (*)(GLMContext, GLuint, GLuint, const GLint *))(ctx, c->a.u[0], c->a.u[1], (const GLint *)&c->a.u[2]);
                break;
            case SIG_UFV: FN(void (*)(GLMContext, GLuint, const GLfloat *))(ctx, c->a.u[0], &c->a.f[1]); break;
            case SIG_UIV: FN(void (*)(GLMContext, GLuint, const GLint *))(ctx, c->a.u[0], (const GLint *)&c->a.u[1]); break;
            case SIG_UDV:
                if (c->blob)
                    FN(void (*)(GLMContext, GLuint, const GLdouble *))(ctx, c->a.u[0], (const GLdouble *)c->blob);
                break;
            case SIG_FV: FN(void (*)(GLMContext, const GLfloat *))(ctx, c->a.f); break;
            case SIG_DV:
                if (c->blob)
                    FN(void (*)(GLMContext, const GLdouble *))(ctx, (const GLdouble *)c->blob);
                break;
            case SIG_CURRENT:
                mglCompatSetCurrent(ctx, c->a.u[0], c->a.f[1], c->a.f[2], c->a.f[3], c->a.f[4]);
                break;
            case SIG_VERTEX:
                mglCompatEmitVertex(ctx, c->a.f[0], c->a.f[1], c->a.f[2], c->a.f[3]);
                break;
            case SIG_RECORDS:
                if (c->blob)
                    mglCompatDrawRecords(ctx, c->a.u[0], (const GLfloat *)c->blob, NULL, (GLsizei)c->a.u[1]);
                break;
            case SIG_RASTER:
            case SIG_WINDOW:
                mglCompatRasterPos(ctx, c->sig == SIG_WINDOW, c->a.f[0], c->a.f[1], c->a.f[2], c->a.f[3]);
                break;
            case SIG_BITMAP:
                mglCompatBitmapPacked(ctx, (GLsizei)c->a.u[0], (GLsizei)c->a.u[1], c->a.f[2], c->a.f[3], c->a.f[4],
                                      c->a.f[5], (const GLubyte *)c->blob);
                break;
            case SIG_PIXELS:
                mglCompatDrawPixelsPacked(ctx, (GLsizei)c->a.u[0], (GLsizei)c->a.u[1], c->a.u[2], c->a.u[3], c->blob);
                break;
            case SIG_CALL_LISTS:
                cCallLists(ctx, (GLsizei)c->a.u[0], GL_UNSIGNED_INT, c->blob);
                break;
            case SIG_EVAL:
                mglCompatEvalReplay(ctx, c->a.u, c->blob);
                break;
        }
    }

    C->no_record--;
}

static void cCallList(GLMContext ctx, GLuint list)
{
    MGLDisplayList *l = findList(ctx, list);

    if (l == NULL || C->list_nesting >= MGL_LIST_NESTING)
        return;

    C->list_nesting++;
    replay(ctx, l);
    C->list_nesting--;
}

static void cCallLists(GLMContext ctx, GLsizei n, GLenum type, const void *lists)
{
    bool ok = true;

    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    for (GLsizei i = 0; i < n; i++)
    {
        GLuint name = listNameAt(type, lists, i, &ok);

        ERROR_CHECK_RETURN(ok, GL_INVALID_ENUM);
        cCallList(ctx, C->list_base + name);
    }
}

// --- the list commands themselves --------------------------------------------------

#define SWAP(field) ctx->dispatch.field = r_##field

static void installRecorders(GLMContext ctx)
{
    SWAP(end); SWAP(push_matrix); SWAP(pop_matrix); SWAP(load_identity); SWAP(pop_attrib); SWAP(init_names);
    SWAP(pop_name); SWAP(begin); SWAP(matrix_mode); SWAP(enable); SWAP(disable); SWAP(shade_model);
    SWAP(cull_face); SWAP(front_face); SWAP(depth_func); SWAP(push_attrib); SWAP(active_texture);
    SWAP(list_base); SWAP(load_name); SWAP(push_name); SWAP(clear); SWAP(depth_mask); SWAP(edge_flag);
    SWAP(use_program); SWAP(stencil_mask); SWAP(blend_equation); SWAP(clear_stencil); SWAP(logic_op);
    SWAP(draw_buffer); SWAP(read_buffer);
    SWAP(bind_texture); SWAP(blend_func); SWAP(polygon_mode); SWAP(hint); SWAP(color_material);
    SWAP(line_stipple); SWAP(light_modeli); SWAP(fogi); SWAP(blend_equation_separate);
    SWAP(lighti); SWAP(materiali); SWAP(tex_envi); SWAP(tex_geni); SWAP(tex_parameteri); SWAP(stencil_func);
    SWAP(stencil_op); SWAP(color_mask); SWAP(blend_func_separate); SWAP(viewport); SWAP(scissor);
    SWAP(alpha_func); SWAP(fogf); SWAP(light_modelf); SWAP(lightf); SWAP(materialf); SWAP(tex_envf);
    SWAP(tex_genf); SWAP(tex_parameterf); SWAP(line_width); SWAP(point_size); SWAP(polygon_offset);
    SWAP(pixel_zoom); SWAP(translatef); SWAP(scalef); SWAP(rotatef); SWAP(clear_color); SWAP(blend_color);
    SWAP(clear_depth); SWAP(translated); SWAP(scaled); SWAP(rotated); SWAP(ortho); SWAP(frustum);
    SWAP(lightfv); SWAP(materialfv); SWAP(tex_envfv); SWAP(tex_genfv); SWAP(lightiv); SWAP(materialiv);
    SWAP(tex_enviv); SWAP(tex_geniv); SWAP(light_modelfv); SWAP(fogfv); SWAP(light_modeliv); SWAP(fogiv);
    SWAP(load_matrixf); SWAP(mult_matrixf); SWAP(load_transpose_matrixf); SWAP(mult_transpose_matrixf);
    SWAP(load_matrixd); SWAP(mult_matrixd); SWAP(load_transpose_matrixd); SWAP(mult_transpose_matrixd);
    SWAP(clip_plane); SWAP(copy_pixels); SWAP(call_list); SWAP(call_lists);
}

static GLuint cGenLists(GLMContext ctx, GLsizei range)
{
    ListTable *t = listTable(ctx);
    GLuint base;

    ERROR_CHECK_RETURN_VALUE(range >= 0, GL_INVALID_VALUE, 0);
    ERROR_CHECK_RETURN_VALUE(!C->in_begin, GL_INVALID_OPERATION, 0);

    if (range == 0 || t == NULL)
        return 0;

    pthread_mutex_lock(&ctx->state.share->lock);

    // the next run of free names
    for (base = t->next;; base++)
    {
        GLsizei i;

        for (i = 0; i < range; i++)
            if (searchHashTable(&t->names, base + (GLuint)i))
                break;

        if (i == range)
            break;

        base += (GLuint)i;
    }

    for (GLsizei i = 0; i < range; i++)
        insertHashElement(&t->names, base + (GLuint)i, calloc(1, sizeof(MGLDisplayList)));

    t->next = base + (GLuint)range;

    pthread_mutex_unlock(&ctx->state.share->lock);

    return base;
}

static void cDeleteLists(GLMContext ctx, GLuint list, GLsizei range)
{
    ListTable *t = listTable(ctx);

    ERROR_CHECK_RETURN(range >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);

    if (t == NULL)
        return;

    pthread_mutex_lock(&ctx->state.share->lock);

    for (GLsizei i = 0; i < range; i++)
    {
        MGLDisplayList *l = (MGLDisplayList *)searchHashTable(&t->names, list + (GLuint)i);

        // the one being compiled is replaced at glEndList
        if (l && l != C->compiling)
        {
            deleteHashElement(&t->names, list + (GLuint)i);
            freeList(l);
        }
    }

    pthread_mutex_unlock(&ctx->state.share->lock);
}

static GLboolean cIsList(GLMContext ctx, GLuint list)
{
    ERROR_CHECK_RETURN_VALUE(!C->in_begin, GL_INVALID_OPERATION, GL_FALSE);

    return findList(ctx, list) != NULL;
}

static void cNewList(GLMContext ctx, GLuint list, GLenum mode)
{
    ERROR_CHECK_RETURN(list != 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(mode == GL_COMPILE || mode == GL_COMPILE_AND_EXECUTE, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(!C->compiling && !C->in_begin, GL_INVALID_OPERATION);

    C->compiling = (MGLDisplayList *)calloc(1, sizeof(MGLDisplayList));
    ERROR_CHECK_RETURN(C->compiling, GL_OUT_OF_MEMORY);

    C->compiling_name = list;
    C->list_mode = mode;
    C->list_live = ctx->dispatch;
    installRecorders(ctx);
}

static void cEndList(GLMContext ctx)
{
    ListTable *t = listTable(ctx);
    MGLDisplayList *old;

    ERROR_CHECK_RETURN(C->compiling, GL_INVALID_OPERATION);

    // the table keeps its glNewList and glEndList, which are not recorders
    ctx->dispatch = C->list_live;

    if (t == NULL)
    {
        freeList(C->compiling);
        C->compiling = NULL;
        return;
    }

    pthread_mutex_lock(&ctx->state.share->lock);

    old = (MGLDisplayList *)searchHashTable(&t->names, C->compiling_name);
    if (old)
    {
        deleteHashElement(&t->names, C->compiling_name);
        freeList(old);
    }

    insertHashElement(&t->names, C->compiling_name, C->compiling);
    if (C->compiling_name >= t->next)
        t->next = C->compiling_name + 1;

    pthread_mutex_unlock(&ctx->state.share->lock);

    C->compiling = NULL;
    C->compiling_name = 0;
}

static void cListBase(GLMContext ctx, GLuint base)
{
    C->list_base = base;
}

#define SET(field, fn) ctx->dispatch.field = fn

void mglCompatInstallLists(GLMContext ctx)
{
    SET(gen_lists, cGenLists);
    SET(delete_lists, cDeleteLists);
    SET(is_list, cIsList);
    SET(new_list, cNewList);
    SET(end_list, cEndList);
    SET(call_list, cCallList);
    SET(call_lists, cCallLists);
    SET(list_base, cListBase);
}

#endif /* MGL_COMPAT_CONTEXTS */
