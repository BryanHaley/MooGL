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
 * compat_attrib.c
 * MGL
 *
 * glPushAttrib and glPopAttrib, and the client-side pair. A push copies the
 * state; a pop puts back only the groups the push named. Bindings are kept
 * by name, so an object deleted in between is not brought back.
 */

#include <stdlib.h>
#include <string.h>

#include "glm_context.h"
#include "compat.h"
#include "hash_table.h"

#ifdef MGL_COMPAT_CONTEXTS

#define C (ctx->compat)

void mglActiveTexture(GLMContext ctx, GLenum texture);
void mglBindTexture(GLMContext ctx, GLenum target, GLuint texture);
void mglBindBuffer(GLMContext ctx, GLenum target, GLuint buffer);

static const struct { int index; GLenum target; } unit_targets[] = {
    { _TEXTURE_1D, GL_TEXTURE_1D },
    { _TEXTURE_2D, GL_TEXTURE_2D },
    { _TEXTURE_3D, GL_TEXTURE_3D },
    { _TEXTURE_CUBE_MAP, GL_TEXTURE_CUBE_MAP },
    { _TEXTURE_RECTANGLE, GL_TEXTURE_RECTANGLE },
};

#define UNIT_TARGETS 5

struct MGLAttribFrame_t {
    GLbitfield mask;

    GLMParams var;
    GLMCaps caps;
    ViewportRect viewport[MAX_VIEWPORTS];
    ScissorRect scissor[MAX_VIEWPORTS];
    DepthRangeVal depth_range[MAX_VIEWPORTS];
    GLfloat color_clear[4];
    AttribConstant attrib_constant[MAX_ATTRIBS];
    GLuint active_texture;
    GLuint tex_names[MGL_COMPAT_UNITS][UNIT_TARGETS];

    GLboolean edge_flag;
    GLfloat raster_pos[4], raster_color[4], raster_secondary[4], raster_tex[4], raster_distance;
    GLboolean raster_valid;

    GLboolean point_smooth, point_sprite;
    GLenum sprite_origin;
    GLboolean line_stipple;
    GLint stipple_factor;
    GLushort stipple_pattern;
    GLenum polygon_mode[2];
    GLboolean polygon_stipple;
    GLubyte polygon_stipple_mask[128];
    GLfloat zoom_x, zoom_y, pixel_scale[4], pixel_bias[4];

    GLboolean lighting;
    GLbitfield lights_on;
    MGLLight lights[MGL_COMPAT_LIGHTS];
    MGLMaterial material[2];
    GLfloat model_ambient[4];
    GLboolean local_viewer, two_side;
    GLenum color_control;
    GLboolean color_material;
    GLenum color_material_face, color_material_mode, shade_model;

    GLboolean fog;
    GLenum fog_mode, fog_coord_src;
    GLfloat fog_color[4], fog_density, fog_start, fog_end, fog_index;

    GLfloat accum_clear[4];
    GLenum matrix_mode;
    GLfloat clip_planes[MGL_COMPAT_PLANES][4];
    GLboolean normalize, rescale_normal, color_sum, auto_normal;
    GLbitfield map1_on, map2_on;
    GLint grid1_n, grid2_n[2];
    GLfloat grid1[2], grid2[4];
    GLenum perspective_hint, point_hint, line_hint, polygon_hint, fog_hint;
    GLuint list_base;
    MGLTexEnv tex[MGL_COMPAT_UNITS];
};

struct MGLClientFrame_t {
    GLbitfield mask;
    PixelStore pack, unpack;
    VertexArray *vao;
    GLuint vao_name;
    VertexArray arrays;
    GLuint buffer_names[MAX_BINDABLE_BUFFERS];
    GLuint element_name;
    GLuint array_buffer;
    GLuint client_active;
    GLboolean edge_flag_array;
    const void *edge_flag_pointer;
    GLsizei edge_flag_stride;
};

