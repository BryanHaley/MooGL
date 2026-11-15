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
 * compat_draw.c
 * MGL
 *
 * Drawing in a compatibility context. Every draw passes through here first:
 * arrays in client memory are copied into buffers for the length of the
 * draw, quads and polygons become triangles, the generated fixed-function
 * program stands in when no program is bound, a polygon mode that differs
 * front and back draws twice, and in selection mode nothing is drawn at all.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "glm_context.h"
#include "compat.h"
#include "buffers.h"

#ifdef MGL_COMPAT_CONTEXTS

#define C (ctx->compat)

GLsizei genStrideFromTypeSize(GLenum type, GLint size);
const void *mglBufferCPUData(GLMContext ctx, Buffer *ptr);
void mglCullFace(GLMContext ctx, GLenum mode);

static size_t indexSize(GLenum type)
{
    switch (type)
    {
        case GL_UNSIGNED_BYTE: return 1;
        case GL_UNSIGNED_SHORT: return 2;
        case GL_UNSIGNED_INT: return 4;
    }

    return 0;
}

// The indices a draw reads, from client memory or the element buffer.
static const void *indexData(GLMContext ctx, GLenum type, const void *indices, GLsizei count)
{
    Buffer *eb = STATE(vao) ? STATE(vao)->element_array.buffer : NULL;

    if (eb == NULL)
        return indices;

    const GLubyte *base = (const GLubyte *)mglBufferCPUData(ctx, eb);
    size_t at = (size_t)(uintptr_t)indices;

    if (base == NULL || at + (size_t)count * indexSize(type) > (size_t)eb->size)
        return NULL;

    return base + at;
}

static GLuint indexAt(const void *data, GLenum type, GLsizei i)
{
    switch (type)
    {
        case GL_UNSIGNED_BYTE: return ((const GLubyte *)data)[i];
        case GL_UNSIGNED_SHORT: return ((const GLushort *)data)[i];
        default: return ((const GLuint *)data)[i];
    }
}

// --- reading attributes on the CPU --------------------------------------------

static GLfloat halfToFloat(GLushort h)
{
    GLuint s = (h >> 15) & 1, e = (h >> 10) & 31, m = h & 1023;
    GLfloat v;

    if (e == 0)
        v = ldexpf((GLfloat)m, -24);
    else if (e == 31)
        v = m ? NAN : INFINITY;
    else
        v = ldexpf((GLfloat)(m | 1024), (int)e - 25);

    return s ? -v : v;
}

// one attribute of one vertex as GL would hand it to a vec4
static void readAttrib(const VertexAttrib *a, const GLubyte *src, GLfloat out[4])
{
    out[0] = out[1] = out[2] = 0.0f;
    out[3] = 1.0f;

    for (GLuint i = 0; i < a->size && i < 4; i++)
    {
        GLfloat v;

        switch (a->type)
        {
            case GL_BYTE: { GLbyte x = ((const GLbyte *)src)[i]; v = a->normalized ? fmaxf(x / 127.0f, -1.0f) : x; break; }
            case GL_UNSIGNED_BYTE: { GLubyte x = src[i]; v = a->normalized ? x / 255.0f : x; break; }
            case GL_SHORT: { GLshort x = ((const GLshort *)src)[i]; v = a->normalized ? fmaxf(x / 32767.0f, -1.0f) : x; break; }
            case GL_UNSIGNED_SHORT: { GLushort x = ((const GLushort *)src)[i]; v = a->normalized ? x / 65535.0f : x; break; }
            case GL_INT: { GLint x = ((const GLint *)src)[i]; v = a->normalized ? (GLfloat)fmax(x / 2147483647.0, -1.0) : (GLfloat)x; break; }
            case GL_UNSIGNED_INT: { GLuint x = ((const GLuint *)src)[i]; v = a->normalized ? (GLfloat)(x / 4294967295.0) : (GLfloat)x; break; }
            case GL_HALF_FLOAT: v = halfToFloat(((const GLushort *)src)[i]); break;
            case GL_DOUBLE: v = (GLfloat)((const GLdouble *)src)[i]; break;
            case GL_FIXED: v = ((const GLint *)src)[i] / 65536.0f; break;
            default: v = ((const GLfloat *)src)[i]; break;
        }

        out[i] = v;
    }
}

