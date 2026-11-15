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
 * compat_raster.c
 * MGL
 *
 * The raster position and what writes at it: glDrawPixels, glBitmap and
 * glCopyPixels. Each puts its pixels in a texture of its own and draws a
 * rectangle over the window area they cover, reading texels by position, so
 * the fragments still meet the depth test, blending and the rest.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "glm_context.h"
#include "compat.h"
#include "mgl_log.h"

#ifdef MGL_COMPAT_CONTEXTS

#define C (ctx->compat)

extern Program *findProgram(GLMContext ctx, GLuint program);
GLuint mglCreateProgram(GLMContext ctx);
void mglDeleteProgram(GLMContext ctx, GLuint program);
void mglLinkProgram(GLMContext ctx, GLuint program);
void mglAttachShader(GLMContext ctx, GLuint program, GLuint shader);
GLuint mglCreateShader(GLMContext ctx, GLenum type);
void mglDeleteShader(GLMContext ctx, GLuint shader);
void mglShaderSource(GLMContext ctx, GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length);
void mglCompileShader(GLMContext ctx, GLuint shader);
GLint mglGetUniformLocation(GLMContext ctx, GLuint program, const GLchar *name);
void mglProgramUniform1i(GLMContext ctx, GLuint program, GLint location, GLint v0);
void mglProgramUniform1f(GLMContext ctx, GLuint program, GLint location, GLfloat v0);
void mglProgramUniform4fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLfloat *value);
void mglGenTextures(GLMContext ctx, GLsizei n, GLuint *textures);
void mglDeleteTextures(GLMContext ctx, GLsizei n, const GLuint *textures);
void mglBindTexture(GLMContext ctx, GLenum target, GLuint texture);
void mglTexImage2D(GLMContext ctx, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                   GLint border, GLenum format, GLenum type, const void *pixels);
void mglTexParameteri(GLMContext ctx, GLenum target, GLenum pname, GLint param);
void mglGetTexImage(GLMContext ctx, GLenum target, GLint level, GLenum format, GLenum type, void *pixels);
void mglCopyTexImage2D(GLMContext ctx, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
                       GLsizei width, GLsizei height, GLint border);
void mglCompatDrawFull(GLMContext ctx, GLenum mode, GLint first, GLsizei count, GLenum type,
                       const void *indices, GLsizei instances, GLint basevertex, GLuint baseinstance);
const void *mglBufferCPUData(GLMContext ctx, Buffer *ptr);

// --- the raster position ------------------------------------------------------------

void mglCompatRasterPos(GLMContext ctx, GLboolean window, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    const GLfloat *cur = mglCompatCurrent(ctx, MGL_ATTR_COLOR);
    const GLfloat *sec = mglCompatCurrent(ctx, MGL_ATTR_SECONDARY);
    const GLfloat *tc = mglCompatCurrent(ctx, MGL_ATTR_TEXCOORD0);
    GLdouble n = STATE(depth_range[0].znear), f = STATE(depth_range[0].zfar);

    if (mglCompatRecording(ctx))
    {
        mglCompatRecordRasterPos(ctx, window, x, y, z, w);
        if (!mglCompatExecuting(ctx))
            return;
    }

    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);

    memcpy(C->raster_color, cur, sizeof(C->raster_color));
    memcpy(C->raster_secondary, sec, sizeof(C->raster_secondary));
    mglMatTransform(mglCompatTextureMatrix(ctx, 0), tc, C->raster_tex);

    if (window)
    {
        C->raster_pos[0] = x;
        C->raster_pos[1] = y;
        C->raster_pos[2] = (GLfloat)(n + fmin(fmax(z, 0.0), 1.0) * (f - n));
        C->raster_pos[3] = 1.0f;
        C->raster_distance = 0.0f;
        C->raster_valid = GL_TRUE;
    }
    else
    {
        GLfloat obj[4] = { x, y, z, w }, eye[4], clip[4];
        ViewportRect *vp = &STATE(viewport[0]);

        mglMatTransform(mglCompatModelview(ctx), obj, eye);
        mglMatTransform(mglCompatProjection(ctx), eye, clip);

        C->raster_valid = fabsf(clip[0]) <= clip[3] && fabsf(clip[1]) <= clip[3] && fabsf(clip[2]) <= clip[3] &&
                          clip[3] > 0.0f;

        for (int i = 0; C->raster_valid && i < MGL_COMPAT_PLANES; i++)
            if (STATE(caps.clip_distances[i]))
                C->raster_valid = eye[0] * C->clip_planes[i][0] + eye[1] * C->clip_planes[i][1] +
                                  eye[2] * C->clip_planes[i][2] + eye[3] * C->clip_planes[i][3] >= 0.0f;

        if (C->raster_valid)
        {
            C->raster_pos[0] = vp->x + (clip[0] / clip[3] + 1.0f) * vp->w * 0.5f;
            C->raster_pos[1] = vp->y + (clip[1] / clip[3] + 1.0f) * vp->h * 0.5f;
            C->raster_pos[2] = (GLfloat)(n + (clip[2] / clip[3] + 1.0) * (f - n) * 0.5);
            C->raster_pos[3] = clip[3];
            C->raster_distance = fabsf(eye[2]);
        }
    }

    if (C->render_mode == GL_SELECT && C->raster_valid)
        mglCompatSelectRaster(ctx);
}

