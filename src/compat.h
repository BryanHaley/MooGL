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
 * compat.h
 * MGL
 *
 * What the compat_*.c files share with each other.
 */

#ifndef compat_h
#define compat_h

#include "glm_context.h"

#ifdef MGL_COMPAT_CONTEXTS

#define MGL_MODELVIEW_DEPTH  32
#define MGL_PROJECTION_DEPTH 4
#define MGL_TEXTURE_DEPTH    10
#define MGL_NAME_DEPTH       64
#define MGL_ATTRIB_DEPTH     16
#define MGL_LIST_NESTING     64
#define MGL_MAX_EVAL_ORDER   30
#define MGL_EVAL_MAPS        9    // GL_MAP1_COLOR_4 to GL_MAP1_VERTEX_4

typedef struct {
    GLfloat m[16];
} MGLMatrix;

typedef struct {
    MGLMatrix s[MGL_MODELVIEW_DEPTH];
    int depth, max;
} MGLMatrixStack;

typedef struct {
    GLfloat ambient[4], diffuse[4], specular[4];
    // position and spot direction are kept in eye space, as GL does
    GLfloat position[4], spot_direction[4];
    GLfloat spot_exponent, spot_cutoff;
    GLfloat attenuation[3];
} MGLLight;

typedef struct {
    GLfloat ambient[4], diffuse[4], specular[4], emission[4];
    GLfloat shininess;
} MGLMaterial;

// one texture unit's fixed-function state
typedef struct {
    GLbitfield enabled;           // MGL_TEX_* bits glEnable set
    GLenum  mode;
    GLfloat color[4];
    GLenum  combine_rgb, combine_alpha;
    GLenum  src_rgb[3], src_alpha[3];
    GLenum  op_rgb[3], op_alpha[3];
    GLfloat rgb_scale, alpha_scale;
    GLfloat lod_bias;
    GLboolean coord_replace;
    GLbitfield gen_enabled;       // S, T, R, Q
    GLenum  gen_mode[4];
    GLfloat obj_plane[4][4];
    GLfloat eye_plane[4][4];      // in eye space
} MGLTexEnv;

enum {
    MGL_TEX_1D   = 1 << 0,
    MGL_TEX_2D   = 1 << 1,
    MGL_TEX_3D   = 1 << 2,
    MGL_TEX_CUBE = 1 << 3,
    MGL_TEX_RECT = 1 << 4,
};

// a glMap1 or glMap2 map; a 1D map has one row in v. points is NULL until
// set, which means the default value.
typedef struct {
    GLint uorder, vorder;
    GLfloat u1, u2, v1, v2;
    GLfloat *points;
} MGLEvalMap;

// the fixed-function state glPushAttrib saves, by group
typedef struct MGLAttribFrame_t MGLAttribFrame;
typedef struct MGLClientFrame_t MGLClientFrame;

typedef struct MGLFFProgram_t MGLFFProgram;
typedef struct MGLDisplayList_t MGLDisplayList;

struct MGLCompat_t {
    // the dispatch table as it was before the compatibility entries went in,
    // which they forward to
    struct GLMDispatchTable next;

    // the quad mode the triangles being drawn were split from, and how many
    // there are, for wireframes that leave out the edges inside
    GLenum split_mode;
    GLsizei split_tris;

    // --- transform ---
    GLenum matrix_mode;
    MGLMatrixStack modelview, projection, texture[MGL_COMPAT_UNITS];
    GLboolean normalize, rescale_normal;
    GLfloat clip_planes[MGL_COMPAT_PLANES][4];   // in eye space
    // bumped when a matrix changes, so uniforms are rewritten only then
    GLuint matrix_serial;
    // the same for lights, materials, fog, clip planes, texgen and texture
    // environment values
    GLuint values_serial;

    // --- vertex arrays ---
    GLuint client_active_texture;
    struct VertexArray_t *default_vao;
    // where client arrays are copied for a draw, one per binding
    struct Buffer_t *staging[16];
    struct Buffer_t *stream;   // big staged copies, one after another
    size_t stream_used;
    GLboolean edge_flag;
    GLboolean edge_flag_array;
    const void *edge_flag_pointer;
    GLsizei edge_flag_stride;
    GLuint array_element_lock_first, array_element_lock_count;

    // --- lighting ---
    GLboolean lighting;
    GLbitfield lights_on;
    MGLLight lights[MGL_COMPAT_LIGHTS];
    MGLMaterial material[2];
    GLfloat model_ambient[4];
    GLboolean local_viewer, two_side;
    GLenum color_control;
    GLboolean color_material;
    GLenum color_material_face, color_material_mode;
    GLenum shade_model;

    // --- texturing ---
    MGLTexEnv tex[MGL_COMPAT_UNITS];
    GLboolean color_sum;