GLfloat *mglCompatFetchVertices(GLMContext ctx, GLint first, GLsizei count, GLenum type,
                                const void *indices, GLint basevertex, GLsizei *out_count)
{
    VertexArray *vao = STATE(vao);
    const void *idx = NULL;
    const GLubyte *base[MAX_ATTRIBS] = { 0 };
    GLfloat *out;

    *out_count = 0;

    if (count <= 0)
        return NULL;

    if (type)
    {
        idx = indexData(ctx, type, indices, count);
        if (idx == NULL)
            return NULL;
    }

    out = (GLfloat *)malloc((size_t)count * MAX_ATTRIBS * 4 * sizeof(GLfloat));
    if (out == NULL)
        return NULL;

    for (GLuint a = 0; vao && a < MAX_ATTRIBS; a++)
    {
        if (!((vao->enabled_attribs >> a) & 1))
            continue;

        BufferBinding *b = &vao->bindings[vao->attrib[a].buffer_bindingindex];

        if (b->buffer)
        {
            const GLubyte *data = (const GLubyte *)mglBufferCPUData(ctx, b->buffer);
            base[a] = data ? data + b->offset + vao->attrib[a].relativeoffset : NULL;
        }
        else
        {
            base[a] = (const GLubyte *)(uintptr_t)b->offset + vao->attrib[a].relativeoffset;
        }
    }

    for (GLsizei v = 0; v < count; v++)
    {
        GLint vert = type ? (GLint)indexAt(idx, type, v) + basevertex : first + v;
        GLfloat *dst = out + (size_t)v * MAX_ATTRIBS * 4;

        for (GLuint a = 0; a < MAX_ATTRIBS; a++)
        {
            if (base[a])
            {
                BufferBinding *b = &vao->bindings[vao->attrib[a].buffer_bindingindex];
                readAttrib(&vao->attrib[a], base[a] + (size_t)vert * b->stride, dst + a * 4);
            }
            else
            {
                memcpy(dst + a * 4, mglCompatCurrent(ctx, a), 4 * sizeof(GLfloat));
            }
        }
    }

    *out_count = count;
    return out;
}

// --- quads and polygons ----------------------------------------------------------

static bool isLegacyMode(GLenum mode)
{
    return mode == GL_QUADS || mode == GL_QUAD_STRIP || mode == GL_POLYGON;
}

static bool isPolygonMode(GLenum mode)
{
    switch (mode)
    {
        case GL_TRIANGLES: case GL_TRIANGLE_STRIP: case GL_TRIANGLE_FAN:
        case GL_TRIANGLES_ADJACENCY: case GL_TRIANGLE_STRIP_ADJACENCY:
        case GL_QUADS: case GL_QUAD_STRIP: case GL_POLYGON:
            return true;
    }

    return false;
}

// Turns n vertices of a quad mode into triangle indices into the same run.
// Metal makes the first vertex the provoking one, and GL makes it the last of
// a quad and the first of a polygon, so each triangle starts there.
static GLuint *expandLegacy(GLenum mode, GLsizei n, GLsizei *out_n)
{
    GLuint *ix;
    GLsizei k = 0;

    *out_n = 0;

    if (mode == GL_QUADS)
        n -= n % 4;
    else if (mode == GL_QUAD_STRIP)
        n -= n % 2;

    if (n < 3 || (mode != GL_POLYGON && n < 4))
        return NULL;

    ix = (GLuint *)malloc((size_t)n * 3 * sizeof(GLuint));
    if (ix == NULL)
        return NULL;

    if (mode == GL_QUADS)
    {
        for (GLsizei q = 0; q + 3 < n; q += 4)
        {
            GLuint a = q, b = q + 1, c = q + 2, d = q + 3;
            ix[k++] = d; ix[k++] = a; ix[k++] = b;
            ix[k++] = d; ix[k++] = b; ix[k++] = c;
        }
    }
    else if (mode == GL_QUAD_STRIP)
    {
        // quad i is 2i, 2i+1, 2i+3, 2i+2 in drawing order
        for (GLsizei q = 0; q + 3 < n; q += 2)
        {
            GLuint a = q, b = q + 1, c = q + 3, d = q + 2;
            ix[k++] = c; ix[k++] = a; ix[k++] = b;
            ix[k++] = c; ix[k++] = d; ix[k++] = a;
        }
    }
    else
    {
        for (GLsizei i = 1; i + 1 < n; i++)
        {
            ix[k++] = 0;
            ix[k++] = i;
            ix[k++] = i + 1;
        }
    }

    *out_n = k;
    return ix;
}