static void cPushAttrib(GLMContext ctx, GLbitfield mask)
{
    MGLAttribFrame *f;

    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(C->attrib_depth < MGL_ATTRIB_DEPTH, GL_STACK_OVERFLOW);

    f = (MGLAttribFrame *)malloc(sizeof(*f));
    ERROR_CHECK_RETURN(f, GL_OUT_OF_MEMORY);

    f->mask = mask;
    f->var = STATE(var);
    f->caps = STATE(caps);
    memcpy(f->viewport, STATE(viewport), sizeof(f->viewport));
    memcpy(f->scissor, STATE(scissor), sizeof(f->scissor));
    memcpy(f->depth_range, STATE(depth_range), sizeof(f->depth_range));
    memcpy(f->color_clear, STATE(color_clear_value), sizeof(f->color_clear));
    memcpy(f->attrib_constant, STATE(attrib_constant), sizeof(f->attrib_constant));
    f->active_texture = STATE(active_texture);

    for (int u = 0; u < MGL_COMPAT_UNITS; u++)
        for (int t = 0; t < UNIT_TARGETS; t++)
        {
            Texture *tex = STATE(texture_units[u].textures[unit_targets[t].index]);
            f->tex_names[u][t] = tex ? tex->name : 0;
        }

#define SAVE(field) memcpy(&f->field, &C->field, sizeof(f->field))
    SAVE(edge_flag); SAVE(raster_pos); SAVE(raster_color); SAVE(raster_secondary); SAVE(raster_tex);
    SAVE(raster_distance); SAVE(raster_valid);
    SAVE(point_smooth); SAVE(point_sprite); SAVE(sprite_origin);
    SAVE(line_stipple); SAVE(stipple_factor); SAVE(stipple_pattern);
    SAVE(polygon_mode); SAVE(polygon_stipple); SAVE(polygon_stipple_mask);
    SAVE(zoom_x); SAVE(zoom_y); SAVE(pixel_scale); SAVE(pixel_bias);
    SAVE(lighting); SAVE(lights_on); SAVE(lights); SAVE(material); SAVE(model_ambient);
    SAVE(local_viewer); SAVE(two_side); SAVE(color_control); SAVE(color_material);
    SAVE(color_material_face); SAVE(color_material_mode); SAVE(shade_model);
    SAVE(fog); SAVE(fog_mode); SAVE(fog_coord_src); SAVE(fog_color); SAVE(fog_density);
    SAVE(fog_start); SAVE(fog_end); SAVE(fog_index);
    SAVE(accum_clear); SAVE(matrix_mode); SAVE(clip_planes); SAVE(normalize); SAVE(rescale_normal);
    SAVE(color_sum); SAVE(auto_normal); SAVE(map1_on); SAVE(map2_on);
    SAVE(grid1_n); SAVE(grid2_n); SAVE(grid1); SAVE(grid2);
    SAVE(perspective_hint); SAVE(point_hint); SAVE(line_hint); SAVE(polygon_hint); SAVE(fog_hint);
    SAVE(list_base); SAVE(tex);
#undef SAVE

    C->attrib_stack[C->attrib_depth++] = f;
}

static void restoreTextures(GLMContext ctx, MGLAttribFrame *f)
{
    for (int u = 0; u < MGL_COMPAT_UNITS; u++)
    {
        for (int t = 0; t < UNIT_TARGETS; t++)
        {
            Texture *tex = STATE(texture_units[u].textures[unit_targets[t].index]);
            GLuint now = tex ? tex->name : 0, want = f->tex_names[u][t];

            if (now == want)
                continue;

            // a texture deleted since the push is not brought back
            if (want && searchHashTable(&SHARED(texture_table), want) == NULL)
                want = 0;

            mglActiveTexture(ctx, GL_TEXTURE0 + u);
            mglBindTexture(ctx, unit_targets[t].target, want);
        }
    }

    mglActiveTexture(ctx, GL_TEXTURE0 + f->active_texture);
}