    // --- fog ---
    GLboolean fog;
    GLenum fog_mode, fog_coord_src;
    GLfloat fog_color[4], fog_density, fog_start, fog_end, fog_index;

    // --- points ---
    GLboolean point_sprite, point_smooth;
    GLenum sprite_origin;

    // --- polygons ---
    GLenum polygon_mode[2];      // front, back
    GLboolean line_stipple, polygon_stipple, line_smooth, polygon_smooth;
    GLint stipple_factor;
    GLushort stipple_pattern;
    GLubyte polygon_stipple_mask[128];

    // --- hints and misc state answered for old code ---
    GLenum perspective_hint, point_hint, line_hint, polygon_hint, fog_hint;
    GLboolean auto_normal, dither_on, index_logic_op;
    GLfloat accum_clear[4];
    GLfloat clear_index;

    // --- evaluators ---
    MGLEvalMap map1[MGL_EVAL_MAPS], map2[MGL_EVAL_MAPS];
    GLbitfield map1_on, map2_on;   // by target, from GL_MAP1_COLOR_4 or GL_MAP2_COLOR_4
    GLint grid1_n, grid2_n[2];
    GLfloat grid1[2], grid2[4];

    // --- immediate mode ---
    GLboolean in_begin;
    GLboolean client_storage;  // GL_UNPACK_CLIENT_STORAGE_APPLE
    GLenum begin_mode;
    GLfloat *imm;              // 16 vec4 per vertex
    size_t imm_count, imm_cap;
    struct VertexArray_t *imm_vao;
    struct Buffer_t *imm_buf;  // bound to imm_vao for good
    GLuint imm_mask;           // the attributes imm_vao reads from it

    // --- raster position ---
    GLfloat raster_pos[4];     // window x, y, z and clip w
    GLboolean raster_valid;
    GLfloat raster_color[4], raster_secondary[4], raster_tex[4];
    GLfloat raster_distance;
    GLfloat zoom_x, zoom_y;
    GLfloat pixel_scale[4], pixel_bias[4];
    GLuint raster_tex_name;
    GLuint raster_prog, bitmap_prog;
    GLint raster_loc_rect, raster_loc_depth, raster_loc_color;
    GLint bitmap_loc_rect, bitmap_loc_depth, bitmap_loc_color;
    struct VertexArray_t *raster_vao;

    // --- render mode ---
    GLenum render_mode;
    GLuint *select_buffer;
    GLsizei select_size;
    GLuint select_used;        // words written
    GLuint select_hits;
    GLboolean select_overflow;
    GLuint names[MGL_NAME_DEPTH];
    GLuint name_depth;
    GLboolean hit;
    GLfloat hit_min, hit_max;
    GLfloat *feedback_buffer;
    GLsizei feedback_size;
    GLenum feedback_type;

    // --- display lists ---
    // the table as it was when glNewList swapped the recorders in
    struct GLMDispatchTable list_live;
    // above zero while a list plays back or the driver draws for itself, so
    // nothing it calls is recorded a second time
    GLuint no_record;
    MGLDisplayList *compiling;
    GLuint compiling_name;
    GLenum list_mode;
    GLuint list_base;
    GLuint list_nesting;

    // --- attribute stacks ---
    MGLAttribFrame *attrib_stack[MGL_ATTRIB_DEPTH];
    GLuint attrib_depth;
    MGLClientFrame *client_stack[MGL_ATTRIB_DEPTH];
    GLuint client_depth;

    // --- the generated fixed-function programs ---
    MGLFFProgram *ff_programs;
    // a program of the application's that uses the old built-in uniforms
    GLuint ff_serial;

    char *extension_string;
};

// compat_state.c
void mglMatIdentity(GLfloat *m);
void mglMatMul(GLfloat *r, const GLfloat *a, const GLfloat *b);
void mglMatTransform(const GLfloat *m, const GLfloat *v, GLfloat *out);
bool mglMatInvert(const GLfloat *m, GLfloat *out);
GLfloat *mglCompatModelview(GLMContext ctx);
GLfloat *mglCompatProjection(GLMContext ctx);
GLfloat *mglCompatTextureMatrix(GLMContext ctx, GLuint unit);
GLint mglCompatNumExtensions(GLMContext ctx);