// --- client arrays ---------------------------------------------------------------

typedef struct {
    GLuint slot;
    Buffer *staged;
    GLintptr offset;
} Staged;

typedef struct {
    Staged s[MAX_BINDABLE_BUFFERS];
    int n;
} StagedSet;

static Buffer *stagingBuffer(GLMContext ctx, GLuint slot)
{
    if (C->staging[slot] == NULL)
        C->staging[slot] = newBuffer(ctx, GL_ARRAY_BUFFER, 0);

    return C->staging[slot];
}

#define MGL_STREAM_SIZE (4 << 20)

#define STREAM_ALIGN 256

// Makes room for bytes in the stream buffer, taking new storage if it is full.
// A draw reserves everything it streams first: new storage part way through
// would leave its earlier copies behind in the old one.
bool mglCompatStreamReserve(GLMContext ctx, size_t bytes)
{
    size_t at = (C->stream_used + STREAM_ALIGN - 1) & ~(size_t)(STREAM_ALIGN - 1);

    if (C->stream == NULL && (C->stream = newBuffer(ctx, GL_ARRAY_BUFFER, 0)) == NULL)
        return false;

    if (C->stream->data.buffer_data == 0 || at + bytes > (size_t)C->stream->size)
    {
        size_t size = bytes > MGL_STREAM_SIZE ? bytes : MGL_STREAM_SIZE;

        if (initBufferData(ctx, C->stream, (GLsizeiptr)size, NULL, false) != 0)
            return false;
        C->stream_used = 0;
    }

    return true;
}

// Room for bytes in the stream buffer. What was written before stays for the
// GPU: a full buffer gets new storage, and the old goes once the GPU is done.
void *mglCompatStream(GLMContext ctx, size_t bytes, Buffer **buf, GLintptr *offset)
{
    if (!mglCompatStreamReserve(ctx, bytes))
        return NULL;

    size_t at = (C->stream_used + STREAM_ALIGN - 1) & ~(size_t)(STREAM_ALIGN - 1);

    C->stream_used = at + bytes;
    C->stream->data.dirty_bits |= DIRTY_BUFFER_DATA;
    *buf = C->stream;
    *offset = (GLintptr)at;
    return (char *)C->stream->data.buffer_data + at;
}

// Bytes of client memory binding b reads for vertices up to hi and the given
// instances, or 0 when it is not a client array any enabled attribute uses.
static size_t clientBindingBytes(VertexArray *vao, GLuint b, GLuint hi, GLsizei instances, GLuint baseinstance)
{
    BufferBinding *bind = &vao->bindings[b];
    GLsizei elem = 0;
    bool used = false;

    if (bind->buffer || bind->offset == 0)
        return 0;

    for (GLuint a = 0; a < MAX_ATTRIBS; a++)
    {
        if (((vao->enabled_attribs >> a) & 1) && vao->attrib[a].buffer_bindingindex == b)
        {
            GLsizei end = (GLsizei)vao->attrib[a].relativeoffset +
                          genStrideFromTypeSize(vao->attrib[a].type, (GLint)vao->attrib[a].size);
            if (end > elem)
                elem = end;
            used = true;
        }
    }

    if (!used)
        return 0;

    GLuint last = bind->divisor ? (baseinstance + (GLuint)instances - 1) / bind->divisor : hi;

    return (size_t)last * (size_t)bind->stride + (size_t)elem;
}