#define RASTER_FAMILY(n, T)                                                                                  \
static void cRasterPos2##n(GLMContext ctx, T x, T y) { mglCompatRasterPos(ctx, 0, (GLfloat)x, (GLfloat)y, 0, 1); } \
static void cRasterPos2##n##v(GLMContext ctx, const T *v) { mglCompatRasterPos(ctx, 0, (GLfloat)v[0], (GLfloat)v[1], 0, 1); } \
static void cRasterPos3##n(GLMContext ctx, T x, T y, T z)                                                    \
{ mglCompatRasterPos(ctx, 0, (GLfloat)x, (GLfloat)y, (GLfloat)z, 1); }                                       \
static void cRasterPos3##n##v(GLMContext ctx, const T *v)                                                    \
{ mglCompatRasterPos(ctx, 0, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], 1); }                              \
static void cRasterPos4##n(GLMContext ctx, T x, T y, T z, T w)                                               \
{ mglCompatRasterPos(ctx, 0, (GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w); }                              \
static void cRasterPos4##n##v(GLMContext ctx, const T *v)                                                    \
{ mglCompatRasterPos(ctx, 0, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]); }                  \
static void cWindowPos2##n(GLMContext ctx, T x, T y) { mglCompatRasterPos(ctx, 1, (GLfloat)x, (GLfloat)y, 0, 1); } \
static void cWindowPos2##n##v(GLMContext ctx, const T *v) { mglCompatRasterPos(ctx, 1, (GLfloat)v[0], (GLfloat)v[1], 0, 1); } \
static void cWindowPos3##n(GLMContext ctx, T x, T y, T z)                                                    \
{ mglCompatRasterPos(ctx, 1, (GLfloat)x, (GLfloat)y, (GLfloat)z, 1); }                                       \
static void cWindowPos3##n##v(GLMContext ctx, const T *v)                                                    \
{ mglCompatRasterPos(ctx, 1, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], 1); }

RASTER_FAMILY(s, GLshort)
RASTER_FAMILY(i, GLint)
RASTER_FAMILY(f, GLfloat)
RASTER_FAMILY(d, GLdouble)

// --- drawing a rectangle of pixels at the raster position ----------------------

static const char *raster_vs =
    "#version 410 core\n"
    "layout(location = 0) in vec4 p;\n"
    "void main() { gl_Position = p; }\n";

// texels by window position; zoom may be negative, which mirrors
static const char *pixels_fs =
    "#version 410 core\n"
    "uniform sampler2D t;\n"
    "uniform vec4 rect;\n"
    "uniform vec4 scale;\n"
    "uniform vec4 bias;\n"
    "uniform float depth;\n"
    "layout(location = 0) out vec4 o;\n"
    "void main() {\n"
    "    ivec2 size = textureSize(t, 0);\n"
    "    ivec2 c = clamp(ivec2(floor((gl_FragCoord.xy - rect.xy) / rect.zw)), ivec2(0), size - 1);\n"
    "    o = texelFetch(t, c, 0) * scale + bias;\n"
    "    gl_FragDepth = depth;\n"
    "}\n";

