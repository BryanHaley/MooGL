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
 * compat_apple.c
 * MGL
 *
 * Apple's own extensions from early Mac OS X, in compatibility contexts:
 * GL_APPLE_ycbcr_422, GL_APPLE_client_storage and GL_APPLE_texture_range.
 */

#include <stdlib.h>
#include <string.h>

#include "glm_context.h"
#include "compat.h"

#ifdef MGL_COMPAT_CONTEXTS

#define C (ctx->compat)

extern __thread GLMContext _ctx;
extern void mgl_lazy_init(void);

GLuint textureIndexFromTarget(GLMContext ctx, GLenum target);
Texture *currentTexture(GLMContext ctx, GLuint index);
void *getBufferData(GLMContext ctx, Buffer *ptr);

static Texture *bound(GLMContext ctx, GLenum target)
{
    GLuint index = textureIndexFromTarget(ctx, target);

    return index == _MAX_TEXTURE_TYPES ? NULL : currentTexture(ctx, index);
}

// --- GL_APPLE_ycbcr_422 --------------------------------------------------------------

static GLubyte clampByte(GLint v)
{
    return (GLubyte)(v < 0 ? 0 : v > 255 ? 255 : v);
}

// Pairs of pixels share their chroma: Cb with the first, Cr with the second.
// _REV is '2vuy' (Cb Y0 Cr Y1 in memory), the other type 'yuvs' (Y0 Cb Y1 Cr).
// The spec leaves the conversion open; this is BT.601 video range.
static GLubyte *ycbcrToRGBA(GLMContext ctx, GLsizei width, GLsizei height, GLenum type, const void *pixels)
{
    const PixelStore *ps = &STATE(unpack);
    size_t pitch = mglPixelStoreRowPitch(ps, width, 2);
    size_t skip = mglPixelStoreSkipBytes2D(ps, 2, pitch);
    Buffer *pbo = STATE(buffers[_PIXEL_UNPACK_BUFFER]);
    const GLubyte *src = (const GLubyte *)pixels;
    GLubyte *out;
    bool rev = type == GL_UNSIGNED_SHORT_8_8_REV_APPLE;

    if (pbo)
    {
        const GLubyte *base = (const GLubyte *)getBufferData(ctx, pbo);
        size_t need = (size_t)(uintptr_t)pixels + skip + pitch * (size_t)(height - 1) + (size_t)width * 2;

        if (base == NULL || need > (size_t)pbo->size)
        {
            ctx->error_func(ctx, __FUNCTION__, GL_INVALID_OPERATION);
            return NULL;
        }
        src = base + (uintptr_t)pixels;
    }

    out = (GLubyte *)malloc((size_t)width * (size_t)height * 4);
    if (out == NULL)
    {
        ctx->error_func(ctx, __FUNCTION__, GL_OUT_OF_MEMORY);
        return NULL;
    }

    for (GLsizei y = 0; y < height; y++)
    {
        const GLubyte *row = src + skip + pitch * (size_t)y;

        for (GLsizei x = 0; x < width; x++)
        {
            GLsizei pair = x & ~1;
            // chroma and luma of a pixel are one 16-bit word
            const GLubyte *p0 = row + (size_t)pair * 2;
            const GLubyte *p1 = pair + 1 < width ? p0 + 2 : p0;
            const GLubyte *me = row + (size_t)x * 2;
            GLint yv = rev ? me[1] : me[0];
            GLint cb = rev ? p0[0] : p0[1];
            GLint cr = rev ? p1[0] : p1[1];
            GLint c = 298 * (yv - 16), d = cb - 128, e = cr - 128;
            GLubyte *o = out + ((size_t)y * (size_t)width + (size_t)x) * 4;

            o[0] = clampByte((c + 409 * e + 128) >> 8);
            o[1] = clampByte((c - 100 * d - 208 * e + 128) >> 8);
            o[2] = clampByte((c + 516 * d + 128) >> 8);
            o[3] = 255;
        }
    }

    return out;
}

// the converted copy is tightly packed and comes from client memory
typedef struct {
    PixelStore unpack;
    Buffer *pbo;
} SavedUnpack;

static void plainUnpack(GLMContext ctx, SavedUnpack *s)
{
    s->unpack = STATE(unpack);
    s->pbo = STATE(buffers[_PIXEL_UNPACK_BUFFER]);

    memset(&STATE(unpack), 0, sizeof(PixelStore));
    STATE(unpack).alignment = 1;
    STATE(buffers[_PIXEL_UNPACK_BUFFER]) = NULL;
}