// Copies every array that points into client memory into a buffer, enough of
// it for vertices [lo, hi] and instances [0, instances). Returns false when
// one could not be staged.
static bool stageClientArrays(GLMContext ctx, GLuint lo, GLuint hi, GLsizei instances, GLuint baseinstance,
                              StagedSet *set)
{
    VertexArray *vao = STATE(vao);

    set->n = 0;

    if (vao == NULL)
        return true;

    (void)lo;

    for (GLuint b = 0; b < MAX_BINDABLE_BUFFERS; b++)
    {
        BufferBinding *bind = &vao->bindings[b];
        size_t bytes = clientBindingBytes(vao, b, hi, instances, baseinstance);

        if (bytes == 0)
            continue;

        Buffer *buf = stagingBuffer(ctx, b);

        if (buf == NULL)
            return false;

        GLintptr at = 0;

        // a small copy goes to Metal inline as the draw is encoded, so its
        // storage can be written again at once; a bigger one goes in the stream
        if (bytes < 4096 && buf->data.mtl_data == NULL && buf->data.buffer_data && buf->data.buffer_size >= bytes)
        {
            memcpy((void *)buf->data.buffer_data, (const void *)(uintptr_t)bind->offset, bytes);
            buf->size = (GLsizeiptr)bytes;
            buf->data.version++;
            buf->data.dirty_bits |= DIRTY_BUFFER_DATA;
        }
        else if (bytes < 4096)
        {
            if (initBufferData(ctx, buf, (GLsizeiptr)bytes, (const void *)(uintptr_t)bind->offset, false) != 0)
                return false;
        }
        else
        {
            void *dst = mglCompatStream(ctx, bytes, &buf, &at);

            if (dst == NULL)
                return false;
            memcpy(dst, (const void *)(uintptr_t)bind->offset, bytes);
        }

        set->s[set->n].slot = b;
        set->s[set->n].staged = buf;
        set->s[set->n].offset = bind->offset;
        set->n++;

        bind->buffer = buf;
        bind->offset = at;
    }

    if (set->n)
        vao->dirty_bits |= DIRTY_VAO | DIRTY_VAO_BUFFER_BASE;

    return true;
}

static void unstageClientArrays(GLMContext ctx, StagedSet *set)
{
    VertexArray *vao = STATE(vao);

    for (int i = 0; i < set->n; i++)
    {
        vao->bindings[set->s[i].slot].buffer = NULL;
        vao->bindings[set->s[i].slot].offset = set->s[i].offset;
    }

    if (set->n)
        vao->dirty_bits |= DIRTY_VAO | DIRTY_VAO_BUFFER_BASE;
}

static bool hasClientArrays(GLMContext ctx)
{
    VertexArray *vao = STATE(vao);

    if (vao == NULL)
        return false;

    for (GLuint a = 0; a < MAX_ATTRIBS; a++)
    {
        if (((vao->enabled_attribs >> a) & 1))
        {
            BufferBinding *b = &vao->bindings[vao->attrib[a].buffer_bindingindex];
            if (b->buffer == NULL && b->offset != 0)
                return true;
        }
    }

    return false;
}

// --- the program --------------------------------------------------------------------

typedef struct {
    Program *was;
    bool swapped;
} ProgramSwap;

static bool enterProgram(GLMContext ctx, GLenum mode, ProgramSwap *ps)
{
    ps->was = STATE(program);
    ps->swapped = false;

    if (STATE(program) == NULL && STATE(program_pipeline) == NULL)
    {
        Program *ff = mglCompatFFProgram(ctx, mode);

        if (ff == NULL)
            return false;

        STATE(program) = ff;
        STATE(dirty_bits) |= DIRTY_PROGRAM;
        ps->swapped = true;
    }

    if (STATE(program))
        mglCompatWriteBuiltins(ctx, STATE(program));

    return true;
}

static void leaveProgram(GLMContext ctx, ProgramSwap *ps)
{
    if (ps->swapped)
    {
        STATE(program) = ps->was;
        STATE(dirty_bits) |= DIRTY_PROGRAM;
    }
}