static void cPopAttrib(GLMContext ctx)
{
    C->values_serial++;
    MGLAttribFrame *f;
    GLbitfield m;

    ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(C->attrib_depth > 0, GL_STACK_UNDERFLOW);

    f = C->attrib_stack[--C->attrib_depth];
    m = f->mask;

#define LOAD(field) memcpy(&C->field, &f->field, sizeof(f->field))
#define VAR(field) memcpy(&STATE_VAR(field), &f->var.field, sizeof(f->var.field))
#define CAP(field) memcpy(&STATE(caps.field), &f->caps.field, sizeof(f->caps.field))

    if (m & GL_CURRENT_BIT)
    {
        memcpy(STATE(attrib_constant), f->attrib_constant, sizeof(f->attrib_constant));
        STATE(attrib_constant_dirty) = (1u << MAX_ATTRIBS) - 1;
        LOAD(edge_flag); LOAD(raster_pos); LOAD(raster_color); LOAD(raster_secondary); LOAD(raster_tex);
        LOAD(raster_distance); LOAD(raster_valid);
    }

    if (m & GL_POINT_BIT)
    {
        VAR(point_size);
        LOAD(point_smooth); LOAD(point_sprite); LOAD(sprite_origin);
        for (int u = 0; u < MGL_COMPAT_UNITS; u++)
            C->tex[u].coord_replace = f->tex[u].coord_replace;
    }

    if (m & GL_LINE_BIT)
    {
        VAR(line_width);
        CAP(line_smooth);
        LOAD(line_stipple); LOAD(stipple_factor); LOAD(stipple_pattern);
    }

    if (m & GL_POLYGON_BIT)
    {
        CAP(cull_face); CAP(polygon_smooth); CAP(polygon_offset_point); CAP(polygon_offset_line);
        CAP(polygon_offset_fill);
        VAR(cull_face_mode); VAR(front_face); VAR(polygon_mode); VAR(polygon_offset_units);
        VAR(polygon_offset_factor); VAR(polygon_offset_clamp);
        LOAD(polygon_mode); LOAD(polygon_stipple);
    }

    if (m & GL_POLYGON_STIPPLE_BIT)
        LOAD(polygon_stipple_mask);

    if (m & GL_PIXEL_MODE_BIT)
    {
        LOAD(zoom_x); LOAD(zoom_y); LOAD(pixel_scale); LOAD(pixel_bias);
    }

    if (m & GL_LIGHTING_BIT)
    {
        LOAD(lighting); LOAD(lights_on); LOAD(lights); LOAD(material); LOAD(model_ambient);
        LOAD(local_viewer); LOAD(two_side); LOAD(color_control); LOAD(color_material);
        LOAD(color_material_face); LOAD(color_material_mode); LOAD(shade_model);
    }

    if (m & GL_FOG_BIT)
    {
        LOAD(fog); LOAD(fog_mode); LOAD(fog_coord_src); LOAD(fog_color); LOAD(fog_density);
        LOAD(fog_start); LOAD(fog_end); LOAD(fog_index);
    }

    if (m & GL_DEPTH_BUFFER_BIT)
    {
        CAP(depth_test);
        VAR(depth_func); VAR(depth_clear_value); VAR(depth_writemask);
    }

    if (m & GL_ACCUM_BUFFER_BIT)
        LOAD(accum_clear);

    if (m & GL_STENCIL_BUFFER_BIT)
    {
        CAP(stencil_test);
        VAR(stencil_clear_value); VAR(stencil_func); VAR(stencil_value_mask); VAR(stencil_fail);
        VAR(stencil_pass_depth_fail); VAR(stencil_pass_depth_pass); VAR(stencil_ref); VAR(stencil_writemask);
        VAR(stencil_back_func); VAR(stencil_back_fail); VAR(stencil_back_pass_depth_fail);
        VAR(stencil_back_pass_depth_pass); VAR(stencil_back_ref); VAR(stencil_back_value_mask);
        VAR(stencil_back_writemask);
    }

    if (m & GL_VIEWPORT_BIT)
    {
        memcpy(STATE(viewport), f->viewport, sizeof(f->viewport));
        memcpy(STATE(depth_range), f->depth_range, sizeof(f->depth_range));
        VAR(depth_range);
    }

    if (m & GL_TRANSFORM_BIT)
    {
        LOAD(matrix_mode); LOAD(clip_planes); LOAD(normalize); LOAD(rescale_normal);
        CAP(clip_distances);
    }

    if (m & GL_ENABLE_BIT)
    {
        STATE(caps) = f->caps;
        LOAD(lighting); LOAD(lights_on); LOAD(color_material); LOAD(normalize); LOAD(rescale_normal);
        LOAD(fog); LOAD(color_sum); LOAD(point_sprite); LOAD(point_smooth); LOAD(line_stipple);
        LOAD(polygon_stipple); LOAD(auto_normal); LOAD(map1_on); LOAD(map2_on);
        for (int u = 0; u < MGL_COMPAT_UNITS; u++)
        {
            C->tex[u].enabled = f->tex[u].enabled;
            C->tex[u].gen_enabled = f->tex[u].gen_enabled;
        }
    }

    if (m & GL_COLOR_BUFFER_BIT)
    {
        CAP(alpha_test); CAP(blend); CAP(use_blend_i); CAP(blend_i); CAP(dither); CAP(color_logic_op);
        CAP(use_color_mask);
        VAR(alpha_test_func); VAR(alpha_test_ref); VAR(blend_color); VAR(blend_dst_rgb); VAR(blend_src_rgb);
        VAR(blend_dst_alpha); VAR(blend_src_alpha); VAR(blend_equation_rgb); VAR(blend_equation_alpha);
        VAR(blend_dst); VAR(blend_src); VAR(logic_op); VAR(logic_op_mode); VAR(color_writemask);
        memcpy(STATE(color_clear_value), f->color_clear, sizeof(f->color_clear));
    }

    if (m & GL_HINT_BIT)
    {
        LOAD(perspective_hint); LOAD(point_hint); LOAD(line_hint); LOAD(polygon_hint); LOAD(fog_hint);
    }

    if (m & GL_EVAL_BIT)
    {
        LOAD(auto_normal); LOAD(map1_on); LOAD(map2_on);
        LOAD(grid1_n); LOAD(grid2_n); LOAD(grid1); LOAD(grid2);
    }

    if (m & GL_LIST_BIT)
        LOAD(list_base);

    if (m & GL_TEXTURE_BIT)
    {
        GLbitfield enabled[MGL_COMPAT_UNITS], gen[MGL_COMPAT_UNITS];

        // the enables belong to GL_ENABLE_BIT, which may not be popping
        for (int u = 0; u < MGL_COMPAT_UNITS; u++)
        {
            enabled[u] = C->tex[u].enabled;
            gen[u] = C->tex[u].gen_enabled;
        }

        LOAD(tex);

        for (int u = 0; u < MGL_COMPAT_UNITS; u++)
        {
            C->tex[u].enabled = (m & GL_ENABLE_BIT) ? f->tex[u].enabled : enabled[u];
            C->tex[u].gen_enabled = (m & GL_ENABLE_BIT) ? f->tex[u].gen_enabled : gen[u];
        }

        restoreTextures(ctx, f);
    }

    if (m & GL_SCISSOR_BIT)
    {
        CAP(scissor_test); CAP(scissor_test_i);
        memcpy(STATE(scissor), f->scissor, sizeof(f->scissor));
    }

    if (m & GL_MULTISAMPLE_BIT)
    {
        CAP(multisample); CAP(sample_alpha_to_coverage); CAP(sample_alpha_to_one); CAP(sample_coverage);
        VAR(sample_coverage_value); VAR(sample_coverage_invert);
    }

#undef LOAD
#undef VAR
#undef CAP

    // the core state holds the polygon mode while both faces agree
    STATE_VAR(polygon_mode) = C->polygon_mode[0];

    STATE(dirty_bits) |= DIRTY_STATE | DIRTY_RENDER_STATE | DIRTY_ALPHA_STATE | DIRTY_TEX_BINDING;

    free(f);
}

