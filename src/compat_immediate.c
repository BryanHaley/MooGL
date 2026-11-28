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
 * compat_immediate.c
 * MGL
 *
 * glBegin and glEnd. Each glVertex copies every current value into a
 * growing array, sixteen vec4s a vertex, and glEnd draws that array through
 * a vertex array object of its own, the same way client arrays are drawn.
 */

#include <stdlib.h>
#include <string.h>

#include "glm_context.h"
#include "compat.h"
#include "buffers.h"

#ifdef MGL_COMPAT_CONTEXTS

#define C (ctx->compat)

#define IMM_STRIDE (MAX_ATTRIBS * 4)

void mglCompatDrawFull(GLMContext ctx, GLenum mode, GLint first, GLsizei count, GLenum type,
                       const void *indices, GLsizei instances, GLint basevertex, GLuint baseinstance);

static bool beginMode(GLenum mode)
{
    switch (mode)
    {
        case GL_POINTS: case GL_LINES: case GL_LINE_LOOP: case GL_LINE_STRIP:
        case GL_TRIANGLES: case GL_TRIANGLE_STRIP: case GL_TRIANGLE_FAN:
        case GL_QUADS: case GL_QUAD_STRIP: case GL_POLYGON:
        case GL_LINES_ADJACENCY: case GL_LINE_STRIP_ADJACENCY:
        case GL_TRIANGLES_ADJACENCY: case GL_TRIANGLE_STRIP_ADJACENCY:
        case GL_PATCHES:
            return true;
    }

    return false;
}

static void cBegin(GLMContext ctx, GLenum mode)
{
    ERROR_CHECK_RETURN(beginMode(mode), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);

    C->in_begin = GL_TRUE;
    C->begin_mode = mode;
    C->imm_count = 0;
}

void mglCompatEmitVertex(GLMContext ctx, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    GLfloat *v;

    if (mglCompatRecording(ctx))
    {
        mglCompatRecordVertex(ctx, x, y, z, w);
        if (!mglCompatExecuting(ctx))
            return;
    }

    if (!C->in_begin)
        return;

    if (C->imm_count == C->imm_cap)
    {
        size_t cap = C->imm_cap ? C->imm_cap * 2 : 256;
        GLfloat *grown = (GLfloat *)realloc(C->imm, cap * IMM_STRIDE * sizeof(GLfloat));
        GLboolean *edges = grown ? (GLboolean *)realloc(C->imm_edges, cap) : NULL;

        if (grown)
            C->imm = grown;
        if (edges == NULL)
            return;

        C->imm_edges = edges;
        C->imm_cap = cap;
    }

    v = C->imm + C->imm_count * IMM_STRIDE;

    for (int a = 0; a < MAX_ATTRIBS; a++)
        memcpy(v + a * 4, STATE(attrib_constant[a].v.f), 4 * sizeof(GLfloat));

    v[0] = x;
    v[1] = y;
    v[2] = z;
    v[3] = w;

    C->imm_edges[C->imm_count] = C->edge_flag;
    C->imm_count++;
}

// The order GL makes triangles of a quad, quad strip, polygon or fan in, as
// vertex numbers. Each triangle starts at GL's provoking vertex, since Metal
// takes the first. Returns how many, or 0 when mode is not one of them.
static GLsizei triangleOrder(GLenum mode, GLsizei n, GLuint *out)
{
    GLsizei k = 0;

    switch (mode)
    {
        case GL_QUADS:
            for (GLsizei q = 0; q + 3 < n; q += 4)
            {
                out[k++] = q + 3; out[k++] = q; out[k++] = q + 1;
                out[k++] = q + 3; out[k++] = q + 1; out[k++] = q + 2;
            }
            return k;

        case GL_QUAD_STRIP:
            for (GLsizei q = 0; q + 3 < n; q += 2)
            {
                out[k++] = q + 3; out[k++] = q; out[k++] = q + 1;
                out[k++] = q + 3; out[k++] = q + 2; out[k++] = q;
            }
            return k;

        case GL_POLYGON:
        case GL_TRIANGLE_FAN:
            for (GLsizei i = 1; i + 1 < n; i++)
            {
                out[k++] = mode == GL_POLYGON ? 0 : i + 1;
                out[k++] = mode == GL_POLYGON ? i : 0;
                out[k++] = mode == GL_POLYGON ? i + 1 : i;
            }
            return k;
    }

    return 0;
}