// --- the draw ---------------------------------------------------------------------

static void issue(GLMContext ctx, GLenum mode, GLint first, GLsizei count, GLenum type, const void *indices,
                  GLsizei instances, GLint basevertex, GLuint baseinstance)
{
    if (type == 0)
    {
        if (instances == 1 && baseinstance == 0)
            C->next.draw_arrays(ctx, mode, first, count);
        else
            C->next.draw_arrays_instanced_base_instance(ctx, mode, first, count, instances, baseinstance);
    }
    else
    {
        if (instances == 1 && baseinstance == 0 && basevertex == 0)
            C->next.draw_elements(ctx, mode, count, type, indices);
        else
            C->next.draw_elements_instanced_base_vertex_base_instance(ctx, mode, count, type, indices,
                                                                     instances, basevertex, baseinstance);
    }
}

// One pass of a draw: quads expanded, client arrays staged, the program in place.
static void drawOnce(GLMContext ctx, GLenum mode, GLint first, GLsizei count, GLenum type, const void *indices,
                     GLsizei instances, GLint basevertex, GLuint baseinstance)
{
    GLuint *expanded = NULL;
    GLsizei n = 0;
    GLuint lo = 0, hi = 0;
    StagedSet staged = { .n = 0 };
    ProgramSwap ps;
    Buffer *element_buffer = NULL;
    bool client = hasClientArrays(ctx);
    bool streamed_indices = false;

    if (isLegacyMode(mode))
    {
        expanded = expandLegacy(mode, count, &n);

        if (expanded == NULL)
            return;

        if (type)
        {
            const void *src = indexData(ctx, type, indices, count);

            if (src == NULL)
            {
                free(expanded);
                ctx->error_func(ctx, __FUNCTION__, GL_INVALID_OPERATION);
                return;
            }

            for (GLsizei i = 0; i < n; i++)
                expanded[i] = indexAt(src, type, (GLsizei)expanded[i]);
        }
        else
        {
            for (GLsizei i = 0; i < n; i++)
                expanded[i] += (GLuint)first;
        }

        mode = GL_TRIANGLES;
        type = GL_UNSIGNED_INT;
        indices = expanded;
        count = n;
        first = 0;

        // the new indices are client memory, so the element buffer steps aside
        if (STATE(vao))
        {
            element_buffer = STATE(vao)->element_array.buffer;
            STATE(vao)->element_array.buffer = NULL;
        }
    }

    if (client)
    {
        if (type)
        {
            const void *src = indexData(ctx, type, indices, count);

            if (src == NULL)
                goto done;

            for (GLsizei i = 0; i < count; i++)
            {
                GLuint v = indexAt(src, type, i) + (GLuint)basevertex;
                hi = v > hi ? v : hi;
            }
        }
        else
        {
            lo = (GLuint)first;
            hi = (GLuint)(first + count - 1);
        }

        size_t need = 0;

        for (GLuint b = 0; b < MAX_BINDABLE_BUFFERS; b++)
        {
            size_t bytes = STATE(vao) ? clientBindingBytes(STATE(vao), b, hi, instances, baseinstance) : 0;

            if (bytes >= 4096)
                need += bytes + STREAM_ALIGN;
        }

        if (type && indices && STATE(vao) && STATE(vao)->element_array.buffer == NULL)
            need += (size_t)count * indexSize(type) + STREAM_ALIGN;

        if (need && !mglCompatStreamReserve(ctx, need))
        {
            ctx->error_func(ctx, __FUNCTION__, GL_OUT_OF_MEMORY);
            goto done;
        }

        if (!stageClientArrays(ctx, lo, hi, instances, baseinstance, &staged))
        {
            ctx->error_func(ctx, __FUNCTION__, GL_OUT_OF_MEMORY);
            goto done;
        }
    }

    // indices in client memory go in the stream too
    if (type && indices && STATE(vao) && STATE(vao)->element_array.buffer == NULL)
    {
        size_t bytes = (size_t)count * indexSize(type);
        Buffer *eb;
        GLintptr at;
        void *dst = mglCompatStream(ctx, bytes, &eb, &at);

        if (dst)
        {
            memcpy(dst, indices, bytes);
            STATE(vao)->element_array.buffer = eb;
            indices = (const void *)at;
            streamed_indices = true;
        }
    }

    if (!enterProgram(ctx, mode, &ps))
        goto done;

    issue(ctx, mode, first, count, type, indices, instances, basevertex, baseinstance);

    leaveProgram(ctx, &ps);

done:
    if (streamed_indices)
        STATE(vao)->element_array.buffer = NULL;

    unstageClientArrays(ctx, &staged);

    if (element_buffer)
        STATE(vao)->element_array.buffer = element_buffer;

    free(expanded);
}