static Buffer *bufferNamed(GLMContext ctx, GLuint name)
{
    return name ? (Buffer *)searchHashTable(&SHARED(buffer_table), name) : NULL;
}

static void cPushClientAttrib(GLMContext ctx, GLbitfield mask)
{
    MGLClientFrame *f;
    VertexArray *vao = STATE(vao);

    ERROR_CHECK_RETURN(C->client_depth < MGL_ATTRIB_DEPTH, GL_STACK_OVERFLOW);

    f = (MGLClientFrame *)calloc(1, sizeof(*f));
    ERROR_CHECK_RETURN(f, GL_OUT_OF_MEMORY);

    f->mask = mask;
    f->pack = STATE(pack);
    f->unpack = STATE(unpack);
    f->vao = vao;
    f->vao_name = vao ? vao->name : 0;

    if (vao)
    {
        f->arrays = *vao;
        for (int b = 0; b < MAX_BINDABLE_BUFFERS; b++)
            f->buffer_names[b] = vao->bindings[b].buffer ? vao->bindings[b].buffer->name : 0;
        f->element_name = vao->element_array.buffer ? vao->element_array.buffer->name : 0;
    }

    f->array_buffer = STATE(buffers[_ARRAY_BUFFER]) ? STATE(buffers[_ARRAY_BUFFER])->name : 0;
    f->client_active = C->client_active_texture;
    f->edge_flag_array = C->edge_flag_array;
    f->edge_flag_pointer = C->edge_flag_pointer;
    f->edge_flag_stride = C->edge_flag_stride;

    C->client_stack[C->client_depth++] = f;
}