static void restoreUnpack(GLMContext ctx, const SavedUnpack *s)
{
    STATE(unpack) = s->unpack;
    STATE(buffers[_PIXEL_UNPACK_BUFFER]) = s->pbo;
}

static bool ycbcrType(GLMContext ctx, GLenum type)
{
    if (type == GL_UNSIGNED_SHORT_8_8_APPLE || type == GL_UNSIGNED_SHORT_8_8_REV_APPLE)
        return true;

    ctx->error_func(ctx, __FUNCTION__, GL_INVALID_ENUM);
    return false;
}

static void cTexImage2D(GLMContext ctx, GLenum target, GLint level, GLint internalformat, GLsizei width,
                        GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels)
{
    if (format != GL_YCBCR_422_APPLE)
    {
        C->next.tex_image2D(ctx, target, level, internalformat, width, height, border, format, type, pixels);
        return;
    }

    if (!ycbcrType(ctx, type))
        return;

    if (internalformat == GL_YCBCR_422_APPLE)
        internalformat = GL_RGB8;

    GLubyte *rgba = NULL;
    SavedUnpack saved;

    if ((pixels || STATE(buffers[_PIXEL_UNPACK_BUFFER])) && width > 0 && height > 0)
    {
        rgba = ycbcrToRGBA(ctx, width, height, type, pixels);
        if (rgba == NULL)
            return;
    }

    plainUnpack(ctx, &saved);
    C->next.tex_image2D(ctx, target, level, internalformat, width, height, border, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    restoreUnpack(ctx, &saved);
    free(rgba);
}

static void cTexSubImage2D(GLMContext ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset,
                           GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
    if (format != GL_YCBCR_422_APPLE)
    {
        C->next.tex_sub_image2D(ctx, target, level, xoffset, yoffset, width, height, format, type, pixels);
        return;
    }

    if (!ycbcrType(ctx, type) || width <= 0 || height <= 0)
        return;

    GLubyte *rgba = ycbcrToRGBA(ctx, width, height, type, pixels);
    SavedUnpack saved;

    if (rgba == NULL)
        return;

    plainUnpack(ctx, &saved);
    C->next.tex_sub_image2D(ctx, target, level, xoffset, yoffset, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    restoreUnpack(ctx, &saved);
    free(rgba);
}

// --- GL_APPLE_client_storage ---------------------------------------------------------
//
// The spec lets the GL copy when it cannot use the application's memory
// directly. MooGL always copies, so the application's promise to keep the
// memory alive costs it nothing.

static void cPixelStorei(GLMContext ctx, GLenum pname, GLint param)
{
    if (pname == GL_UNPACK_CLIENT_STORAGE_APPLE)
    {
        C->client_storage = param != 0;
        return;
    }

    C->next.pixel_storei(ctx, pname, param);
}

static void cPixelStoref(GLMContext ctx, GLenum pname, GLfloat param)
{
    if (pname == GL_UNPACK_CLIENT_STORAGE_APPLE)
    {
        C->client_storage = param != 0.0f;
        return;
    }

    C->next.pixel_storef(ctx, pname, param);
}

// --- GL_APPLE_texture_range ----------------------------------------------------------

static bool storageHint(GLMContext ctx, GLenum target, GLint value)
{
    Texture *tex = bound(ctx, target);

    if (tex == NULL)
    {
        ctx->error_func(ctx, __FUNCTION__, GL_INVALID_ENUM);
        return true;
    }

    if (value != GL_STORAGE_PRIVATE_APPLE && value != GL_STORAGE_CACHED_APPLE && value != GL_STORAGE_SHARED_APPLE)
    {
        ctx->error_func(ctx, __FUNCTION__, GL_INVALID_ENUM);
        return true;
    }

    tex->apple_storage_hint = (GLenum)value;
    return true;
}

static void cTexParameteri(GLMContext ctx, GLenum target, GLenum pname, GLint param)
{
    if (pname == GL_TEXTURE_STORAGE_HINT_APPLE && storageHint(ctx, target, param))
        return;

    C->next.tex_parameteri(ctx, target, pname, param);
}

static void cTexParameterf(GLMContext ctx, GLenum target, GLenum pname, GLfloat param)
{
    if (pname == GL_TEXTURE_STORAGE_HINT_APPLE && storageHint(ctx, target, (GLint)param))
        return;

    C->next.tex_parameterf(ctx, target, pname, param);
}

static void cTexParameteriv(GLMContext ctx, GLenum target, GLenum pname, const GLint *params)
{
    if (pname == GL_TEXTURE_STORAGE_HINT_APPLE && params && storageHint(ctx, target, params[0]))
        return;

    C->next.tex_parameteriv(ctx, target, pname, params);
}

static void cTexParameterfv(GLMContext ctx, GLenum target, GLenum pname, const GLfloat *params)
{
    if (pname == GL_TEXTURE_STORAGE_HINT_APPLE && params && storageHint(ctx, target, (GLint)params[0]))
        return;

    C->next.tex_parameterfv(ctx, target, pname, params);
}

// what the range queries answer, or false for a name this file does not own
static bool rangeParam(GLMContext ctx, GLenum target, GLenum pname, GLint *out)
{
    if (pname != GL_TEXTURE_STORAGE_HINT_APPLE && pname != GL_TEXTURE_RANGE_LENGTH_APPLE)
        return false;

    Texture *tex = bound(ctx, target);

    if (tex == NULL)
    {
        ctx->error_func(ctx, __FUNCTION__, GL_INVALID_ENUM);
        return true;
    }

    if (pname == GL_TEXTURE_RANGE_LENGTH_APPLE)
        *out = tex->apple_range_length;
    else
        *out = tex->apple_storage_hint ? (GLint)tex->apple_storage_hint : GL_STORAGE_PRIVATE_APPLE;

    return true;
}

static void cGetTexParameteriv(GLMContext ctx, GLenum target, GLenum pname, GLint *params)
{
    GLint v = 0;

    if (rangeParam(ctx, target, pname, &v))
    {
        if (params)
            params[0] = v;
        return;
    }

    C->next.get_tex_parameteriv(ctx, target, pname, params);
}

static void cGetTexParameterfv(GLMContext ctx, GLenum target, GLenum pname, GLfloat *params)
{
    GLint v = 0;

    if (rangeParam(ctx, target, pname, &v))
    {
        if (params)
            params[0] = (GLfloat)v;
        return;
    }

    C->next.get_tex_parameterfv(ctx, target, pname, params);
}

void glTextureRangeAPPLE(GLenum target, GLsizei length, const void *pointer)
{
    GLMContext ctx = (mgl_lazy_init(), _ctx);
    Texture *tex;

    if (ctx->compat == NULL)
    {
        ctx->error_func(ctx, __FUNCTION__, GL_INVALID_OPERATION);
        return;
    }

    if (length < 0)
    {
        ctx->error_func(ctx, __FUNCTION__, GL_INVALID_VALUE);
        return;
    }

    if ((tex = bound(ctx, target)) == NULL)
    {
        ctx->error_func(ctx, __FUNCTION__, GL_INVALID_ENUM);
        return;
    }

    tex->apple_range_pointer = (void *)pointer;
    tex->apple_range_length = length;
}

void glGetTexParameterPointervAPPLE(GLenum target, GLenum pname, void **params)
{
    GLMContext ctx = (mgl_lazy_init(), _ctx);
    Texture *tex;

    if (ctx->compat == NULL)
    {
        ctx->error_func(ctx, __FUNCTION__, GL_INVALID_OPERATION);
        return;
    }

    if (pname != GL_TEXTURE_RANGE_POINTER_APPLE || (tex = bound(ctx, target)) == NULL)
    {
        ctx->error_func(ctx, __FUNCTION__, GL_INVALID_ENUM);
        return;
    }

    if (params)
        *params = tex->apple_range_pointer;
}

#define SET(field, fn) ctx->dispatch.field = fn

void mglCompatInstallApple(GLMContext ctx)
{
    SET(tex_image2D, cTexImage2D);
    SET(tex_sub_image2D, cTexSubImage2D);
    SET(pixel_storei, cPixelStorei);
    SET(pixel_storef, cPixelStoref);
    SET(tex_parameteri, cTexParameteri);
    SET(tex_parameterf, cTexParameterf);
    SET(tex_parameteriv, cTexParameteriv);
    SET(tex_parameterfv, cTexParameterfv);
    SET(get_tex_parameteriv, cGetTexParameteriv);
    SET(get_tex_parameterfv, cGetTexParameterfv);
}

#endif /* MGL_COMPAT_CONTEXTS */
