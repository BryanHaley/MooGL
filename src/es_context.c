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
 * es_context.c
 * MGL
 *
 * OpenGL ES 3.1 contexts: the ES strings and limits, the enables ES has, and
 * GLSL ES shaders checked under their own rules before the desktop compiler
 * sees them.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <glslang_c_interface.h>

#include "glm_context.h"
#include "mgl_es.h"
#include "mgl_compat.h"
#include "moogl_version.h"
#include "mgl_format_table.h"
#include "mgl.h"

#ifdef MGL_OGL_ES

#define E (ctx->es)

#define STR_(x) #x
#define STR(x) STR_(x)

static const char *es_extensions[] = {
    "GL_EXT_buffer_storage",
    "GL_EXT_color_buffer_float",
    "GL_EXT_conservative_depth",
    "GL_EXT_texture_filter_anisotropic",
    "GL_KHR_debug",
    "GL_KHR_texture_compression_astc_ldr",
    "GL_OES_texture_float_linear",
    "GL_OES_texture_storage_multisample_2d_array",
};

#define ES_EXTENSIONS (GLuint)(sizeof(es_extensions) / sizeof(es_extensions[0]))

Texture *getTex(GLMContext ctx, GLuint name, GLenum target);

// --- strings and queries ---

static const GLubyte *esGetString(GLMContext ctx, GLenum name)
{
    switch (name)
    {
        case GL_VERSION:
            return (const GLubyte *)"OpenGL ES " STR(MGL_ES_MAJOR) "." STR(MGL_ES_MINOR) " MooGL " MOOGL_VERSION;

        case GL_SHADING_LANGUAGE_VERSION:
            return (const GLubyte *)"OpenGL ES GLSL ES " STR(MGL_ES_MAJOR) "." STR(MGL_ES_MINOR) "0";

        case GL_EXTENSIONS:
            if (E->extension_string == NULL)
            {
                size_t len = 1;

                for (GLuint i = 0; i < ES_EXTENSIONS; i++)
                    len += strlen(es_extensions[i]) + 1;

                E->extension_string = (char *)calloc(1, len);
                if (E->extension_string == NULL)
                    return (const GLubyte *)"";

                for (GLuint i = 0; i < ES_EXTENSIONS; i++)
                {
                    strcat(E->extension_string, es_extensions[i]);
                    strcat(E->extension_string, " ");
                }
            }
            return (const GLubyte *)E->extension_string;
    }

    return E->next.get_string(ctx, name);
}

static const GLubyte *esGetStringi(GLMContext ctx, GLenum name, GLuint index)
{
    if (name == GL_EXTENSIONS)
    {
        ERROR_CHECK_RETURN_VALUE(index < ES_EXTENSIONS, GL_INVALID_VALUE, NULL);
        return (const GLubyte *)es_extensions[index];
    }

    ERROR_RETURN_VALUE(GL_INVALID_ENUM, NULL);
}

// the queries whose answer differs in ES; false for the rest
static bool esGet(GLMContext ctx, GLenum pname, GLint *v)
{
    switch (pname)
    {
        case GL_MAJOR_VERSION: *v = MGL_ES_MAJOR; return true;
        case GL_MINOR_VERSION: *v = MGL_ES_MINOR; return true;
        case GL_NUM_EXTENSIONS: *v = (GLint)ES_EXTENSIONS; return true;
    }

    return false;
}

static bool esCap(GLenum cap);

// desktop queries ES does not have, desktop capabilities among them
static bool desktopOnly(GLMContext ctx, GLenum pname)
{
    switch (pname)
    {
        case GL_CONTEXT_PROFILE_MASK:
        case GL_NUM_SHADING_LANGUAGE_VERSIONS:
            return true;
    }

    return mglCapState(ctx, pname) >= 0 && !esCap(pname);
}

static void esGetIntegerv(GLMContext ctx, GLenum pname, GLint *data)
{
    ERROR_CHECK_RETURN(!desktopOnly(ctx, pname), GL_INVALID_ENUM);

    if (data && esGet(ctx, pname, data))
        return;

    E->next.get_integerv(ctx, pname, data);
}

static void esGetInteger64v(GLMContext ctx, GLenum pname, GLint64 *data)
{
    GLint v;

    ERROR_CHECK_RETURN(!desktopOnly(ctx, pname), GL_INVALID_ENUM);

    if (data && esGet(ctx, pname, &v))
    {
        *data = v;
        return;
    }

    E->next.get_integer64v(ctx, pname, data);
}

static void esGetFloatv(GLMContext ctx, GLenum pname, GLfloat *data)
{
    GLint v;

    ERROR_CHECK_RETURN(!desktopOnly(ctx, pname), GL_INVALID_ENUM);

    if (data && esGet(ctx, pname, &v))
    {
        *data = (GLfloat)v;
        return;
    }

    E->next.get_floatv(ctx, pname, data);
}

static void esGetBooleanv(GLMContext ctx, GLenum pname, GLboolean *data)
{
    GLint v;

    ERROR_CHECK_RETURN(!desktopOnly(ctx, pname), GL_INVALID_ENUM);

    if (data && esGet(ctx, pname, &v))
    {
        *data = v ? GL_TRUE : GL_FALSE;
        return;
    }

    E->next.get_booleanv(ctx, pname, data);
}

// --- enables ---

static bool esCap(GLenum cap)
{
    switch (cap)
    {
        case GL_BLEND:
        case GL_CULL_FACE:
        case GL_DEPTH_TEST:
        case GL_DITHER:
        case GL_POLYGON_OFFSET_FILL:
        case GL_PRIMITIVE_RESTART_FIXED_INDEX:
        case GL_RASTERIZER_DISCARD:
        case GL_SAMPLE_ALPHA_TO_COVERAGE:
        case GL_SAMPLE_COVERAGE:
        case GL_SAMPLE_MASK:
        case GL_SCISSOR_TEST:
        case GL_STENCIL_TEST:
        case GL_DEBUG_OUTPUT:
        case GL_DEBUG_OUTPUT_SYNCHRONOUS:
            return true;
    }

    return false;
}

static void esEnable(GLMContext ctx, GLenum cap)
{
    ERROR_CHECK_RETURN(esCap(cap), GL_INVALID_ENUM);
    E->next.enable(ctx, cap);
}

static void esDisable(GLMContext ctx, GLenum cap)
{
    ERROR_CHECK_RETURN(esCap(cap), GL_INVALID_ENUM);
    E->next.disable(ctx, cap);
}

static GLboolean esIsEnabled(GLMContext ctx, GLenum cap)
{
    ERROR_CHECK_RETURN_VALUE(esCap(cap), GL_INVALID_ENUM, GL_FALSE);
    return E->next.is_enabled(ctx, cap);
}

// --- draws ---

// the compatibility layer underneath also takes quads and polygons
static bool esPipelineDrawable(GLMContext ctx);

#define ES_MODE(mode) ERROR_CHECK_RETURN((mode) <= GL_TRIANGLE_FAN, GL_INVALID_ENUM); \
                      ERROR_CHECK_RETURN(esPipelineDrawable(ctx), GL_INVALID_OPERATION)

bool mglDrawFramebufferComplete(GLMContext ctx);

// The checks every draw makes. With no program or pipeline an ES draw draws
// nothing, but the compatibility layer underneath would draw fixed function.
static bool esDrawChecks(GLMContext ctx, GLsizei count, GLenum type)
{
    ERROR_CHECK_RETURN_VALUE(type == 0 || type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT ||
                             type == GL_UNSIGNED_INT, GL_INVALID_ENUM, false);
    ERROR_CHECK_RETURN_VALUE(count >= 0, GL_INVALID_VALUE, false);

    // ES 3.1 12.1: no indexed draw while transform feedback records
    if (type)
    {
        TransformFeedback *tf = STATE(transform_feedback);

        ERROR_CHECK_RETURN_VALUE(!(tf && tf->active && !tf->paused), GL_INVALID_OPERATION, false);
    }

    if (STATE(program) == NULL && STATE(program_pipeline) == NULL)
    {
        mglDrawFramebufferComplete(ctx);
        return false;
    }

    return true;
}

#define ES_DRAW(count, type) if (!esDrawChecks(ctx, count, type)) return

static void esDrawArrays(GLMContext ctx, GLenum mode, GLint first, GLsizei count)
{
    ES_MODE(mode);
    ES_DRAW(count, 0);
    E->next.draw_arrays(ctx, mode, first, count);
}