static void cPopClientAttrib(GLMContext ctx)
{
    MGLClientFrame *f;

    ERROR_CHECK_RETURN(C->client_depth > 0, GL_STACK_UNDERFLOW);

    f = C->client_stack[--C->client_depth];

    if (f->mask & GL_CLIENT_PIXEL_STORE_BIT)
    {
        STATE(pack) = f->pack;
        STATE(unpack) = f->unpack;
    }

    if (f->mask & GL_CLIENT_VERTEX_ARRAY_BIT)
    {
        // only a vertex array that still exists takes its arrays back
        VertexArray *vao = f->vao == C->default_vao ? C->default_vao :
                           f->vao_name ? (VertexArray *)searchHashTable(&STATE(vao_table), f->vao_name) : NULL;

        if (vao && vao == f->vao)
        {
            GLuint name = vao->name;
            void *mtl = vao->mtl_data;

            *vao = f->arrays;
            vao->name = name;
            vao->mtl_data = mtl;

            for (int b = 0; b < MAX_BINDABLE_BUFFERS; b++)
            {
                if (f->buffer_names[b])
                    vao->bindings[b].buffer = bufferNamed(ctx, f->buffer_names[b]);
                else
                    vao->bindings[b].buffer = NULL;
            }

            vao->element_array.buffer = bufferNamed(ctx, f->element_name);
            vao->dirty_bits |= DIRTY_VAO | DIRTY_VAO_BUFFER_BASE;
            STATE(vao) = vao;
            STATE(dirty_bits) |= DIRTY_VAO;
        }

        mglBindBuffer(ctx, GL_ARRAY_BUFFER, bufferNamed(ctx, f->array_buffer) ? f->array_buffer : 0);
        C->client_active_texture = f->client_active;
        C->edge_flag_array = f->edge_flag_array;
        C->edge_flag_pointer = f->edge_flag_pointer;
        C->edge_flag_stride = f->edge_flag_stride;
    }

    free(f);
}

#define SET(field, fn) ctx->dispatch.field = fn

void mglCompatInstallAttrib(GLMContext ctx)
{
    SET(push_attrib, cPushAttrib);
    SET(pop_attrib, cPopAttrib);
    SET(push_client_attrib, cPushClientAttrib);
    SET(pop_client_attrib, cPopClientAttrib);
}

void mglCompatFreeAttrib(GLMContext ctx)
{
    while (C->attrib_depth)
        free(C->attrib_stack[--C->attrib_depth]);

    while (C->client_depth)
        free(C->client_stack[--C->client_depth]);
}

#endif /* MGL_COMPAT_CONTEXTS */