// compat_arrays.c
void mglCompatInstallArrays(GLMContext ctx);
void mglCompatFreeArrays(GLMContext ctx);
GLboolean mglCompatClientStateEnabled(GLMContext ctx, GLenum array);
int mglCompatCurrentValue(GLMContext ctx, GLenum pname, GLdouble *out);
int mglCompatArrayQuery(GLMContext ctx, GLenum pname, GLdouble *out);
void mglCompatSetCurrent(GLMContext ctx, GLuint slot, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
const GLfloat *mglCompatCurrent(GLMContext ctx, GLuint slot);

// compat_draw.c
void mglCompatInstallDraw(GLMContext ctx);
// Draws with the vertex arrays as they stand, through the compatibility
// path: client memory, quads and polygons, fixed function, both polygon
// modes, selection. type is 0 for an array draw.
void mglCompatDraw(GLMContext ctx, GLenum mode, GLint first, GLsizei count, GLenum type,
                   const void *indices, GLsizei instances, GLint basevertex);

// Reads count vertices of the current arrays into 16 vec4 per vertex, the
// layout immediate mode records. Returns a malloc'd block or NULL.
GLfloat *mglCompatFetchVertices(GLMContext ctx, GLint first, GLsizei count, GLenum type,
                                const void *indices, GLint basevertex, GLsizei *out_count);

// compat_ff.c
struct Program_t;
// the generated program for the current fixed-function state, for a draw of
// mode; NULL when it will not build
struct Program_t *mglCompatFFProgram(GLMContext ctx, GLenum mode);
// writes the old built-in uniforms (gl_ModelViewMatrix and the rest) that
// prog reads, and the fixed-function uniforms when prog is generated
void mglCompatWriteBuiltins(GLMContext ctx, struct Program_t *prog);
GLenum mglCompatSplitMode(GLMContext ctx, GLsizei *tris);
void mglCompatFreeFF(GLMContext ctx);

// compat_immediate.c
void mglCompatInstallImmediate(GLMContext ctx);
void mglCompatEmitVertex(GLMContext ctx, GLfloat x, GLfloat y, GLfloat z, GLfloat w);

// draws records of 16 vec4 a vertex, the layout immediate mode keeps
void mglCompatDrawRecords(GLMContext ctx, GLenum mode, const GLfloat *records, GLsizei count);
void *mglCompatStream(GLMContext ctx, size_t bytes, struct Buffer_t **buf, GLintptr *offset);
bool mglCompatStreamReserve(GLMContext ctx, size_t bytes);

// compat_attrib.c
void mglCompatInstallAttrib(GLMContext ctx);
void mglCompatFreeAttrib(GLMContext ctx);

// compat_lists.c
void mglCompatInstallLists(GLMContext ctx);
// a list is being compiled and this call belongs in it
bool mglCompatRecording(GLMContext ctx);
// the call runs as well: not compiling, or compiling and executing
bool mglCompatExecuting(GLMContext ctx);
void mglCompatRecordCurrent(GLMContext ctx, GLuint slot, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void mglCompatRecordVertex(GLMContext ctx, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void mglCompatRecordRecords(GLMContext ctx, GLenum mode, const GLfloat *records, GLsizei count);
void mglCompatRecordEval(GLMContext ctx, const GLuint *args, int n, void *blob);
void mglCompatRecordRasterPos(GLMContext ctx, GLboolean window, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void mglCompatRecordBitmap(GLMContext ctx, GLsizei w, GLsizei h, GLfloat xo, GLfloat yo, GLfloat xm, GLfloat ym,
                           const GLubyte *packed);
void mglCompatRecordDrawPixels(GLMContext ctx, GLsizei w, GLsizei h, GLenum format, GLenum type,
                               const void *packed, size_t bytes);

// compat_raster.c
void mglCompatInstallRaster(GLMContext ctx);
void mglCompatFreeRaster(GLMContext ctx);
// glRasterPos4f, or glWindowPos3f when window is set
void mglCompatRasterPos(GLMContext ctx, GLboolean window, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
// glBitmap with the bitmap already unpacked: rows of (w + 7) / 8 bytes, the
// first pixel in the top bit
void mglCompatBitmapPacked(GLMContext ctx, GLsizei w, GLsizei h, GLfloat xo, GLfloat yo, GLfloat xm, GLfloat ym,
                           const GLubyte *bits);
// glDrawPixels with the pixels already unpacked into tight rows
void mglCompatDrawPixelsPacked(GLMContext ctx, GLsizei w, GLsizei h, GLenum format, GLenum type, const void *pixels);

// compat_eval.c
void mglCompatInstallEval(GLMContext ctx);
void mglCompatFreeEval(GLMContext ctx);
void mglCompatEvalReplay(GLMContext ctx, const GLuint *args, const void *blob);
int mglCompatEvalGet(GLMContext ctx, GLenum pname, GLdouble *out);

// compat_select.c
void mglCompatInstallSelect(GLMContext ctx);
void mglCompatInstallApple(GLMContext ctx);
// in GL_SELECT mode: records hits for the vertices of a draw instead of
// drawing it
void mglCompatSelectVertices(GLMContext ctx, GLenum mode, const GLfloat *verts, GLsizei count);
void mglCompatSelectRaster(GLMContext ctx);

#endif

#endif /* compat_h */