void mglCompatDrawFull(GLMContext ctx, GLenum mode, GLint first, GLsizei count, GLenum type,
                       const void *indices, GLsizei instances, GLint basevertex, GLuint baseinstance)
{
    if (C->in_begin)
    {
        ctx->error_func(ctx, __FUNCTION__, GL_INVALID_OPERATION);
        return;
    }

    if (count <= 0 || instances <= 0)
    {
        if (count < 0 || instances < 0)
            ctx->error_func(ctx, __FUNCTION__, GL_INVALID_VALUE);
        return;
    }

    // a list keeps the vertices as they are now, not the arrays
    if (mglCompatRecording(ctx))
    {
        GLsizei n;
        GLfloat *verts = mglCompatFetchVertices(ctx, first, count, type, indices, basevertex, &n);

        if (verts)
        {
            mglCompatRecordRecords(ctx, mode, verts, n);
            free(verts);
        }

        if (!mglCompatExecuting(ctx))
            return;
    }

    if (C->render_mode != GL_RENDER)
    {
        GLsizei n;
        GLfloat *verts;

        if (C->render_mode == GL_FEEDBACK)
            return;

        verts = mglCompatFetchVertices(ctx, first, count, type, indices, basevertex, &n);
        if (verts)
        {
            mglCompatSelectVertices(ctx, mode, verts, n);
            free(verts);
        }
        return;
    }

    if (C->polygon_mode[0] != C->polygon_mode[1] && isPolygonMode(mode))
    {
        GLboolean culling = STATE(caps.cull_face);
        GLenum cull = STATE_VAR(cull_face_mode);
        bool front = !culling || cull == GL_BACK;
        bool back = !culling || cull == GL_FRONT;

        // each face in its own mode, the other face culled away
        STATE(caps.cull_face) = GL_TRUE;
        STATE(dirty_bits) |= DIRTY_RENDER_STATE;

        if (front)
        {
            C->next.polygon_mode(ctx, GL_FRONT_AND_BACK, C->polygon_mode[0]);
            mglCullFace(ctx, GL_BACK);
            drawOnce(ctx, mode, first, count, type, indices, instances, basevertex, baseinstance);
        }

        if (back)
        {
            C->next.polygon_mode(ctx, GL_FRONT_AND_BACK, C->polygon_mode[1]);
            mglCullFace(ctx, GL_FRONT);
            drawOnce(ctx, mode, first, count, type, indices, instances, basevertex, baseinstance);
        }

        C->next.polygon_mode(ctx, GL_FRONT_AND_BACK, C->polygon_mode[0]);
        mglCullFace(ctx, cull);
        STATE(caps.cull_face) = culling;
        STATE(dirty_bits) |= DIRTY_RENDER_STATE;
        return;
    }

    drawOnce(ctx, mode, first, count, type, indices, instances, basevertex, baseinstance);
}

void mglCompatDraw(GLMContext ctx, GLenum mode, GLint first, GLsizei count, GLenum type,
                   const void *indices, GLsizei instances, GLint basevertex)
{
    mglCompatDrawFull(ctx, mode, first, count, type, indices, instances, basevertex, 0);
}

// --- the entry points ---------------------------------------------------------------

static void cDrawArrays(GLMContext ctx, GLenum mode, GLint first, GLsizei count)
{
    mglCompatDrawFull(ctx, mode, first, count, 0, NULL, 1, 0, 0);
}