// Sets up imm_vao to read the attributes in mask, packed in that order.
static void immLayout(GLMContext ctx, GLuint mask)
{
    VertexArray *vao = C->imm_vao;
    GLuint k = 0;

    for (GLuint a = 0; a < MAX_ATTRIBS; a++)
    {
        vao->attrib[a].size = 4;
        vao->attrib[a].type = GL_FLOAT;
        vao->attrib[a].normalized = GL_FALSE;
        vao->attrib[a].is_integer = GL_FALSE;
        vao->attrib[a].is_long = GL_FALSE;
        vao->attrib[a].buffer_bindingindex = 0;
        vao->attrib[a].pointer = 0;
        vao->attrib[a].relativeoffset = (mask >> a) & 1 ? k++ * 4 * sizeof(GLfloat) : 0;
        vao->bindings[a].buffer = NULL;
        vao->bindings[a].offset = 0;
        vao->bindings[a].divisor = 0;
    }

    for (GLuint a = 0; a < MAX_ATTRIBS; a++)
        vao->attrib[a].stride = k * 4 * sizeof(GLfloat);

    vao->bindings[0].buffer = C->imm_buf;
    vao->bindings[0].stride = k * 4 * sizeof(GLfloat);
    vao->enabled_attribs = mask;
    vao->element_array.buffer = NULL;
    vao->dirty_bits |= DIRTY_VAO_ATTRIB | DIRTY_VAO_BUFFER_BASE;
    C->imm_mask = mask;
}