static const char *bitmap_fs =
    "#version 410 core\n"
    "uniform sampler2D t;\n"
    "uniform vec4 rect;\n"
    "uniform vec4 color;\n"
    "uniform float depth;\n"
    "layout(location = 0) out vec4 o;\n"
    "void main() {\n"
    "    ivec2 size = textureSize(t, 0);\n"
    "    ivec2 c = clamp(ivec2(floor(gl_FragCoord.xy - rect.xy)), ivec2(0), size - 1);\n"
    "    if (texelFetch(t, c, 0).r < 0.5) discard;\n"
    "    o = color;\n"
    "    gl_FragDepth = depth;\n"
    "}\n";

static GLuint buildProgram(GLMContext ctx, const char *fs)
{
    GLuint v = mglCreateShader(ctx, GL_VERTEX_SHADER), f = mglCreateShader(ctx, GL_FRAGMENT_SHADER), p;
    GLenum err = STATE(error);
    Program *prog;

    ctx->error_suppress++;
    mglShaderSource(ctx, v, 1, &raster_vs, NULL);
    mglCompileShader(ctx, v);
    mglShaderSource(ctx, f, 1, &fs, NULL);
    mglCompileShader(ctx, f);
    p = mglCreateProgram(ctx);
    mglAttachShader(ctx, p, v);
    mglAttachShader(ctx, p, f);
    mglLinkProgram(ctx, p);
    mglDeleteShader(ctx, v);
    mglDeleteShader(ctx, f);
    ctx->error_suppress--;
    STATE(error) = err;

    prog = findProgram(ctx, p);
    if (prog == NULL || !prog->executable)
    {
        MGL_ERR("MGL Error: the raster program did not build\n");
        mglDeleteProgram(ctx, p);
        return 0;
    }

    return p;
}

static GLint location(GLMContext ctx, GLuint prog, const char *name)
{
    GLint loc;
    loc = mglGetUniformLocation(ctx, prog, name);
    return loc;
}

static bool programs(GLMContext ctx)
{
    if (C->raster_prog == 0)
    {
        C->raster_prog = buildProgram(ctx, pixels_fs);
        C->bitmap_prog = buildProgram(ctx, bitmap_fs);

        if (C->raster_prog == 0 || C->bitmap_prog == 0)
            return false;

        mglProgramUniform1i(ctx, C->raster_prog, location(ctx, C->raster_prog, "t"), 0);
        mglProgramUniform1i(ctx, C->bitmap_prog, location(ctx, C->bitmap_prog, "t"), 0);
    }

    if (C->raster_tex_name == 0)
        mglGenTextures(ctx, 1, &C->raster_tex_name);

    return C->raster_prog && C->bitmap_prog && C->raster_tex_name;
}

typedef struct {
    GLuint active;
    Texture *unit0;
    Sampler *sampler0;
    Buffer *unpack;
    PixelStore store;
} TexSave;