static void cDrawElements(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices)
{
    mglCompatDrawFull(ctx, mode, 0, count, type, indices, 1, 0, 0);
}

static void cDrawRangeElements(GLMContext ctx, GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type,
                               const void *indices)
{
    ERROR_CHECK_RETURN(end >= start, GL_INVALID_VALUE);
    mglCompatDrawFull(ctx, mode, 0, count, type, indices, 1, 0, 0);
}

static void cDrawArraysInstanced(GLMContext ctx, GLenum mode, GLint first, GLsizei count, GLsizei instances)
{
    mglCompatDrawFull(ctx, mode, first, count, 0, NULL, instances, 0, 0);
}

static void cDrawElementsInstanced(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices,
                                   GLsizei instances)
{
    mglCompatDrawFull(ctx, mode, 0, count, type, indices, instances, 0, 0);
}

static void cDrawElementsBaseVertex(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices,
                                    GLint basevertex)
{
    mglCompatDrawFull(ctx, mode, 0, count, type, indices, 1, basevertex, 0);
}

static void cDrawRangeElementsBaseVertex(GLMContext ctx, GLenum mode, GLuint start, GLuint end, GLsizei count,
                                         GLenum type, const void *indices, GLint basevertex)
{
    ERROR_CHECK_RETURN(end >= start, GL_INVALID_VALUE);
    mglCompatDrawFull(ctx, mode, 0, count, type, indices, 1, basevertex, 0);
}

static void cDrawElementsInstancedBaseVertex(GLMContext ctx, GLenum mode, GLsizei count, GLenum type,
                                             const void *indices, GLsizei instances, GLint basevertex)
{
    mglCompatDrawFull(ctx, mode, 0, count, type, indices, instances, basevertex, 0);
}

static void cDrawArraysInstancedBaseInstance(GLMContext ctx, GLenum mode, GLint first, GLsizei count,
                                             GLsizei instances, GLuint baseinstance)
{
    mglCompatDrawFull(ctx, mode, first, count, 0, NULL, instances, 0, baseinstance);
}

static void cDrawElementsInstancedBaseInstance(GLMContext ctx, GLenum mode, GLsizei count, GLenum type,
                                               const void *indices, GLsizei instances, GLuint baseinstance)
{
    mglCompatDrawFull(ctx, mode, 0, count, type, indices, instances, 0, baseinstance);
}

static void cDrawElementsInstancedBaseVertexBaseInstance(GLMContext ctx, GLenum mode, GLsizei count, GLenum type,
                                                         const void *indices, GLsizei instances, GLint basevertex,
                                                         GLuint baseinstance)
{
    mglCompatDrawFull(ctx, mode, 0, count, type, indices, instances, basevertex, baseinstance);
}

static void cMultiDrawArrays(GLMContext ctx, GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount)
{
    ERROR_CHECK_RETURN(drawcount >= 0, GL_INVALID_VALUE);

    for (GLsizei i = 0; i < drawcount; i++)
        mglCompatDrawFull(ctx, mode, first[i], count[i], 0, NULL, 1, 0, 0);
}

static void cMultiDrawElements(GLMContext ctx, GLenum mode, const GLsizei *count, GLenum type,
                               const void *const *indices, GLsizei drawcount)
{
    ERROR_CHECK_RETURN(drawcount >= 0, GL_INVALID_VALUE);

    for (GLsizei i = 0; i < drawcount; i++)
        mglCompatDrawFull(ctx, mode, 0, count[i], type, indices[i], 1, 0, 0);
}

static void cMultiDrawElementsBaseVertex(GLMContext ctx, GLenum mode, const GLsizei *count, GLenum type,
                                         const void *const *indices, GLsizei drawcount, const GLint *basevertex)
{
    ERROR_CHECK_RETURN(drawcount >= 0, GL_INVALID_VALUE);

    for (GLsizei i = 0; i < drawcount; i++)
        mglCompatDrawFull(ctx, mode, 0, count[i], type, indices[i], 1, basevertex[i], 0);
}