// draws count records of 16 vec4 through the immediate-mode vertex array
// edges, if not NULL, is each vertex's edge flag
void mglCompatDrawRecords(GLMContext ctx, GLenum mode, const GLfloat *records, const GLboolean *edges,
                          GLsizei count)
{
    VertexArray *was = STATE(vao);
    GLuint *order = NULL;
    GLsizei n = count;
    GLuint mask = 1;

    if (count <= 0 || C->imm_vao == NULL)
        return;

    if (C->imm_buf == NULL && (C->imm_buf = newBuffer(ctx, GL_ARRAY_BUFFER, 0)) == NULL)
        return;

    GLenum source = mode;
    GLboolean *drawn_edges = NULL;

    // the shapes Metal has no primitive for become a plain triangle list
    if (mode == GL_QUADS || mode == GL_QUAD_STRIP || mode == GL_POLYGON || mode == GL_TRIANGLE_FAN)
    {
        order = (GLuint *)malloc((size_t)count * 6 * sizeof(GLuint));
        n = order ? triangleOrder(mode, count, order) : 0;

        if (n == 0)
        {
            free(order);
            return;
        }

        mode = GL_TRIANGLES;
    }

    // an attribute that is the current value on every vertex is read from
    // there, which keeps most vertices a few vec4s
    for (GLuint a = 1; a < MAX_ATTRIBS; a++)
    {
        if ((C->imm_mask >> a) & 1)
            continue;

        const AttribConstant *cur = &STATE(attrib_constant[a]);

        if (cur->type != _ATTRIB_CONST_FLOAT)
        {
            mask |= 1u << a;
            continue;
        }

        for (GLsizei v = 0; v < count; v++)
        {
            if (memcmp(records + (size_t)v * IMM_STRIDE + a * 4, cur->v.f, 4 * sizeof(GLfloat)))
            {
                mask |= 1u << a;
                break;
            }
        }
    }

    // what varied once stays in, so the layout settles
    mask |= C->imm_mask;

    GLuint per = (GLuint)__builtin_popcount(mask);
    size_t bytes = (size_t)n * per * 4 * sizeof(GLfloat);
    Buffer *buf = C->imm_buf;

    GLintptr at = 0;
    GLfloat *dst;

    // a small one goes to Metal inline as the draw is encoded, so the same
    // storage can be written again at once; a bigger one goes in the stream
    if (bytes >= 4096)
        dst = (GLfloat *)mglCompatStream(ctx, bytes, &buf, &at);
    else if (buf->data.mtl_data == NULL && buf->data.buffer_data && buf->data.buffer_size >= bytes)
        dst = (GLfloat *)buf->data.buffer_data;
    else
        dst = initBufferData(ctx, buf, (GLsizeiptr)bytes, NULL, false) == 0 ? (GLfloat *)buf->data.buffer_data : NULL;

    if (dst == NULL)
    {
        free(order);
        ctx->error_func(ctx, __FUNCTION__, GL_OUT_OF_MEMORY);
        return;
    }

    // a flag only marks an edge of separate triangles, quads and polygons
    if (edges && (source == GL_TRIANGLES || source == GL_QUADS || source == GL_POLYGON) &&
        memchr(edges, GL_FALSE, (size_t)count) && (drawn_edges = (GLboolean *)malloc((size_t)n)))
    {
        for (GLsizei i = 0; i < n; i++)
            drawn_edges[i] = edges[order ? order[i] : (GLuint)i];
    }

    for (GLsizei i = 0; i < n; i++)
    {
        const GLfloat *src = records + (size_t)(order ? order[i] : (GLuint)i) * IMM_STRIDE;

        for (GLuint a = 0; a < MAX_ATTRIBS; a++)
        {
            if ((mask >> a) & 1)
            {
                memcpy(dst, src + a * 4, 4 * sizeof(GLfloat));
                dst += 4;
            }
        }
    }

    free(order);

    if (buf == C->imm_buf)
    {
        buf->size = (GLsizeiptr)bytes;
        buf->data.version++;
        buf->data.dirty_bits |= DIRTY_BUFFER_DATA;
    }

    if (mask != C->imm_mask)
        immLayout(ctx, mask);

    if (C->imm_vao->bindings[0].buffer != buf || C->imm_vao->bindings[0].offset != at)
    {
        C->imm_vao->bindings[0].buffer = buf;
        C->imm_vao->bindings[0].offset = at;
        C->imm_vao->dirty_bits |= DIRTY_VAO_BUFFER_BASE;
    }

    STATE(vao) = C->imm_vao;
    STATE(dirty_bits) |= DIRTY_VAO;

    // glEnd itself was what a list recorded
    C->no_record++;
    C->source_mode = source;
    C->source_edges = drawn_edges;
    mglCompatDrawFull(ctx, mode, 0, n, 0, NULL, 1, 0, 0);
    C->source_mode = 0;
    C->source_edges = NULL;
    C->no_record--;
    free(drawn_edges);

    STATE(vao) = was;
    STATE(dirty_bits) |= DIRTY_VAO;
}

static void cEnd(GLMContext ctx)
{
    ERROR_CHECK_RETURN(C->in_begin, GL_INVALID_OPERATION);

    C->in_begin = GL_FALSE;
    mglCompatDrawRecords(ctx, C->begin_mode, C->imm, C->imm_edges, (GLsizei)C->imm_count);
    C->imm_count = 0;
}