static void esDrawElements(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices)
{
    ES_MODE(mode);
    ES_DRAW(count, type);
    E->next.draw_elements(ctx, mode, count, type, indices);
}

static void esDrawRangeElements(GLMContext ctx, GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type,
                                const void *indices)
{
    ES_MODE(mode);
    ERROR_CHECK_RETURN(end >= start, GL_INVALID_VALUE);
    ES_DRAW(count, type);
    E->next.draw_range_elements(ctx, mode, start, end, count, type, indices);
}

static void esDrawArraysInstanced(GLMContext ctx, GLenum mode, GLint first, GLsizei count, GLsizei instances)
{
    ES_MODE(mode);
    ERROR_CHECK_RETURN(instances >= 0, GL_INVALID_VALUE);
    ES_DRAW(count, 0);
    E->next.draw_arrays_instanced(ctx, mode, first, count, instances);
}

static void esDrawElementsInstanced(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices,
                                    GLsizei instances)
{
    ES_MODE(mode);
    ERROR_CHECK_RETURN(instances >= 0, GL_INVALID_VALUE);
    ES_DRAW(count, type);
    E->next.draw_elements_instanced(ctx, mode, count, type, indices, instances);
}

// ES 3.1 10.5: an indirect draw needs a vertex array object, buffers behind
// every enabled array, and no transform feedback running
static bool esIndirectAllowed(GLMContext ctx)
{
    VertexArray *vao = STATE(vao);
    TransformFeedback *tf = STATE(transform_feedback);

    if (vao == NULL || vao->name == 0)
        return false;

    for (int i = 0; i < MAX_ATTRIBS; i++)
        if ((vao->enabled_attribs & (1u << i)) && vao->bindings[vao->attrib[i].buffer_bindingindex].buffer == NULL)
            return false;

    return !(tf && tf->active && !tf->paused);
}

static void esDrawArraysIndirect(GLMContext ctx, GLenum mode, const void *indirect)
{
    ES_MODE(mode);
    ERROR_CHECK_RETURN(esIndirectAllowed(ctx), GL_INVALID_OPERATION);
    E->next.draw_arrays_indirect(ctx, mode, indirect);
}

static void esDrawElementsIndirect(GLMContext ctx, GLenum mode, GLenum type, const void *indirect)
{
    ES_MODE(mode);
    ERROR_CHECK_RETURN(esIndirectAllowed(ctx), GL_INVALID_OPERATION);
    E->next.draw_elements_indirect(ctx, mode, type, indirect);
}

// --- vertex arrays ---

// ES 3.1 10.3.1: the separate format and binding calls need a vertex array
// object other than the default one
#define ES_NAMED_VAO() ERROR_CHECK_RETURN(STATE(vao) && STATE(vao)->name != 0, GL_INVALID_OPERATION)

static void esBindVertexBuffer(GLMContext ctx, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride)
{
    ES_NAMED_VAO();
    E->next.bind_vertex_buffer(ctx, bindingindex, buffer, offset, stride);
}

static void esVertexAttribFormat(GLMContext ctx, GLuint attribindex, GLint size, GLenum type, GLboolean normalized,
                                 GLuint relativeoffset)
{
    ES_NAMED_VAO();
    E->next.vertex_attrib_format(ctx, attribindex, size, type, normalized, relativeoffset);
}

static void esVertexAttribIFormat(GLMContext ctx, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
    ES_NAMED_VAO();
    E->next.vertex_attrib_i_format(ctx, attribindex, size, type, relativeoffset);
}

static void esVertexAttribBinding(GLMContext ctx, GLuint attribindex, GLuint bindingindex)
{
    ES_NAMED_VAO();
    E->next.vertex_attrib_binding(ctx, attribindex, bindingindex);
}

static void esVertexBindingDivisor(GLMContext ctx, GLuint bindingindex, GLuint divisor)
{
    ES_NAMED_VAO();
    E->next.vertex_binding_divisor(ctx, bindingindex, divisor);
}

// --- reading pixels ---

static bool esReadFormat(GLenum format)
{
    switch (format)
    {
        case GL_RGBA: case GL_RGB: case GL_RG: case GL_RED:
        case GL_RGBA_INTEGER: case GL_RGB_INTEGER: case GL_RG_INTEGER: case GL_RED_INTEGER:
        case GL_ALPHA: case GL_LUMINANCE: case GL_LUMINANCE_ALPHA:
            return true;
    }

    return false;
}

static bool esReadType(GLenum type)
{
    switch (type)
    {
        case GL_UNSIGNED_BYTE: case GL_BYTE: case GL_UNSIGNED_SHORT: case GL_SHORT:
        case GL_UNSIGNED_INT: case GL_INT: case GL_HALF_FLOAT: case GL_FLOAT:
        case GL_UNSIGNED_SHORT_5_6_5: case GL_UNSIGNED_SHORT_4_4_4_4: case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_INT_2_10_10_10_REV: case GL_UNSIGNED_INT_10F_11F_11F_REV:
        case GL_UNSIGNED_INT_5_9_9_9_REV:
            return true;
    }

    return false;
}

static bool floatFormat(GLenum internalformat)
{
    switch (internalformat)
    {
        case GL_R16F: case GL_RG16F: case GL_RGB16F: case GL_RGBA16F:
        case GL_R32F: case GL_RG32F: case GL_RGB32F: case GL_RGBA32F:
        case GL_R11F_G11F_B10F:
            return true;
    }

    return false;
}