// draws whose vertex counts live on the GPU only need the program
#define PROGRAM_ONLY(fn, field, params, args)                       \
static void fn params                                               \
{                                                                   \
    ProgramSwap ps;                                                 \
    if (C->render_mode != GL_RENDER)                                \
        return;                                                     \
    if (!enterProgram(ctx, mode, &ps))                              \
        return;                                                     \
    C->next.field args;                                             \
    leaveProgram(ctx, &ps);                                         \
}

PROGRAM_ONLY(cDrawArraysIndirect, draw_arrays_indirect, (GLMContext ctx, GLenum mode, const void *indirect),
             (ctx, mode, indirect))
PROGRAM_ONLY(cDrawElementsIndirect, draw_elements_indirect,
             (GLMContext ctx, GLenum mode, GLenum type, const void *indirect), (ctx, mode, type, indirect))
PROGRAM_ONLY(cMultiDrawArraysIndirect, multi_draw_arrays_indirect,
             (GLMContext ctx, GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride),
             (ctx, mode, indirect, drawcount, stride))
PROGRAM_ONLY(cMultiDrawElementsIndirect, multi_draw_elements_indirect,
             (GLMContext ctx, GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride),
             (ctx, mode, type, indirect, drawcount, stride))
PROGRAM_ONLY(cDrawTransformFeedback, draw_transform_feedback, (GLMContext ctx, GLenum mode, GLuint id),
             (ctx, mode, id))
PROGRAM_ONLY(cDrawTransformFeedbackStream, draw_transform_feedback_stream,
             (GLMContext ctx, GLenum mode, GLuint id, GLuint stream), (ctx, mode, id, stream))
PROGRAM_ONLY(cDrawTransformFeedbackInstanced, draw_transform_feedback_instanced,
             (GLMContext ctx, GLenum mode, GLuint id, GLsizei instances), (ctx, mode, id, instances))
PROGRAM_ONLY(cDrawTransformFeedbackStreamInstanced, draw_transform_feedback_stream_instanced,
             (GLMContext ctx, GLenum mode, GLuint id, GLuint stream, GLsizei instances),
             (ctx, mode, id, stream, instances))

#define SET(field, fn) ctx->dispatch.field = fn

void mglCompatInstallDraw(GLMContext ctx)
{
    SET(draw_arrays, cDrawArrays);
    SET(draw_elements, cDrawElements);
    SET(draw_range_elements, cDrawRangeElements);
    SET(draw_arrays_instanced, cDrawArraysInstanced);
    SET(draw_elements_instanced, cDrawElementsInstanced);
    SET(draw_elements_base_vertex, cDrawElementsBaseVertex);
    SET(draw_range_elements_base_vertex, cDrawRangeElementsBaseVertex);
    SET(draw_elements_instanced_base_vertex, cDrawElementsInstancedBaseVertex);
    SET(draw_arrays_instanced_base_instance, cDrawArraysInstancedBaseInstance);
    SET(draw_elements_instanced_base_instance, cDrawElementsInstancedBaseInstance);
    SET(draw_elements_instanced_base_vertex_base_instance, cDrawElementsInstancedBaseVertexBaseInstance);
    SET(multi_draw_arrays, cMultiDrawArrays);
    SET(multi_draw_elements, cMultiDrawElements);
    SET(multi_draw_elements_base_vertex, cMultiDrawElementsBaseVertex);
    SET(draw_arrays_indirect, cDrawArraysIndirect);
    SET(draw_elements_indirect, cDrawElementsIndirect);
    SET(multi_draw_arrays_indirect, cMultiDrawArraysIndirect);
    SET(multi_draw_elements_indirect, cMultiDrawElementsIndirect);
    SET(draw_transform_feedback, cDrawTransformFeedback);
    SET(draw_transform_feedback_stream, cDrawTransformFeedbackStream);
    SET(draw_transform_feedback_instanced, cDrawTransformFeedbackInstanced);
    SET(draw_transform_feedback_stream_instanced, cDrawTransformFeedbackStreamInstanced);
}

#endif /* MGL_COMPAT_CONTEXTS */