#define VERTEX_FAMILY(sfx, T)                                                                              \
static void cVertex2##sfx(GLMContext ctx, T x, T y) { mglCompatEmitVertex(ctx, (GLfloat)x, (GLfloat)y, 0, 1); } \
static void cVertex2##sfx##v(GLMContext ctx, const T *v) { mglCompatEmitVertex(ctx, (GLfloat)v[0], (GLfloat)v[1], 0, 1); } \
static void cVertex3##sfx(GLMContext ctx, T x, T y, T z)                                                   \
{ mglCompatEmitVertex(ctx, (GLfloat)x, (GLfloat)y, (GLfloat)z, 1); }                                       \
static void cVertex3##sfx##v(GLMContext ctx, const T *v)                                                   \
{ mglCompatEmitVertex(ctx, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], 1); }                              \
static void cVertex4##sfx(GLMContext ctx, T x, T y, T z, T w)                                              \
{ mglCompatEmitVertex(ctx, (GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w); }                              \
static void cVertex4##sfx##v(GLMContext ctx, const T *v)                                                   \
{ mglCompatEmitVertex(ctx, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]); }

VERTEX_FAMILY(s, GLshort)
VERTEX_FAMILY(i, GLint)
VERTEX_FAMILY(f, GLfloat)
VERTEX_FAMILY(d, GLdouble)

static void rect(GLMContext ctx, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
    ERROR_CHECK_RETURN(!C->in_begin || mglCompatRecording(ctx), GL_INVALID_OPERATION);

    // through the table, so a list being compiled records it
    ctx->dispatch.begin(ctx, GL_POLYGON);
    mglCompatEmitVertex(ctx, x1, y1, 0, 1);
    mglCompatEmitVertex(ctx, x2, y1, 0, 1);
    mglCompatEmitVertex(ctx, x2, y2, 0, 1);
    mglCompatEmitVertex(ctx, x1, y2, 0, 1);
    ctx->dispatch.end(ctx);
}

static void cRectd(GLMContext ctx, GLdouble a, GLdouble b, GLdouble c, GLdouble d) { rect(ctx, (GLfloat)a, (GLfloat)b, (GLfloat)c, (GLfloat)d); }
static void cRectf(GLMContext ctx, GLfloat a, GLfloat b, GLfloat c, GLfloat d) { rect(ctx, a, b, c, d); }
static void cRecti(GLMContext ctx, GLint a, GLint b, GLint c, GLint d) { rect(ctx, (GLfloat)a, (GLfloat)b, (GLfloat)c, (GLfloat)d); }
static void cRects(GLMContext ctx, GLshort a, GLshort b, GLshort c, GLshort d) { rect(ctx, a, b, c, d); }
static void cRectdv(GLMContext ctx, const GLdouble *a, const GLdouble *b) { rect(ctx, (GLfloat)a[0], (GLfloat)a[1], (GLfloat)b[0], (GLfloat)b[1]); }
static void cRectfv(GLMContext ctx, const GLfloat *a, const GLfloat *b) { rect(ctx, a[0], a[1], b[0], b[1]); }
static void cRectiv(GLMContext ctx, const GLint *a, const GLint *b) { rect(ctx, (GLfloat)a[0], (GLfloat)a[1], (GLfloat)b[0], (GLfloat)b[1]); }
static void cRectsv(GLMContext ctx, const GLshort *a, const GLshort *b) { rect(ctx, a[0], a[1], b[0], b[1]); }

// each enabled array's element i becomes the current value, and the vertex
// array's makes a vertex
static void cArrayElement(GLMContext ctx, GLint i)
{
    GLsizei n;
    GLfloat *v;
    VertexArray *vao = STATE(vao);

    ERROR_CHECK_RETURN(i >= 0, GL_INVALID_VALUE);

    v = mglCompatFetchVertices(ctx, i, 1, 0, NULL, 0, &n);
    if (v == NULL)
        return;

    for (GLuint a = 1; a < MAX_ATTRIBS; a++)
        if (vao && ((vao->enabled_attribs >> a) & 1))
            mglCompatSetCurrent(ctx, a, v[a * 4], v[a * 4 + 1], v[a * 4 + 2], v[a * 4 + 3]);

    if (vao && (vao->enabled_attribs & 1))
        mglCompatEmitVertex(ctx, v[0], v[1], v[2], v[3]);

    free(v);
}