// the scratch texture goes on unit 0 for as long as it is needed
static void takeUnit(GLMContext ctx, TexSave *s, bool plain_unpack)
{
    s->active = STATE(active_texture);
    s->unit0 = STATE(texture_units[0].textures[_TEXTURE_2D]);
    s->sampler0 = STATE(texture_samplers[0]);
    s->unpack = STATE(buffers[_PIXEL_UNPACK_BUFFER]);
    s->store = STATE(unpack);

    STATE(active_texture) = 0;
    STATE(texture_samplers[0]) = NULL;

    if (plain_unpack)
    {
        STATE(buffers[_PIXEL_UNPACK_BUFFER]) = NULL;
        memset(&STATE(unpack), 0, sizeof(PixelStore));
        STATE(unpack.alignment) = 1;
    }

    mglBindTexture(ctx, GL_TEXTURE_2D, C->raster_tex_name);

    // one level and no mipmaps, or the texture is incomplete and reads black
    mglTexParameteri(ctx, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    mglTexParameteri(ctx, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

static void giveUnit(GLMContext ctx, TexSave *s)
{
    STATE(texture_units[0].textures[_TEXTURE_2D]) = s->unit0;
    STATE(texture_samplers[0]) = s->sampler0;
    STATE(active_texture) = s->active;
    STATE(buffers[_PIXEL_UNPACK_BUFFER]) = s->unpack;
    STATE(unpack) = s->store;
    STATE(dirty_bits) |= DIRTY_TEX_BINDING | DIRTY_TEX | DIRTY_SAMPLER;
}

// draws the window rectangle [x0, x1] x [y0, y1] with prog
static void drawRect(GLMContext ctx, GLuint prog_name, GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1)
{
    ViewportRect *vp = &STATE(viewport[0]);
    Program *prog = findProgram(ctx, prog_name), *was_prog = STATE(program);
    VertexArray *was_vao = STATE(vao);
    GLenum modes[2] = { C->polygon_mode[0], C->polygon_mode[1] };
    GLuint core_mode = STATE_VAR(polygon_mode);
    GLboolean cull = STATE(caps.cull_face);
    GLfloat q[4][4];

    if (prog == NULL || vp->w == 0 || vp->h == 0)
        return;

    // window to normalised device coordinates, through the current viewport
    GLfloat ax = (x0 - vp->x) / vp->w * 2.0f - 1.0f, bx = (x1 - vp->x) / vp->w * 2.0f - 1.0f;
    GLfloat ay = (y0 - vp->y) / vp->h * 2.0f - 1.0f, by = (y1 - vp->y) / vp->h * 2.0f - 1.0f;

    GLfloat corners[4][2] = { { ax, ay }, { bx, ay }, { bx, by }, { ax, by } };

    for (int i = 0; i < 4; i++)
    {
        q[i][0] = corners[i][0];
        q[i][1] = corners[i][1];
        q[i][2] = 0.0f;
        q[i][3] = 1.0f;
    }

    C->polygon_mode[0] = C->polygon_mode[1] = GL_FILL;
    STATE_VAR(polygon_mode) = GL_FILL;
    STATE(caps.cull_face) = GL_FALSE;

    // set up directly: glVertexAttribPointer would take the application's
    // array buffer, if it has one bound
    VertexArray *vao = C->raster_vao;

    vao->attrib[0].size = 4;
    vao->attrib[0].type = GL_FLOAT;
    vao->attrib[0].normalized = GL_FALSE;
    vao->attrib[0].is_integer = GL_FALSE;
    vao->attrib[0].is_long = GL_FALSE;
    vao->attrib[0].relativeoffset = 0;
    vao->attrib[0].buffer_bindingindex = 0;
    vao->attrib[0].pointer = (GLintptr)q;
    vao->bindings[0].buffer = NULL;
    vao->bindings[0].offset = (GLintptr)q;
    vao->bindings[0].stride = 4 * sizeof(GLfloat);
    vao->bindings[0].divisor = 0;
    vao->enabled_attribs = 1;
    vao->element_array.buffer = NULL;
    vao->dirty_bits |= DIRTY_VAO | DIRTY_VAO_BUFFER_BASE;

    STATE(program) = prog;
    STATE(vao) = vao;
    STATE(dirty_bits) |= DIRTY_PROGRAM | DIRTY_VAO | DIRTY_RENDER_STATE;

    C->no_record++;
    mglCompatDrawFull(ctx, GL_TRIANGLE_FAN, 0, 4, 0, NULL, 1, 0, 0);
    C->no_record--;

    STATE(program) = was_prog;
    STATE(vao) = was_vao;
    C->polygon_mode[0] = modes[0];
    C->polygon_mode[1] = modes[1];
    STATE_VAR(polygon_mode) = core_mode;
    STATE(caps.cull_face) = cull;
    STATE(dirty_bits) |= DIRTY_PROGRAM | DIRTY_VAO | DIRTY_RENDER_STATE;
}

// What the scratch texture holds a format as, so the old formats read back
// the way they should.
static GLenum scratchFormat(GLenum format, GLenum type)
{
    bool wide = type == GL_FLOAT || type == GL_HALF_FLOAT || type == GL_INT || type == GL_UNSIGNED_INT;

    switch (format)
    {
        case GL_LUMINANCE: return wide ? GL_LUMINANCE32F_ARB : GL_LUMINANCE8;
        case GL_LUMINANCE_ALPHA: return wide ? GL_LUMINANCE_ALPHA32F_ARB : GL_LUMINANCE8_ALPHA8;
        case GL_ALPHA: return wide ? GL_ALPHA32F_ARB : GL_ALPHA8;
    }

    return wide ? GL_RGBA32F : GL_RGBA8;
}

static bool colorFormat(GLenum format)
{
    switch (format)
    {
        case GL_RGBA: case GL_RGB: case GL_BGRA: case GL_BGR: case GL_RED: case GL_RG:
        case GL_LUMINANCE: case GL_LUMINANCE_ALPHA: case GL_ALPHA:
            return true;
    }

    return false;
}

static void drawPixels(GLMContext ctx, GLsizei w, GLsizei h, GLenum format, GLenum type, const void *pixels, bool packed)
{
    TexSave save;
    GLfloat rect[4], scale[4], bias[4];

    if (!programs(ctx))
        return;

    takeUnit(ctx, &save, packed);
    mglTexImage2D(ctx, GL_TEXTURE_2D, 0, (GLint)scratchFormat(format, type), w, h, 0, format, type, pixels);

    // a list keeps the pixels as unpacked now, as floats
    if (mglCompatRecording(ctx))
    {
        size_t bytes = (size_t)w * (size_t)h * 4 * sizeof(GLfloat);
        GLfloat *copy = (GLfloat *)malloc(bytes ? bytes : 1);

        if (copy)
        {
            PixelStore pack = STATE(pack);
            Buffer *pb = STATE(buffers[_PIXEL_PACK_BUFFER]);

            memset(&STATE(pack), 0, sizeof(PixelStore));
            STATE(pack.alignment) = 1;
            STATE(buffers[_PIXEL_PACK_BUFFER]) = NULL;
            mglGetTexImage(ctx, GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, copy);
            STATE(pack) = pack;
            STATE(buffers[_PIXEL_PACK_BUFFER]) = pb;

            mglCompatRecordDrawPixels(ctx, w, h, GL_RGBA, GL_FLOAT, copy, bytes);
            free(copy);
        }

        if (!mglCompatExecuting(ctx))
        {
            giveUnit(ctx, &save);
            return;
        }
    }

    rect[0] = C->raster_pos[0];
    rect[1] = C->raster_pos[1];
    rect[2] = C->zoom_x;
    rect[3] = C->zoom_y;
    memcpy(scale, C->pixel_scale, sizeof(scale));
    memcpy(bias, C->pixel_bias, sizeof(bias));

    mglProgramUniform4fv(ctx, C->raster_prog, location(ctx, C->raster_prog, "rect"), 1, rect);
    mglProgramUniform4fv(ctx, C->raster_prog, location(ctx, C->raster_prog, "scale"), 1, scale);
    mglProgramUniform4fv(ctx, C->raster_prog, location(ctx, C->raster_prog, "bias"), 1, bias);
    mglProgramUniform1f(ctx, C->raster_prog, location(ctx, C->raster_prog, "depth"), C->raster_pos[2]);

    drawRect(ctx, C->raster_prog, rect[0], rect[1], rect[0] + w * C->zoom_x, rect[1] + h * C->zoom_y);

    giveUnit(ctx, &save);
}

static void cDrawPixels(GLMContext ctx, GLsizei w, GLsizei h, GLenum format, GLenum type, const void *pixels)
{
    ERROR_CHECK_RETURN(w >= 0 && h >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);

    if (!mglCompatRecording(ctx))
    {
        if (C->render_mode == GL_SELECT)
        {
            if (C->raster_valid)
                mglCompatSelectRaster(ctx);
            return;
        }

        if (!C->raster_valid || w == 0 || h == 0)
            return;
    }

    // depth and stencil pixels are accepted and not written
    if (!colorFormat(format))
    {
        ERROR_CHECK_RETURN(format == GL_DEPTH_COMPONENT || format == GL_STENCIL_INDEX ||
                           format == GL_DEPTH_STENCIL || format == GL_COLOR_INDEX, GL_INVALID_ENUM);
        return;
    }

    drawPixels(ctx, w, h, format, type, pixels, false);
}

void mglCompatDrawPixelsPacked(GLMContext ctx, GLsizei w, GLsizei h, GLenum format, GLenum type, const void *pixels)
{
    if (C->render_mode == GL_SELECT)
    {
        if (C->raster_valid)
            mglCompatSelectRaster(ctx);
        return;
    }

    if (!C->raster_valid || w <= 0 || h <= 0 || pixels == NULL)
        return;

    drawPixels(ctx, w, h, format, type, pixels, true);
}

// --- bitmaps -------------------------------------------------------------------------

// Unpacks a bitmap the way glBitmap reads one, into rows of (w + 7) / 8
// bytes with the first pixel in the top bit.
static GLubyte *unpackBitmap(GLMContext ctx, GLsizei w, GLsizei h, const GLubyte *src)
{
    PixelStore *ps = &STATE(unpack);
    GLint row_pixels = ps->row_length > 0 ? ps->row_length : w;
    GLint align = ps->alignment > 0 ? ps->alignment : 1;
    size_t row_bytes = (size_t)((row_pixels + 7) / 8);
    size_t out_row = (size_t)((w + 7) / 8);
    GLubyte *out;
    Buffer *pub = STATE(buffers[_PIXEL_UNPACK_BUFFER]);

    row_bytes = (row_bytes + (size_t)align - 1) / (size_t)align * (size_t)align;

    // with an unpack buffer bound the pointer is an offset into it
    if (pub)
    {
        const GLubyte *base = (const GLubyte *)mglBufferCPUData(ctx, pub);

        if (base == NULL)
            return NULL;

        src = base + (uintptr_t)src;
    }

    out = (GLubyte *)calloc(out_row * (size_t)h + 1, 1);
    if (out == NULL || src == NULL)
        return out;

    for (GLsizei y = 0; y < h; y++)
    {
        const GLubyte *row = src + (size_t)(y + ps->skip_rows) * row_bytes;

        for (GLsizei x = 0; x < w; x++)
        {
            GLint bit = x + ps->skip_pixels;
            GLubyte byte = row[bit / 8];
            GLint on = ps->lsb_first ? (byte >> (bit % 8)) & 1 : (byte >> (7 - bit % 8)) & 1;

            if (on)
                out[(size_t)y * out_row + (size_t)x / 8] |= (GLubyte)(0x80 >> (x % 8));
        }
    }

    return out;
}

void mglCompatBitmapPacked(GLMContext ctx, GLsizei w, GLsizei h, GLfloat xo, GLfloat yo, GLfloat xm, GLfloat ym,
                           const GLubyte *bits)
{
    if (mglCompatRecording(ctx))
    {
        mglCompatRecordBitmap(ctx, w, h, xo, yo, xm, ym, bits);
        if (!mglCompatExecuting(ctx))
            return;
    }

    if (!C->raster_valid)
        return;

    if (C->render_mode == GL_SELECT)
    {
        mglCompatSelectRaster(ctx);
    }
    else if (w > 0 && h > 0 && bits && C->render_mode == GL_RENDER && programs(ctx))
    {
        size_t row = (size_t)((w + 7) / 8);
        GLubyte *texels = (GLubyte *)malloc((size_t)w * (size_t)h);
        TexSave save;
        GLfloat rect[4];

        if (texels)
        {
            for (GLsizei y = 0; y < h; y++)
                for (GLsizei x = 0; x < w; x++)
                    texels[(size_t)y * (size_t)w + (size_t)x] =
                        (bits[(size_t)y * row + (size_t)x / 8] & (0x80 >> (x % 8))) ? 255 : 0;

            takeUnit(ctx, &save, true);
            C->no_record++;
            mglTexImage2D(ctx, GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, texels);
            C->no_record--;

            // the bitmap's origin sits at the raster position, and it is not zoomed
            rect[0] = floorf(C->raster_pos[0] - xo);
            rect[1] = floorf(C->raster_pos[1] - yo);
            rect[2] = rect[3] = 1.0f;

            mglProgramUniform4fv(ctx, C->bitmap_prog, location(ctx, C->bitmap_prog, "rect"), 1, rect);
            mglProgramUniform4fv(ctx, C->bitmap_prog, location(ctx, C->bitmap_prog, "color"), 1, C->raster_color);
            mglProgramUniform1f(ctx, C->bitmap_prog, location(ctx, C->bitmap_prog, "depth"), C->raster_pos[2]);

            drawRect(ctx, C->bitmap_prog, rect[0], rect[1], rect[0] + w, rect[1] + h);
            giveUnit(ctx, &save);
            free(texels);
        }
    }

    C->raster_pos[0] += xm;
    C->raster_pos[1] += ym;
}

static void cBitmap(GLMContext ctx, GLsizei w, GLsizei h, GLfloat xo, GLfloat yo, GLfloat xm, GLfloat ym,
                    const GLubyte *bitmap)
{
    GLubyte *bits = NULL;

    ERROR_CHECK_RETURN(w >= 0 && h >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);

    if (w > 0 && h > 0 && (bitmap || STATE(buffers[_PIXEL_UNPACK_BUFFER])))
        bits = unpackBitmap(ctx, w, h, bitmap);

    mglCompatBitmapPacked(ctx, w, h, xo, yo, xm, ym, bits);
    free(bits);
}

// --- copying pixels ------------------------------------------------------------------

static void cCopyPixels(GLMContext ctx, GLint x, GLint y, GLsizei w, GLsizei h, GLenum type)
{
    TexSave save;
    GLfloat rect[4], scale[4], bias[4];

    ERROR_CHECK_RETURN(w >= 0 && h >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(type == GL_COLOR || type == GL_DEPTH || type == GL_STENCIL || type == GL_DEPTH_STENCIL,
                       GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);

    if (C->render_mode == GL_SELECT)
    {
        if (C->raster_valid)
            mglCompatSelectRaster(ctx);
        return;
    }

    // depth and stencil copies are accepted and not done
    if (!C->raster_valid || w == 0 || h == 0 || type != GL_COLOR || !programs(ctx))
        return;

    takeUnit(ctx, &save, true);
    mglCopyTexImage2D(ctx, GL_TEXTURE_2D, 0, GL_RGBA8, x, y, w, h, 0);

    rect[0] = C->raster_pos[0];
    rect[1] = C->raster_pos[1];
    rect[2] = C->zoom_x;
    rect[3] = C->zoom_y;
    memcpy(scale, C->pixel_scale, sizeof(scale));
    memcpy(bias, C->pixel_bias, sizeof(bias));

    mglProgramUniform4fv(ctx, C->raster_prog, location(ctx, C->raster_prog, "rect"), 1, rect);
    mglProgramUniform4fv(ctx, C->raster_prog, location(ctx, C->raster_prog, "scale"), 1, scale);
    mglProgramUniform4fv(ctx, C->raster_prog, location(ctx, C->raster_prog, "bias"), 1, bias);
    mglProgramUniform1f(ctx, C->raster_prog, location(ctx, C->raster_prog, "depth"), C->raster_pos[2]);

    drawRect(ctx, C->raster_prog, rect[0], rect[1], rect[0] + w * C->zoom_x, rect[1] + h * C->zoom_y);
    giveUnit(ctx, &save);
}

#define SET(field, fn) ctx->dispatch.field = fn

void mglCompatInstallRaster(GLMContext ctx)
{
#define RASTER_SET(n) \
    SET(raster_pos2##n, cRasterPos2##n); SET(raster_pos2##n##v, cRasterPos2##n##v); \
    SET(raster_pos3##n, cRasterPos3##n); SET(raster_pos3##n##v, cRasterPos3##n##v); \
    SET(raster_pos4##n, cRasterPos4##n); SET(raster_pos4##n##v, cRasterPos4##n##v); \
    SET(window_pos2##n, cWindowPos2##n); SET(window_pos2##n##v, cWindowPos2##n##v); \
    SET(window_pos3##n, cWindowPos3##n); SET(window_pos3##n##v, cWindowPos3##n##v)

    RASTER_SET(s); RASTER_SET(i); RASTER_SET(f); RASTER_SET(d);

    SET(draw_pixels, cDrawPixels);
    SET(bitmap, cBitmap);
    SET(copy_pixels, cCopyPixels);
}

void mglCompatFreeRaster(GLMContext ctx)
{
    if (C->raster_prog)
        mglDeleteProgram(ctx, C->raster_prog);
    if (C->bitmap_prog)
        mglDeleteProgram(ctx, C->bitmap_prog);
    if (C->raster_tex_name)
        mglDeleteTextures(ctx, 1, &C->raster_tex_name);

    C->raster_prog = C->bitmap_prog = C->raster_tex_name = 0;
}

#endif /* MGL_COMPAT_CONTEXTS */