// ES 3.0 4.3.1: the one pair the colour buffer's kind takes, or the
// implementation's own pair; nothing else
static void esReadPixels(GLMContext ctx, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                         GLenum type, void *pixels)
{
    GLenum internalformat = mglReadColorInternalFormat(ctx);
    GLenum own_format, own_type;
    bool ok;

    ERROR_CHECK_RETURN(esReadFormat(format) && esReadType(type), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(width >= 0 && height >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(mglCheckFramebufferStatus(ctx, GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                       GL_INVALID_FRAMEBUFFER_OPERATION);
    ERROR_CHECK_RETURN(mglFramebufferSamples(ctx->state.readbuffer) == 0, GL_INVALID_OPERATION);

    mglReadColorFormatAndType(ctx, &own_format, &own_type);

    switch (internalformat ? mglFormatKind(internalformat) : MGL_FMT_COLOR_FLOAT)
    {
        case MGL_FMT_COLOR_INT:
            ok = format == GL_RGBA_INTEGER && type == GL_INT;
            break;

        case MGL_FMT_COLOR_UINT:
            ok = format == GL_RGBA_INTEGER && type == GL_UNSIGNED_INT;
            break;

        default:
            ok = format == GL_RGBA && (floatFormat(internalformat) ? type == GL_FLOAT : type == GL_UNSIGNED_BYTE);
            if (internalformat == GL_RGB10_A2)
                ok = ok || (format == GL_RGBA && type == GL_UNSIGNED_INT_2_10_10_10_REV);
            break;
    }

    ERROR_CHECK_RETURN(ok || (format == own_format && type == own_type), GL_INVALID_OPERATION);

    E->next.read_pixels(ctx, x, y, width, height, format, type, pixels);
}

// --- texture images ---

// ES 3.0 tables 3.2 and 3.3: every internal format, format and type that go together
static const struct { GLenum internalformat, format, type; } es_tex_formats[] = {
    { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE }, { GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_BYTE },
    { GL_RGBA4, GL_RGBA, GL_UNSIGNED_BYTE }, { GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE },
    { GL_RGBA8_SNORM, GL_RGBA, GL_BYTE }, { GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4 },
    { GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1 }, { GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV },
    { GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV }, { GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT },
    { GL_RGBA32F, GL_RGBA, GL_FLOAT }, { GL_RGBA16F, GL_RGBA, GL_FLOAT },
    { GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE }, { GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE },
    { GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT }, { GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT },
    { GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT }, { GL_RGBA32I, GL_RGBA_INTEGER, GL_INT },
    { GL_RGB10_A2UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV },
    { GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE }, { GL_RGB565, GL_RGB, GL_UNSIGNED_BYTE },
    { GL_SRGB8, GL_RGB, GL_UNSIGNED_BYTE }, { GL_RGB8_SNORM, GL_RGB, GL_BYTE },
    { GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5 }, { GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV },
    { GL_RGB9_E5, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV }, { GL_RGB16F, GL_RGB, GL_HALF_FLOAT },
    { GL_R11F_G11F_B10F, GL_RGB, GL_HALF_FLOAT }, { GL_RGB9_E5, GL_RGB, GL_HALF_FLOAT },
    { GL_RGB32F, GL_RGB, GL_FLOAT }, { GL_RGB16F, GL_RGB, GL_FLOAT },
    { GL_R11F_G11F_B10F, GL_RGB, GL_FLOAT }, { GL_RGB9_E5, GL_RGB, GL_FLOAT },
    { GL_RGB8UI, GL_RGB_INTEGER, GL_UNSIGNED_BYTE }, { GL_RGB8I, GL_RGB_INTEGER, GL_BYTE },
    { GL_RGB16UI, GL_RGB_INTEGER, GL_UNSIGNED_SHORT }, { GL_RGB16I, GL_RGB_INTEGER, GL_SHORT },
    { GL_RGB32UI, GL_RGB_INTEGER, GL_UNSIGNED_INT }, { GL_RGB32I, GL_RGB_INTEGER, GL_INT },
    { GL_RG8, GL_RG, GL_UNSIGNED_BYTE }, { GL_RG8_SNORM, GL_RG, GL_BYTE },
    { GL_RG16F, GL_RG, GL_HALF_FLOAT }, { GL_RG32F, GL_RG, GL_FLOAT }, { GL_RG16F, GL_RG, GL_FLOAT },
    { GL_RG8UI, GL_RG_INTEGER, GL_UNSIGNED_BYTE }, { GL_RG8I, GL_RG_INTEGER, GL_BYTE },
    { GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT }, { GL_RG16I, GL_RG_INTEGER, GL_SHORT },
    { GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT }, { GL_RG32I, GL_RG_INTEGER, GL_INT },
    { GL_R8, GL_RED, GL_UNSIGNED_BYTE }, { GL_R8_SNORM, GL_RED, GL_BYTE },
    { GL_R16F, GL_RED, GL_HALF_FLOAT }, { GL_R32F, GL_RED, GL_FLOAT }, { GL_R16F, GL_RED, GL_FLOAT },
    { GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE }, { GL_R8I, GL_RED_INTEGER, GL_BYTE },
    { GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT }, { GL_R16I, GL_RED_INTEGER, GL_SHORT },
    { GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT }, { GL_R32I, GL_RED_INTEGER, GL_INT },
    { GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT },
    { GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT },
    { GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT },
    { GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT },
    { GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8 },
    { GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV },
    // unsized
    { GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE }, { GL_RGBA, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4 },
    { GL_RGBA, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1 }, { GL_RGB, GL_RGB, GL_UNSIGNED_BYTE },
    { GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5 }, { GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE },
    { GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE }, { GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE },
    { GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT },
    { GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT },
    { GL_DEPTH_STENCIL, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8 },
};

#define ES_TEX_FORMATS (int)(sizeof(es_tex_formats) / sizeof(es_tex_formats[0]))

// GL_NO_ERROR, or the error ES raises for this triple
static GLenum esTexFormatError(GLint internalformat, GLenum format, GLenum type)
{
    bool known_i = false, known_f = false, known_t = false;

    for (int i = 0; i < ES_TEX_FORMATS; i++)
    {
        if (es_tex_formats[i].internalformat == (GLenum)internalformat && es_tex_formats[i].format == format &&
            es_tex_formats[i].type == type)
            return GL_NO_ERROR;

        // a base format names a format, wrongly paired or not
        known_i |= es_tex_formats[i].internalformat == (GLenum)internalformat ||
                   es_tex_formats[i].format == (GLenum)internalformat;
        known_f |= es_tex_formats[i].format == format;
        known_t |= es_tex_formats[i].type == type;
    }

    if (!known_f || !known_t)
        return GL_INVALID_ENUM;

    return known_i ? GL_INVALID_OPERATION : GL_INVALID_VALUE;
}

// ES 3.0 table 3.3: an unsized format is stored as the sized one its type names
static GLint esEffectiveFormat(GLint internalformat, GLenum type)
{
    switch (internalformat)
    {
        case GL_RGBA:
            return type == GL_UNSIGNED_SHORT_4_4_4_4 ? GL_RGBA4 :
                   type == GL_UNSIGNED_SHORT_5_5_5_1 ? GL_RGB5_A1 : GL_RGBA8;
        case GL_RGB:             return type == GL_UNSIGNED_SHORT_5_6_5 ? GL_RGB565 : GL_RGB8;
        case GL_LUMINANCE_ALPHA: return GL_LUMINANCE8_ALPHA8;
        case GL_LUMINANCE:       return GL_LUMINANCE8;
        case GL_ALPHA:           return GL_ALPHA8;
    }

    return internalformat;
}

static void esTexImage2D(GLMContext ctx, GLenum target, GLint level, GLint internalformat, GLsizei width,
                         GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels)
{
    GLenum e = esTexFormatError(internalformat, format, type);

    ERROR_CHECK_RETURN(e == GL_NO_ERROR, e);
    E->next.tex_image2D(ctx, target, level, esEffectiveFormat(internalformat, type), width, height, border, format,
                        type, pixels);

    Texture *tex = getTex(ctx, 0, target);

    if (tex)
        tex->es_unsized_depth = internalformat == GL_DEPTH_COMPONENT || internalformat == GL_DEPTH_STENCIL;
}

static void esTexImage3D(GLMContext ctx, GLenum target, GLint level, GLint internalformat, GLsizei width,
                         GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels)
{
    GLenum e = esTexFormatError(internalformat, format, type);

    ERROR_CHECK_RETURN(e == GL_NO_ERROR, e);
    E->next.tex_image3D(ctx, target, level, esEffectiveFormat(internalformat, type), width, height, depth, border,
                        format, type, pixels);
}

// --- rendering ---

// ES 3.0 table 3.13, and the float formats GL_EXT_color_buffer_float adds
bool mglESColorRenderable(GLenum internalformat)
{
    switch (internalformat)
    {
        case GL_R8: case GL_RG8: case GL_RGB8: case GL_RGB565: case GL_RGBA4: case GL_RGB5_A1:
        case GL_RGBA8: case GL_RGB10_A2: case GL_RGB10_A2UI: case GL_SRGB8_ALPHA8:
        case GL_R8I: case GL_R8UI: case GL_R16I: case GL_R16UI: case GL_R32I: case GL_R32UI:
        case GL_RG8I: case GL_RG8UI: case GL_RG16I: case GL_RG16UI: case GL_RG32I: case GL_RG32UI:
        case GL_RGBA8I: case GL_RGBA8UI: case GL_RGBA16I: case GL_RGBA16UI: case GL_RGBA32I: case GL_RGBA32UI:
        case GL_R16F: case GL_RG16F: case GL_RGBA16F: case GL_R32F: case GL_RG32F: case GL_RGBA32F:
        case GL_R11F_G11F_B10F:
        // unsized ones the texture calls take
        case GL_RGBA: case GL_RGB:
            return true;
    }

    return false;
}

GLuint textureIndexFromTarget(GLMContext ctx, GLenum target);
Texture *currentTexture(GLMContext ctx, GLuint index);

// ES 3.0 3.8.10: the base image is unsized, or both color-renderable and
// filterable, which leaves out integer formats
static void esGenerateMipmap(GLMContext ctx, GLenum target)
{
    ERROR_CHECK_RETURN(target == GL_TEXTURE_2D || target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY ||
                       target == GL_TEXTURE_CUBE_MAP, GL_INVALID_ENUM);

    Texture *tex = currentTexture(ctx, textureIndexFromTarget(ctx, target));
    GLenum f = tex ? (GLenum)tex->internalformat : 0;

    if (f && f != GL_RGB && f != GL_RGBA && f != GL_LUMINANCE && f != GL_ALPHA && f != GL_LUMINANCE_ALPHA)
    {
        const MGLFormatDesc *d = mglFormatDesc(f);
        bool integer = d->kind == MGL_FMT_COLOR_INT || d->kind == MGL_FMT_COLOR_UINT;
        bool legacy = mglLegacyFormatClass(f) >= 2;

        ERROR_CHECK_RETURN(legacy || (mglESColorRenderable(f) && !integer), GL_INVALID_OPERATION);
    }

    E->next.generate_mipmap(ctx, target);
}

static bool esDepthStencilFormat(GLenum internalformat)
{
    switch (internalformat)
    {
        case GL_DEPTH_COMPONENT16: case GL_DEPTH_COMPONENT24: case GL_DEPTH_COMPONENT32F:
        case GL_DEPTH24_STENCIL8: case GL_DEPTH32F_STENCIL8: case GL_STENCIL_INDEX8:
            return true;
    }

    return false;
}

static void esRenderbufferStorage(GLMContext ctx, GLenum target, GLenum internalformat, GLsizei width,
                                  GLsizei height)
{
    ERROR_CHECK_RETURN((mglESColorRenderable(internalformat) && internalformat != GL_RGBA &&
                        internalformat != GL_RGB) || esDepthStencilFormat(internalformat), GL_INVALID_ENUM);
    E->next.renderbuffer_storage(ctx, target, internalformat, width, height);
}

// ES 3.1 20.3.2: sample counts of a renderable format, and nothing else
static void esGetInternalformativ(GLMContext ctx, GLenum target, GLenum internalformat, GLenum pname, GLsizei count,
                                  GLint *params)
{
    ERROR_CHECK_RETURN(target == GL_RENDERBUFFER || target == GL_TEXTURE_2D_MULTISAMPLE ||
                       target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(pname == GL_SAMPLES || pname == GL_NUM_SAMPLE_COUNTS, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN((mglESColorRenderable(internalformat) && internalformat != GL_RGBA &&
                        internalformat != GL_RGB) || esDepthStencilFormat(internalformat), GL_INVALID_ENUM);
    E->next.get_internalformativ(ctx, target, internalformat, pname, count, params);
}

static void esRenderbufferStorageMultisample(GLMContext ctx, GLenum target, GLsizei samples, GLenum internalformat,
                                             GLsizei width, GLsizei height)
{
    ERROR_CHECK_RETURN((mglESColorRenderable(internalformat) && internalformat != GL_RGBA &&
                        internalformat != GL_RGB) || esDepthStencilFormat(internalformat), GL_INVALID_ENUM);
    E->next.renderbuffer_storage_multisample(ctx, target, samples, internalformat, width, height);
}

// ES calls the window's colour buffer GL_BACK, and has no other names for it
static void esGetFramebufferAttachmentParameteriv(GLMContext ctx, GLenum target, GLenum attachment, GLenum pname,
                                                  GLint *params)
{
    Framebuffer *fbo = target == GL_READ_FRAMEBUFFER ? ctx->state.readbuffer : ctx->state.framebuffer;

    if (fbo == NULL && (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER))
    {
        ERROR_CHECK_RETURN(attachment == GL_BACK || attachment == GL_DEPTH || attachment == GL_STENCIL,
                           GL_INVALID_ENUM);
        // the window's buffers have no object name
        ERROR_CHECK_RETURN(pname != GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, GL_INVALID_ENUM);

        if (attachment == GL_BACK)
            attachment = GL_BACK_LEFT;
    }

    E->next.get_framebuffer_attachment_parameteriv(ctx, target, attachment, pname, params);
}

// ES 3.0 2.14: the two any-samples targets count as one for "already active"
static void esBeginQuery(GLMContext ctx, GLenum target, GLuint id)
{
    GLint other = 0;

    if (target == GL_ANY_SAMPLES_PASSED)
        E->next.get_queryiv(ctx, GL_ANY_SAMPLES_PASSED_CONSERVATIVE, GL_CURRENT_QUERY, &other);
    else if (target == GL_ANY_SAMPLES_PASSED_CONSERVATIVE)
        E->next.get_queryiv(ctx, GL_ANY_SAMPLES_PASSED, GL_CURRENT_QUERY, &other);

    ERROR_CHECK_RETURN(other == 0, GL_INVALID_OPERATION);
    E->next.begin_query(ctx, target, id);
}

// ES 3.0 4.3.1: the window is GL_BACK, and there are no other names for a buffer
static void esReadBuffer(GLMContext ctx, GLenum src)
{
    ERROR_CHECK_RETURN(src == GL_BACK || src == GL_NONE ||
                       (src >= GL_COLOR_ATTACHMENT0 && src <= GL_COLOR_ATTACHMENT31), GL_INVALID_ENUM);
    E->next.read_buffer(ctx, src);
}

// ES 3.0 4.3.3: no blit from a buffer onto itself, and none into a
// multisampled framebuffer
static bool sameImage(const FBOAttachment *a, const FBOAttachment *b)
{
    bool rb = a->textarget == GL_RENDERBUFFER;

    return a->texture && b->texture && rb == (b->textarget == GL_RENDERBUFFER) &&
           (rb ? a->buf.rbo == b->buf.rbo : a->buf.tex == b->buf.tex) && a->level == b->level &&
           a->layer == b->layer;
}

// ES 3.0 4.3.3: a blit whose source and destination are the same buffer
static bool blitsOntoItself(GLMContext ctx, GLbitfield mask)
{
    Framebuffer *read = ctx->state.readbuffer, *draw = ctx->state.framebuffer;

    if (read == NULL || draw == NULL)
        return false;

    if ((mask & GL_COLOR_BUFFER_BIT) && read->read_buffer >= GL_COLOR_ATTACHMENT0 &&
        read->read_buffer < GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS)
    {
        const FBOAttachment *src = &read->color_attachments[read->read_buffer - GL_COLOR_ATTACHMENT0];

        for (GLsizei i = 0; i < draw->n_draw_buffers; i++)
        {
            GLenum b = draw->draw_buffers[i];

            if (b >= GL_COLOR_ATTACHMENT0 && b < GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS &&
                sameImage(src, &draw->color_attachments[b - GL_COLOR_ATTACHMENT0]))
                return true;
        }
    }

    return ((mask & GL_DEPTH_BUFFER_BIT) && sameImage(&read->depth, &draw->depth)) ||
           ((mask & GL_STENCIL_BUFFER_BIT) && sameImage(&read->stencil, &draw->stencil));
}

static void esBlitFramebuffer(GLMContext ctx, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
                              GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    Framebuffer *fbo = ctx->state.framebuffer;

    // after the plain argument errors, which come first
    if (!(mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) &&
        (filter == GL_NEAREST || filter == GL_LINEAR))
        ERROR_CHECK_RETURN(!blitsOntoItself(ctx, mask), GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(mglDrawFramebufferSamples(ctx) == 0, GL_INVALID_OPERATION);

    // out of a multisampled one, the same format and the same rectangle
    if (mglFramebufferSamples(ctx->state.readbuffer) && (mask & GL_COLOR_BUFFER_BIT))
    {
        GLenum rf = mglReadColorInternalFormat(ctx);
        Framebuffer *draw = ctx->state.framebuffer;

        ERROR_CHECK_RETURN(srcX0 == dstX0 && srcY0 == dstY0 && srcX1 == dstX1 && srcY1 == dstY1,
                           GL_INVALID_OPERATION);

        for (GLsizei i = 0; draw && i < draw->n_draw_buffers; i++)
        {
            GLenum b = draw->draw_buffers[i];
            FBOAttachment *a;
            Texture *tex;

            if (b < GL_COLOR_ATTACHMENT0 || b >= GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS)
                continue;

            a = &draw->color_attachments[b - GL_COLOR_ATTACHMENT0];
            tex = a->textarget == GL_RENDERBUFFER ? (a->buf.rbo ? a->buf.rbo->tex : NULL) : a->buf.tex;
            ERROR_CHECK_RETURN(tex == NULL || tex->internalformat == rf, GL_INVALID_OPERATION);
        }
    }

    if (fbo == ctx->state.readbuffer)
    {
        bool same = (mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) != 0 || fbo == NULL;

        for (GLsizei i = 0; fbo && !same && (mask & GL_COLOR_BUFFER_BIT) && i < fbo->n_draw_buffers; i++)
            same = fbo->draw_buffers[i] == fbo->read_buffer;

        ERROR_CHECK_RETURN(!same || !(mask & (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)),
                           GL_INVALID_OPERATION);
    }

    E->next.blit_framebuffer(ctx, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

// ES 3.1 9.2.1: no layers here, and no samples
static bool esFramebufferPname(GLenum pname)
{
    return pname == GL_FRAMEBUFFER_DEFAULT_WIDTH || pname == GL_FRAMEBUFFER_DEFAULT_HEIGHT ||
           pname == GL_FRAMEBUFFER_DEFAULT_SAMPLES || pname == GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS;
}

static void esFramebufferParameteri(GLMContext ctx, GLenum target, GLenum pname, GLint param)
{
    ERROR_CHECK_RETURN(esFramebufferPname(pname), GL_INVALID_ENUM);
    E->next.framebuffer_parameteri(ctx, target, pname, param);
}

static void esGetFramebufferParameteriv(GLMContext ctx, GLenum target, GLenum pname, GLint *params)
{
    ERROR_CHECK_RETURN(esFramebufferPname(pname), GL_INVALID_ENUM);
    E->next.get_framebuffer_parameteriv(ctx, target, pname, params);
}

// --- multisample textures ---

// ES 3.1 8.8: no array target here, and the format has to be renderable;
// the array form is GL_OES_texture_storage_multisample_2d_array
static void esTexStorage2DMultisample(GLMContext ctx, GLenum target, GLsizei samples, GLenum internalformat,
                                      GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
    ERROR_CHECK_RETURN(target == GL_TEXTURE_2D_MULTISAMPLE, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(mglESColorRenderable(internalformat) || esDepthStencilFormat(internalformat),
                       GL_INVALID_ENUM);
    E->next.tex_storage2_d_multisample(ctx, target, samples, internalformat, width, height, fixedsamplelocations);
}

static void esTexStorage3DMultisample(GLMContext ctx, GLenum target, GLsizei samples, GLenum internalformat,
                                      GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
    ERROR_CHECK_RETURN(target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(mglESColorRenderable(internalformat) || esDepthStencilFormat(internalformat),
                       GL_INVALID_ENUM);
    E->next.tex_storage3_d_multisample(ctx, target, samples, internalformat, width, height, depth,
                                       fixedsamplelocations);
}

// --- compressed textures ---

// ES 3.0 3.8.6: a format that is not compressed cannot match the image
static void esCompressedTexSubImage2D(GLMContext ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                      GLsizei width, GLsizei height, GLenum format, GLsizei imageSize,
                                      const void *data)
{
    ERROR_CHECK_RETURN(mglFormatIsCompressed(format), GL_INVALID_OPERATION);
    E->next.compressed_tex_sub_image2D(ctx, target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

static void esCompressedTexSubImage3D(GLMContext ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                      GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format,
                                      GLsizei imageSize, const void *data)
{
    ERROR_CHECK_RETURN(mglFormatIsCompressed(format), GL_INVALID_OPERATION);
    E->next.compressed_tex_sub_image3D(ctx, target, level, xoffset, yoffset, zoffset, width, height, depth, format,
                                       imageSize, data);
}

// GL_EXT_buffer_storage's entry point
void glBufferStorageEXT(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags)
{
    glBufferStorage(target, size, data, flags);
}

// GL_OES_texture_storage_multisample_2d_array's entry point
void glTexStorage3DMultisampleOES(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                                  GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
    glTexStorage3DMultisample(target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

// --- program pipelines ---

static bool word(const char *s, size_t n, const char *w)
{
    return strlen(w) == n && !strncmp(s, w, n);
}

static char scanVarying(const char *src, const char *name, bool fragment, bool *located)
{
    char def_float = fragment ? 0 : 'h', def_int = fragment ? 'm' : 'h';
    const char *p = src;
    int depth = 0;

    if (src == NULL || name == NULL)
        return 0;

    // one global statement at a time, ending at a ';' outside braces
    while (*p)
    {
        const char *w[32];
        size_t wl[32];
        int n = 0;

        for (; *p && !(*p == ';' && depth == 0); p++)
        {
            if (p[0] == '/' && p[1] == '/')
            {
                while (*p && *p != '\n') p++;
                if (!*p) break;
                continue;
            }

            if (p[0] == '/' && p[1] == '*')
            {
                const char *e = strstr(p + 2, "*/");
                p = e ? e + 1 : p + strlen(p) - 1;
                continue;
            }

            if (*p == '#')
            {
                while (*p && *p != '\n') p++;
                if (!*p) break;
                continue;
            }

            if (*p == '{') depth++;
            if (*p == '}') { depth--; n = 0; continue; }

            if (depth == 0 && (isalpha((unsigned char)*p) || *p == '_') && n < 32)
            {
                const char *b = p;

                while (isalnum((unsigned char)p[1]) || p[1] == '_') p++;
                w[n] = b;
                wl[n++] = (size_t)(p - b + 1);
            }
        }

        if (n >= 3 && word(w[0], wl[0], "precision"))
        {
            char q = word(w[1], wl[1], "highp") ? 'h' : word(w[1], wl[1], "mediump") ? 'm' : 'l';

            if (word(w[2], wl[2], "float")) def_float = q;
            if (word(w[2], wl[2], "int")) def_int = q;
        }
        else
        {
            bool io = false, named = false, integer = false, loc = false;
            char q = 0;

            for (int i = 0; i < n; i++)
            {
                io |= word(w[i], wl[i], "in") || word(w[i], wl[i], "out");
                named |= word(w[i], wl[i], name);
                integer |= word(w[i], wl[i], "int") || word(w[i], wl[i], "uint") ||
                           (wl[i] == 5 && (!strncmp(w[i], "ivec", 4) || !strncmp(w[i], "uvec", 4)));

                if (word(w[i], wl[i], "highp")) q = 'h';
                if (word(w[i], wl[i], "mediump")) q = 'm';
                if (word(w[i], wl[i], "lowp")) q = 'l';
                loc |= word(w[i], wl[i], "location");
            }

            if (io && named)
            {
                if (located)
                    *located = loc;
                return q ? q : integer ? def_int : def_float;
            }
        }

        if (*p) p++;
    }

    return 0;
}

char mglESVaryingPrecision(const char *src, const char *name, bool fragment)
{
    return scanVarying(src, name, fragment, NULL);
}

static bool locatedInSource(const char *src, const char *name)
{
    bool located = false;

    scanVarying(src, name, false, &located);
    return located;
}

extern ProgramPipeline *findProgramPipeline(GLMContext ctx, GLuint pipeline);

static const SpirvResource *findVarying(const SpirvResourceList *list, const SpirvResource *v)
{
    for (GLuint i = 0; i < list->count; i++)
        if (v->name && list->list[i].name && !strcmp(v->name, list->list[i].name))
            return &list->list[i];

    // not by name, so by a layout(location) both sides gave
    for (GLuint i = 0; i < list->count; i++)
        if (v->location != MGL_NO_LOCATION && list->list[i].location == v->location)
            return &list->list[i];

    return NULL;
}

// ES 3.1 7.4.1: separate programs meet only if the outputs of one are the
// inputs of the next, each with the same type and precision
static bool interfaceMatches(Program *out, Program *in)
{
    const SpirvResourceList *o = &out->spirv_resources_list[_VERTEX_SHADER][SPVC_RESOURCE_TYPE_STAGE_OUTPUT];
    const SpirvResourceList *i = &in->spirv_resources_list[_FRAGMENT_SHADER][SPVC_RESOURCE_TYPE_STAGE_INPUT];

    if (o->count != i->count)
        return false;

    for (GLuint a = 0; a < o->count; a++)
    {
        const SpirvResource *match = findVarying(i, &o->list[a]);

        if (match == NULL || match->gl_type != o->list[a].gl_type ||
            match->array_size != o->list[a].array_size || match->flat != o->list[a].flat ||
            (match->precision && o->list[a].precision && match->precision != o->list[a].precision))
            return false;

        if (locatedInSource(out->stage_src[_VERTEX_SHADER], o->list[a].name) !=
            locatedInSource(in->stage_src[_FRAGMENT_SHADER], match->name))
            return false;
    }

    return true;
}

extern Program *findProgram(GLMContext ctx, GLuint program);

typedef struct { const char *s; size_t n; } Token;

// the source as words and single punctuation marks, without comments or
// preprocessor lines; the caller frees the list
static Token *tokenize(const char *src, int *count)
{
    size_t cap = 256;
    Token *t = malloc(cap * sizeof(Token));
    int n = 0;

    for (const char *p = src; t && p && *p;)
    {
        if (p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') p++; continue; }
        if (p[0] == '/' && p[1] == '*') { const char *e = strstr(p + 2, "*/"); p = e ? e + 2 : p + strlen(p); continue; }
        if (*p == '#') { while (*p && *p != '\n') p++; continue; }
        if (isspace((unsigned char)*p)) { p++; continue; }

        if ((size_t)n == cap)
        {
            Token *g = realloc(t, (cap *= 2) * sizeof(Token));
            if (!g) { free(t); return NULL; }
            t = g;
        }

        t[n].s = p;
        if (isalnum((unsigned char)*p) || *p == '_')
            while (isalnum((unsigned char)*p) || *p == '_') p++;
        else
            p++;
        t[n].n = (size_t)(p - t[n].s);
        n++;
    }

    *count = n;
    return t;
}

static bool is(const Token *t, const char *w)
{
    return word(t->s, t->n, w);
}

#define BLOCK_MEMBERS 64

// what two stages must agree on about a block: its packing, and which of its
// array members are unsized
typedef struct {
    char packing[8];
    int members;
    Token member[BLOCK_MEMBERS];
    bool unsized[BLOCK_MEMBERS];
} BlockShape;

static void packingIn(const Token *t, int from, int to, char *out)
{
    for (int i = from; i < to; i++)
        if (is(&t[i], "shared") || is(&t[i], "packed") || is(&t[i], "std140") || is(&t[i], "std430"))
            snprintf(out, 8, "%.*s", (int)t[i].n, t[i].s);
}

// the block called name, declared with storage ("uniform" or "buffer")
static bool blockShape(const char *src, const char *storage, const char *name, BlockShape *b)
{
    int n = 0;
    Token *t = tokenize(src, &n);
    char def[8] = "shared";
    bool found = false;

    if (t == NULL)
        return false;

    for (int i = 0; i < n && !found; i++)
    {
        int lay = -1, close = -1;

        if (!is(&t[i], storage) || (i > 0 && is(&t[i - 1], "(")))
            continue;

        // a layout(...) just before
        if (i > 0 && is(&t[i - 1], ")"))
            for (int j = i - 2, d = 1; j >= 1; j--)
            {
                d += is(&t[j], ")") - is(&t[j], "(");
                if (d == 0) { if (is(&t[j - 1], "layout")) { lay = j; close = i - 1; } break; }
            }

        // layout(...) uniform; sets the default
        if (i + 1 < n && is(&t[i + 1], ";"))
        {
            if (lay >= 0)
                packingIn(t, lay, close, def);
            continue;
        }

        if (i + 2 >= n || !is(&t[i + 1], name) || !is(&t[i + 2], "{"))
            continue;

        memset(b, 0, sizeof(*b));
        strcpy(b->packing, def);
        if (lay >= 0)
            packingIn(t, lay, close, b->packing);

        // members: the word before a ';' or '[', and whether "[]" follows it
        for (int j = i + 3, d = 0; j < n && b->members < BLOCK_MEMBERS; j++)
        {
            if (is(&t[j], "{")) d++;
            if (is(&t[j], "}")) { if (d-- == 0) break; }
            if (d == 0 && (is(&t[j], ";") || is(&t[j], "[") || is(&t[j], ",")) &&
                (isalpha((unsigned char)*t[j - 1].s) || *t[j - 1].s == '_') && !is(&t[j - 1], "]"))
            {
                b->member[b->members] = t[j - 1];
                b->unsized[b->members] = is(&t[j], "[") && j + 1 < n && is(&t[j + 1], "]");
                b->members++;
            }
        }

        found = true;
    }

    free(t);
    return found;
}

// ES 3.1 7.6.2: a block two stages share has the same packing and members
static const char *blockMismatch(const char *a, const char *b, const SpirvResourceList *list, const char *storage)
{
    for (GLuint i = 0; i < list->count; i++)
    {
        BlockShape x, y;
        char name[128];

        if (!list->list[i].name)
            continue;

        snprintf(name, sizeof(name), "%.*s", (int)strcspn(list->list[i].name, "["), list->list[i].name);

        if (!blockShape(a, storage, name, &x) || !blockShape(b, storage, name, &y))
            continue;

        if (strcmp(x.packing, y.packing))
            return "a block has a different layout in each stage\n";

        for (int m = 0; m < x.members; m++)
            for (int k = 0; k < y.members; k++)
                if (x.member[m].n == y.member[k].n && !strncmp(x.member[m].s, y.member[k].s, x.member[m].n) &&
                    x.unsized[m] != y.unsized[k])
                    return "a block member is sized in one stage and not in the other\n";
    }

    return NULL;
}

// ES 3.0 4.3.9: an output and the input it feeds agree on flat
static void esLinkProgram(GLMContext ctx, GLuint program)
{
    Program *p;

    E->next.link_program(ctx, program);

    p = findProgram(ctx, program);

    if (p == NULL || !p->link_status || !p->shader_slots[_VERTEX_SHADER] || !p->shader_slots[_FRAGMENT_SHADER])
        return;

    const SpirvResourceList *o = &p->spirv_resources_list[_VERTEX_SHADER][SPVC_RESOURCE_TYPE_STAGE_OUTPUT];
    const SpirvResourceList *in = &p->spirv_resources_list[_FRAGMENT_SHADER][SPVC_RESOURCE_TYPE_STAGE_INPUT];
    const char *vsrc = p->shader_slots[_VERTEX_SHADER]->src, *fsrc = p->shader_slots[_FRAGMENT_SHADER]->src;
    const char *why = NULL;

    if (vsrc && fsrc)
    {
        why = blockMismatch(vsrc, fsrc, &p->spirv_resources_list[_VERTEX_SHADER][SPVC_RESOURCE_TYPE_UNIFORM_BUFFER],
                            "uniform");
        if (why == NULL)
            why = blockMismatch(vsrc, fsrc,
                                &p->spirv_resources_list[_VERTEX_SHADER][SPVC_RESOURCE_TYPE_STORAGE_BUFFER], "buffer");
    }

    if (why)
    {
        p->link_status = GL_FALSE;
        free(p->log);
        p->log = strdup(why);
        return;
    }

    for (GLuint a = 0; a < in->count; a++)
    {
        const SpirvResource *match = findVarying(o, &in->list[a]);

        if (match && match->flat != in->list[a].flat)
        {
            p->link_status = GL_FALSE;
            free(p->log);
            p->log = strdup("a varying is flat in one stage and not in the other\n");
            return;
        }
    }
}

bool mglPipelineLinks(GLMContext ctx, ProgramPipeline *pp);

static bool pipelineInterfacesMatch(GLMContext ctx, ProgramPipeline *pp)
{
    Program *vs = pp->stage_programs[_VERTEX_SHADER], *fs = pp->stage_programs[_FRAGMENT_SHADER];

    if (vs == NULL || fs == NULL || vs == fs)
        return true;

    return interfaceMatches(vs, fs) && mglPipelineLinks(ctx, pp);
}

static void esValidateProgramPipeline(GLMContext ctx, GLuint pipeline)
{
    ProgramPipeline *pp;

    E->next.validate_program_pipeline(ctx, pipeline);

    pp = findProgramPipeline(ctx, pipeline);

    if (pp && pp->validated && !pipelineInterfacesMatch(ctx, pp))
        pp->validated = GL_FALSE;
}

// ES 3.1 11.1.3.11: drawing with a pipeline whose stages don't meet is an error
static bool esPipelineDrawable(GLMContext ctx)
{
    ProgramPipeline *pp = STATE(program_pipeline);

    return STATE(program) || pp == NULL || pipelineInterfacesMatch(ctx, pp);
}

// ES 3.1 7.3.1: a varying between two stages, which a separable program
// can have, sits at -1 unless layout(location) placed it
static bool unplacedVarying(GLMContext ctx, GLuint program, GLenum iface, const char *name)
{
    Program *p = findProgram(ctx, program);
    int stage = -1;
    char base[256];

    if (p == NULL || name == NULL || (iface != GL_PROGRAM_INPUT && iface != GL_PROGRAM_OUTPUT))
        return false;

    for (int s = _VERTEX_SHADER; s <= _FRAGMENT_SHADER; s++)
        if (p->stage_src[s] && (stage < 0 || iface == GL_PROGRAM_OUTPUT))
            stage = s;

    if (stage < 0 || (iface == GL_PROGRAM_INPUT && stage == _VERTEX_SHADER) ||
        (iface == GL_PROGRAM_OUTPUT && stage == _FRAGMENT_SHADER) || !strncmp(name, "gl_", 3))
        return false;

    snprintf(base, sizeof(base), "%.*s", (int)strcspn(name, ".["), name);
    return !locatedInSource(p->stage_src[stage], base);
}

static void esGetProgramResourceiv(GLMContext ctx, GLuint program, GLenum iface, GLuint index, GLsizei propCount,
                                   const GLenum *props, GLsizei count, GLsizei *length, GLint *params)
{
    GLsizei n = 0;
    char name[256] = { 0 };

    E->next.get_program_resourceiv(ctx, program, iface, index, propCount, props, count, &n, params);

    if (length)
        *length = n;

    for (GLsizei i = 0; i < n && i < propCount && params; i++)
    {
        if (props[i] != GL_LOCATION)
            continue;

        E->next.get_program_resource_name(ctx, program, iface, index, sizeof(name), NULL, name);
        if (unplacedVarying(ctx, program, iface, name))
            params[i] = -1;
    }
}

static GLint esGetProgramResourceLocation(GLMContext ctx, GLuint program, GLenum iface, const GLchar *name)
{
    GLint loc = E->next.get_program_resource_location(ctx, program, iface, name);

    return loc >= 0 && unplacedVarying(ctx, program, iface, name) ? -1 : loc;
}

// --- images ---

extern Texture *findTexture(GLMContext ctx, GLuint texture);

// ES 3.1 8.22: only an immutable texture can be bound to an image unit
static void esBindImageTexture(GLMContext ctx, GLuint unit, GLuint texture, GLint level, GLboolean layered,
                               GLint layer, GLenum access, GLenum format)
{
    Texture *t = texture ? findTexture(ctx, texture) : NULL;

    if (t && !t->immutable_storage && unit < (GLuint)STATE_VAR(max_image_units) && level >= 0 && layer >= 0)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    E->next.bind_image_texture(ctx, unit, texture, level, layered, layer, access, format);
}

// --- copying from the framebuffer ---

// normalized, sRGB, signed normalized, float, int or unsigned int
static char formatClass(GLenum f)
{
    const MGLFormatDesc *d = mglFormatDesc(f);

    if (d->kind == MGL_FMT_COLOR_INT)
        return 'i';
    if (d->kind == MGL_FMT_COLOR_UINT)
        return 'u';
    if (floatFormat(f))
        return 'f';
    if (d->upload_type == GL_BYTE || d->upload_type == GL_SHORT)
        return 's';
    return d->srgb ? 'S' : 'n';
}

// ES 3.0 3.8.5: a copy can drop channels but not add them, keeps the kind of
// value, and a sized format keeps each channel's size
static bool esCopyFits(GLenum src, GLenum dst, bool exact);

static bool esCopyAllowed(GLenum src, GLenum dst)
{
    return esCopyFits(src, dst, true);
}

static bool esCopyFits(GLenum src, GLenum dst, bool exact)
{
    const MGLFormatDesc *s = mglFormatDesc(src), *d;
    bool r = s->bits[0], g = s->bits[1], b = s->bits[2], a = s->bits[3];
    bool plain = formatClass(src) == 'n';

    switch (dst)
    {
        // table 3.12 has no unsized format for a 10 bit source
        case GL_RGBA:            return plain && r && g && b && a && src != GL_RGB10_A2;
        case GL_RGB:             return plain && r && g && b && src != GL_RGB10_A2;
        case GL_LUMINANCE_ALPHA: return plain && r && a && src != GL_RGB10_A2;
        case GL_LUMINANCE:       return plain && r && src != GL_RGB10_A2;
        case GL_ALPHA:           return plain && a && src != GL_RGB10_A2;
    }

    d = mglFormatDesc(dst);

    // a shared exponent format is never a copy destination
    if (dst == GL_RGB9_E5)
        return false;

    if (d->gl_format == 0 || d->kind == MGL_FMT_DEPTH || d->kind == MGL_FMT_STENCIL ||
        d->kind == MGL_FMT_DEPTH_STENCIL || formatClass(dst) != formatClass(src))
        return false;

    for (int c = 0; c < 4; c++)
        if (d->bits[c] && (!s->bits[c] || (exact && d->bits[c] != s->bits[c])))
            return false;

    return true;
}

static void esCopyTexImage2D(GLMContext ctx, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
                             GLsizei width, GLsizei height, GLint border)
{
    ERROR_CHECK_RETURN(internalformat != 0 && mglFormatDesc(internalformat)->gl_format != 0, GL_INVALID_ENUM);

    // an incomplete framebuffer is the desktop path's error to raise
    if (mglCheckFramebufferStatus(ctx, GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
    {
        GLenum src = mglReadColorInternalFormat(ctx);

        if (src == 0)
            src = GL_RGBA8;

        ERROR_CHECK_RETURN(esCopyAllowed(src, internalformat), GL_INVALID_OPERATION);

        // ES 3.0 table 3.12: an unsized format takes its sizes from the source
        switch (internalformat)
        {
            case GL_RGBA:
                internalformat = src == GL_RGBA4 || src == GL_RGB5_A1 ? src : GL_RGBA8;
                break;
            case GL_RGB:
                internalformat = mglFormatDesc(src)->bits[0] <= 5 ? GL_RGB565 : GL_RGB8;
                break;
            case GL_LUMINANCE_ALPHA: internalformat = GL_LUMINANCE8_ALPHA8; break;
            case GL_LUMINANCE:       internalformat = GL_LUMINANCE8; break;
            case GL_ALPHA:           internalformat = GL_ALPHA8; break;
        }
    }

    E->next.copy_tex_image2D(ctx, target, level, internalformat, x, y, width, height, border);
}

// ES 3.0 3.8.5: copying into part of a texture follows the same rules
static bool esCopyIntoAllowed(GLMContext ctx, GLenum target)
{
    Texture *tex = currentTexture(ctx, textureIndexFromTarget(ctx, target));
    GLenum src;

    if (tex == NULL || tex->internalformat == 0 ||
        mglCheckFramebufferStatus(ctx, GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return true;

    src = mglReadColorInternalFormat(ctx);
    return esCopyFits(src ? src : GL_RGBA8, (GLenum)tex->internalformat, false);
}

static void esCopyTexSubImage2D(GLMContext ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x,
                                GLint y, GLsizei width, GLsizei height)
{
    ERROR_CHECK_RETURN(esCopyIntoAllowed(ctx, target), GL_INVALID_OPERATION);
    E->next.copy_tex_sub_image2D(ctx, target, level, xoffset, yoffset, x, y, width, height);
}

static void esCopyTexSubImage3D(GLMContext ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    ERROR_CHECK_RETURN(esCopyIntoAllowed(ctx, target), GL_INVALID_OPERATION);
    E->next.copy_tex_sub_image3D(ctx, target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

// --- shaders ---

extern Program *findProgram(GLMContext ctx, GLuint program);
extern Shader *findShader(GLMContext ctx, GLuint shader);

// ES 3.0 7.3: one shader of each type per program
static void esAttachShader(GLMContext ctx, GLuint program, GLuint shader)
{
    Program *p = findProgram(ctx, program);
    Shader *sh = findShader(ctx, shader);

    if (p && sh && p->shader_slots[sh->glm_type] && p->shader_slots[sh->glm_type] != sh)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    E->next.attach_shader(ctx, program, shader);
}

static GLuint esCreateShader(GLMContext ctx, GLenum type)
{
    ERROR_CHECK_RETURN_VALUE(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER || type == GL_COMPUTE_SHADER,
                             GL_INVALID_ENUM, 0);
    return E->next.create_shader(ctx, type);
}

// The #version line: 100 when there is none. Sets *line to it, or NULL.
static int shaderVersion(const char *src, bool *es, const char **line)
{
    const char *p = src;

    *es = false;
    *line = NULL;

    // comments and white space may come first
    for (;;)
    {
        while (isspace((unsigned char)*p))
            p++;

        if (p[0] == '/' && p[1] == '/')
        {
            p = strchr(p, '\n');
            if (p == NULL)
                return 100;
        }
        else if (p[0] == '/' && p[1] == '*')
        {
            p = strstr(p + 2, "*/");
            if (p == NULL)
                return 100;
            p += 2;
        }
        else
            break;
    }

    if (*p != '#')
        return 100;

    const char *q = p + 1;

    while (*q == ' ' || *q == '\t')
        q++;

    if (strncmp(q, "version", 7))
        return 100;

    q += 7;

    int version = atoi(q);
    const char *eol = strchr(q, '\n');
    size_t n = eol ? (size_t)(eol - q) : strlen(q);

    *es = memmem(q, n, " es", 3) != NULL;
    *line = p;

    return version;
}

static glslang_stage_t esStage(GLenum type)
{
    switch (type)
    {
        case GL_FRAGMENT_SHADER: return GLSLANG_STAGE_FRAGMENT;
        case GL_COMPUTE_SHADER: return GLSLANG_STAGE_COMPUTE;
        default: return GLSLANG_STAGE_VERTEX;
    }
}

// __VERSION__ as the shader's own version, where it is compiled as another;
// padded in place so line and column numbers stay
static void pinVersion(char *s, const char *version)
{
    for (char *p = strstr(s, "__VERSION__"); p; p = strstr(p + 1, "__VERSION__"))
    {
        bool start = p == s || !(isalnum((unsigned char)p[-1]) || p[-1] == '_');
        bool end = !(isalnum((unsigned char)p[11]) || p[11] == '_');

        if (start && end)
        {
            memset(p, ' ', 11);
            memcpy(p, version, strlen(version));
        }
    }
}

// GLSL ES 3.00 3.4: #extension has to come before anything that is not a
// preprocessor directive
static bool extensionAfterCode(const char *src)
{
    bool code = false, comment = false;

    for (const char *line = src; line && *line;)
    {
        const char *eol = strchr(line, '\n');
        const char *end = eol ? eol : line + strlen(line);
        const char *p = line;

        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r'))
            p++;

        if (!comment && p < end && *p == '#')
        {
            p++;
            while (p < end && (*p == ' ' || *p == '\t'))
                p++;
            if (code && end - p >= 9 && !strncmp(p, "extension", 9))
                return true;
        }
        else
        {
            for (; p < end; p++)
            {
                if (comment)
                {
                    if (p[0] == '*' && p + 1 < end && p[1] == '/')
                        comment = false, p++;
                }
                else if (p[0] == '/' && p + 1 < end && p[1] == '/')
                    break;
                else if (p[0] == '/' && p + 1 < end && p[1] == '*')
                    comment = true, p++;
                else if (!isspace((unsigned char)*p))
                    code = true;
            }
        }

        line = eol ? eol + 1 : NULL;
    }

    return false;
}

// glslang under the GLSL ES rules alone; NULL, or the info log
static char *esValidate(GLMContext ctx, const char *src, GLenum type)
{
    glslang_input_t input = {
        .language = GLSLANG_SOURCE_GLSL,
        .stage = esStage(type),
        .client = GLSLANG_CLIENT_NONE,
        .target_language = GLSLANG_TARGET_NONE,
        .code = src,
        .default_version = 100,
        .default_profile = GLSLANG_ES_PROFILE,
        .messages = GLSLANG_MSG_DEFAULT_BIT,
        .resource = (const glslang_resource_t *)mglGlslangResource(ctx),
    };
    glslang_shader_t *shader = glslang_shader_create(&input);
    char *log = NULL;

    if (shader == NULL)
        return strdup("ERROR: the shader could not be parsed\n");

    if (!glslang_shader_preprocess(shader, &input) || !glslang_shader_parse(shader, &input))
    {
        const char *info = glslang_shader_get_info_log(shader);

        log = strdup(info && *info ? info : "ERROR: the shader did not compile\n");
    }

    glslang_shader_delete(shader);

    return log;
}

char *mglESShaderSource(GLMContext ctx, const char *src, GLenum type, char **out)
{
    const char *line;
    bool es;
    int version = shaderVersion(src, &es, &line);
    char *log;

    *out = NULL;

    if (!((version == 100 && !es) || (es && (version == 300 || version == 310))))
        return strdup("ERROR: 0:1: '#version' : not a version OpenGL ES " STR(MGL_ES_MAJOR) "."
                      STR(MGL_ES_MINOR) " takes\n");

    if (version >= 300 && extensionAfterCode(src))
        return strdup("ERROR: '#extension' : must come before any code\n");

    log = esValidate(ctx, src, type);
    if (log)
        return log;

    // glslang makes SPIR-V from ES 3.10 and up only
    if (version == 300)
    {
        char *copy = strdup(src);
        char *digits;

        if (copy == NULL)
            return strdup("ERROR: out of memory\n");

        digits = strstr(copy + (line - src), "300");
        if (digits)
            digits[1] = '1';

        pinVersion(copy, "300");
        *out = copy;
    }
    else if (version == 100)
    {
#ifdef MGL_COMPAT_CONTEXTS
        *out = mglCompatRewriteGLSL(src, type);
        if (*out)
            pinVersion(*out, "100");
#else
        return strdup("ERROR: GLSL ES 1.00 needs the compatibility profile in this build\n");
#endif
    }

    return NULL;
}

// --- set up ---

void mglESInit(GLMContext ctx)
{
    ctx->es = (MGLES *)calloc(1, sizeof(MGLES));

    if (ctx->es == NULL)
        return;

    E->next = ctx->dispatch;

    // always on in ES, and not an enable there
    ctx->state.caps.texture_cube_map_seamless = true;
    ctx->state.caps.program_point_size = true;
    ctx->state.caps.framebuffer_srgb = true;

    ctx->dispatch.get_string = esGetString;
    ctx->dispatch.get_stringi = esGetStringi;
    ctx->dispatch.get_integerv = esGetIntegerv;
    ctx->dispatch.get_integer64v = esGetInteger64v;
    ctx->dispatch.get_floatv = esGetFloatv;
    ctx->dispatch.get_booleanv = esGetBooleanv;
    ctx->dispatch.enable = esEnable;
    ctx->dispatch.disable = esDisable;
    ctx->dispatch.is_enabled = esIsEnabled;
    ctx->dispatch.create_shader = esCreateShader;
    ctx->dispatch.read_pixels = esReadPixels;
    ctx->dispatch.copy_tex_image2D = esCopyTexImage2D;
    ctx->dispatch.copy_tex_sub_image2D = esCopyTexSubImage2D;
    ctx->dispatch.copy_tex_sub_image3D = esCopyTexSubImage3D;
    ctx->dispatch.blit_framebuffer = esBlitFramebuffer;
    ctx->dispatch.read_buffer = esReadBuffer;
    ctx->dispatch.link_program = esLinkProgram;
    ctx->dispatch.begin_query = esBeginQuery;
    ctx->dispatch.get_internalformativ = esGetInternalformativ;
    ctx->dispatch.get_framebuffer_attachment_parameteriv = esGetFramebufferAttachmentParameteriv;
    ctx->dispatch.tex_image2D = esTexImage2D;
    ctx->dispatch.tex_image3D = esTexImage3D;
    ctx->dispatch.renderbuffer_storage = esRenderbufferStorage;
    ctx->dispatch.renderbuffer_storage_multisample = esRenderbufferStorageMultisample;
    ctx->dispatch.draw_arrays = esDrawArrays;
    ctx->dispatch.draw_elements = esDrawElements;
    ctx->dispatch.draw_range_elements = esDrawRangeElements;
    ctx->dispatch.draw_arrays_instanced = esDrawArraysInstanced;
    ctx->dispatch.draw_elements_instanced = esDrawElementsInstanced;
    ctx->dispatch.draw_arrays_indirect = esDrawArraysIndirect;
    ctx->dispatch.draw_elements_indirect = esDrawElementsIndirect;
    ctx->dispatch.bind_vertex_buffer = esBindVertexBuffer;
    ctx->dispatch.vertex_attrib_format = esVertexAttribFormat;
    ctx->dispatch.vertex_attrib_i_format = esVertexAttribIFormat;
    ctx->dispatch.vertex_attrib_binding = esVertexAttribBinding;
    ctx->dispatch.vertex_binding_divisor = esVertexBindingDivisor;
    ctx->dispatch.attach_shader = esAttachShader;
    ctx->dispatch.framebuffer_parameteri = esFramebufferParameteri;
    ctx->dispatch.get_framebuffer_parameteriv = esGetFramebufferParameteriv;
    ctx->dispatch.tex_storage2_d_multisample = esTexStorage2DMultisample;
    ctx->dispatch.tex_storage3_d_multisample = esTexStorage3DMultisample;
    ctx->dispatch.compressed_tex_sub_image2D = esCompressedTexSubImage2D;
    ctx->dispatch.compressed_tex_sub_image3D = esCompressedTexSubImage3D;
    ctx->dispatch.bind_image_texture = esBindImageTexture;
    ctx->dispatch.validate_program_pipeline = esValidateProgramPipeline;
    ctx->dispatch.get_program_resourceiv = esGetProgramResourceiv;
    ctx->dispatch.generate_mipmap = esGenerateMipmap;
    ctx->dispatch.get_program_resource_location = esGetProgramResourceLocation;

    for (GLuint i = 0; i < TEXTURE_UNITS; i++)
        mglResetImageUnit(ctx, i);
}

void mglESFree(GLMContext ctx)
{
    if (ctx->es == NULL)
        return;

    free(E->extension_string);
    free(ctx->es);
    ctx->es = NULL;
}

#endif /* MGL_OGL_ES */