// attribute zero inside glBegin is a vertex
static void cVertexAttrib1f(GLMContext ctx, GLuint i, GLfloat x)
{
    if (i == 0 && C->in_begin) mglCompatEmitVertex(ctx, x, 0, 0, 1);
    else C->next.vertex_attrib1f(ctx, i, x);
}

static void cVertexAttrib2f(GLMContext ctx, GLuint i, GLfloat x, GLfloat y)
{
    if (i == 0 && C->in_begin) mglCompatEmitVertex(ctx, x, y, 0, 1);
    else C->next.vertex_attrib2f(ctx, i, x, y);
}

static void cVertexAttrib3f(GLMContext ctx, GLuint i, GLfloat x, GLfloat y, GLfloat z)
{
    if (i == 0 && C->in_begin) mglCompatEmitVertex(ctx, x, y, z, 1);
    else C->next.vertex_attrib3f(ctx, i, x, y, z);
}

static void cVertexAttrib4f(GLMContext ctx, GLuint i, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    if (i == 0 && C->in_begin) mglCompatEmitVertex(ctx, x, y, z, w);
    else C->next.vertex_attrib4f(ctx, i, x, y, z, w);
}

static void cVertexAttrib2fv(GLMContext ctx, GLuint i, const GLfloat *v)
{
    if (i == 0 && C->in_begin) mglCompatEmitVertex(ctx, v[0], v[1], 0, 1);
    else C->next.vertex_attrib2fv(ctx, i, v);
}

static void cVertexAttrib3fv(GLMContext ctx, GLuint i, const GLfloat *v)
{
    if (i == 0 && C->in_begin) mglCompatEmitVertex(ctx, v[0], v[1], v[2], 1);
    else C->next.vertex_attrib3fv(ctx, i, v);
}

static void cVertexAttrib4fv(GLMContext ctx, GLuint i, const GLfloat *v)
{
    if (i == 0 && C->in_begin) mglCompatEmitVertex(ctx, v[0], v[1], v[2], v[3]);
    else C->next.vertex_attrib4fv(ctx, i, v);
}

#define SET(field, fn) ctx->dispatch.field = fn

void mglCompatInstallImmediate(GLMContext ctx)
{
    SET(begin, cBegin);
    SET(end, cEnd);

#define VERTEX_SET(sfx) \
    SET(vertex2##sfx, cVertex2##sfx); SET(vertex2##sfx##v, cVertex2##sfx##v); \
    SET(vertex3##sfx, cVertex3##sfx); SET(vertex3##sfx##v, cVertex3##sfx##v); \
    SET(vertex4##sfx, cVertex4##sfx); SET(vertex4##sfx##v, cVertex4##sfx##v)

    VERTEX_SET(s); VERTEX_SET(i); VERTEX_SET(f); VERTEX_SET(d);

    SET(rectd, cRectd); SET(rectdv, cRectdv);
    SET(rectf, cRectf); SET(rectfv, cRectfv);
    SET(recti, cRecti); SET(rectiv, cRectiv);
    SET(rects, cRects); SET(rectsv, cRectsv);
    SET(array_element, cArrayElement);

    SET(vertex_attrib1f, cVertexAttrib1f);
    SET(vertex_attrib2f, cVertexAttrib2f);
    SET(vertex_attrib3f, cVertexAttrib3f);
    SET(vertex_attrib4f, cVertexAttrib4f);
    SET(vertex_attrib2fv, cVertexAttrib2fv);
    SET(vertex_attrib3fv, cVertexAttrib3fv);
    SET(vertex_attrib4fv, cVertexAttrib4fv);
}

#endif /* MGL_COMPAT_CONTEXTS */
