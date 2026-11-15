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
 * compat_arrays.c
 * MGL
 *
 * The legacy vertex arrays and current values. Each is a generic attribute
 * at a fixed slot (mgl_compat.h), so glColorPointer is glVertexAttribPointer
 * on slot 3 and glColor4f is glVertexAttrib4f on it. A compatibility context
 * has a default vertex array object, bound as zero, whose arrays may point
 * into client memory.
 */

#include <stdlib.h>
#include <string.h>

#include "glm_context.h"
#include "compat.h"

#ifdef MGL_COMPAT_CONTEXTS

#define C (ctx->compat)

VertexArray *newVAO(GLMContext ctx, GLuint vao);
VertexArray *getVAO(GLMContext ctx, GLuint vao);
void mglVertexAttribPointer(GLMContext ctx, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
void mglEnableVertexAttribArray(GLMContext ctx, GLuint index);
void mglDisableVertexAttribArray(GLMContext ctx, GLuint index);
void mglVertexAttrib4f(GLMContext ctx, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);

GLboolean mglCompatClientArraysAllowed(GLMContext ctx)
{
    VertexArray *vao = STATE(vao);

    return C && vao && (vao == C->default_vao || vao == C->imm_vao || vao == C->raster_vao);
}

void mglCompatSetCurrent(GLMContext ctx, GLuint slot, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    if (mglCompatRecording(ctx))
    {
        mglCompatRecordCurrent(ctx, slot, x, y, z, w);
        if (!mglCompatExecuting(ctx))
            return;
    }

    mglVertexAttrib4f(ctx, slot, x, y, z, w);
}

const GLfloat *mglCompatCurrent(GLMContext ctx, GLuint slot)
{
    return STATE(attrib_constant[slot].v.f);
}

int mglCompatCurrentValue(GLMContext ctx, GLenum pname, GLdouble *out)
{
    GLuint slot;
    int n = 4;

    switch (pname)
    {
        case GL_CURRENT_COLOR: slot = MGL_ATTR_COLOR; break;
        case GL_CURRENT_SECONDARY_COLOR: slot = MGL_ATTR_SECONDARY; break;
        case GL_CURRENT_NORMAL: slot = MGL_ATTR_NORMAL; n = 3; break;
        case GL_CURRENT_FOG_COORD: slot = MGL_ATTR_FOG; n = 1; break;
        case GL_CURRENT_TEXTURE_COORDS:
            slot = MGL_ATTR_TEXCOORD0 + (STATE(active_texture) < MGL_COMPAT_UNITS ? STATE(active_texture) : 0);
            break;
        default:
            return 0;
    }

    for (int i = 0; i < n; i++)
        out[i] = STATE(attrib_constant[slot].v.f[i]);

    return n;
}

// --- current values ------------------------------------------------------------

// GL's conversions of integer colours and normals to [-1, 1] or [0, 1]
#define CB(v)  ((GLfloat)((2.0 * (v) + 1.0) / 255.0))
#define CUB(v) ((GLfloat)(v) / 255.0f)
#define CS(v)  ((GLfloat)((2.0 * (v) + 1.0) / 65535.0))
#define CUS(v) ((GLfloat)(v) / 65535.0f)
#define CI(v)  ((GLfloat)((2.0 * (double)(v) + 1.0) / 4294967295.0))
#define CUI(v) ((GLfloat)((double)(v) / 4294967295.0))
#define CF(v)  ((GLfloat)(v))

#define COLOR_FAMILY(sfx, T, cv)                                                              \
static void cColor3##sfx(GLMContext ctx, T r, T g, T b)                                       \
{ mglCompatSetCurrent(ctx, MGL_ATTR_COLOR, cv(r), cv(g), cv(b), 1.0f); }                      \
static void cColor3##sfx##v(GLMContext ctx, const T *v)                                       \
{ mglCompatSetCurrent(ctx, MGL_ATTR_COLOR, cv(v[0]), cv(v[1]), cv(v[2]), 1.0f); }             \
static void cColor4##sfx(GLMContext ctx, T r, T g, T b, T a)                                  \
{ mglCompatSetCurrent(ctx, MGL_ATTR_COLOR, cv(r), cv(g), cv(b), cv(a)); }                     \
static void cColor4##sfx##v(GLMContext ctx, const T *v)                                       \
{ mglCompatSetCurrent(ctx, MGL_ATTR_COLOR, cv(v[0]), cv(v[1]), cv(v[2]), cv(v[3])); }         \
static void cSecondaryColor3##sfx(GLMContext ctx, T r, T g, T b)                              \
{ mglCompatSetCurrent(ctx, MGL_ATTR_SECONDARY, cv(r), cv(g), cv(b), 1.0f); }                  \
static void cSecondaryColor3##sfx##v(GLMContext ctx, const T *v)                              \
{ mglCompatSetCurrent(ctx, MGL_ATTR_SECONDARY, cv(v[0]), cv(v[1]), cv(v[2]), 1.0f); }

COLOR_FAMILY(b, GLbyte, CB)
COLOR_FAMILY(ub, GLubyte, CUB)
COLOR_FAMILY(s, GLshort, CS)
COLOR_FAMILY(us, GLushort, CUS)
COLOR_FAMILY(i, GLint, CI)
COLOR_FAMILY(ui, GLuint, CUI)
COLOR_FAMILY(f, GLfloat, CF)
COLOR_FAMILY(d, GLdouble, CF)

#define NORMAL_FAMILY(sfx, T, cv)                                                             \
static void cNormal3##sfx(GLMContext ctx, T x, T y, T z)                                      \
{ mglCompatSetCurrent(ctx, MGL_ATTR_NORMAL, cv(x), cv(y), cv(z), 1.0f); }                     \
static void cNormal3##sfx##v(GLMContext ctx, const T *v)                                      \
{ mglCompatSetCurrent(ctx, MGL_ATTR_NORMAL, cv(v[0]), cv(v[1]), cv(v[2]), 1.0f); }

NORMAL_FAMILY(b, GLbyte, CB)
NORMAL_FAMILY(s, GLshort, CS)
NORMAL_FAMILY(i, GLint, CI)
NORMAL_FAMILY(f, GLfloat, CF)
NORMAL_FAMILY(d, GLdouble, CF)

static GLuint unitOf(GLenum target)
{
    return target - GL_TEXTURE0;
}

#define TEXCOORD_FAMILY(sfx, T)                                                                         \
static void cTexCoord1##sfx(GLMContext ctx, T s)                                                        \
{ mglCompatSetCurrent(ctx, MGL_ATTR_TEXCOORD0, (GLfloat)s, 0, 0, 1); }                                  \
static void cTexCoord1##sfx##v(GLMContext ctx, const T *v)                                              \
{ mglCompatSetCurrent(ctx, MGL_ATTR_TEXCOORD0, (GLfloat)v[0], 0, 0, 1); }                               \
static void cTexCoord2##sfx(GLMContext ctx, T s, T t)                                                   \
{ mglCompatSetCurrent(ctx, MGL_ATTR_TEXCOORD0, (GLfloat)s, (GLfloat)t, 0, 1); }                         \
static void cTexCoord2##sfx##v(GLMContext ctx, const T *v)                                              \
{ mglCompatSetCurrent(ctx, MGL_ATTR_TEXCOORD0, (GLfloat)v[0], (GLfloat)v[1], 0, 1); }                   \
static void cTexCoord3##sfx(GLMContext ctx, T s, T t, T r)                                              \
{ mglCompatSetCurrent(ctx, MGL_ATTR_TEXCOORD0, (GLfloat)s, (GLfloat)t, (GLfloat)r, 1); }                \
static void cTexCoord3##sfx##v(GLMContext ctx, const T *v)                                              \
{ mglCompatSetCurrent(ctx, MGL_ATTR_TEXCOORD0, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], 1); }       \
static void cTexCoord4##sfx(GLMContext ctx, T s, T t, T r, T q)                                         \
{ mglCompatSetCurrent(ctx, MGL_ATTR_TEXCOORD0, (GLfloat)s, (GLfloat)t, (GLfloat)r, (GLfloat)q); }       \
static void cTexCoord4##sfx##v(GLMContext ctx, const T *v)                                              \
{ mglCompatSetCurrent(ctx, MGL_ATTR_TEXCOORD0, (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]); } \
static void cMultiTexCoord1##sfx(GLMContext ctx, GLenum u, T s)                                         \
{ if (unitOf(u) >= MGL_COMPAT_UNITS) ERROR_RETURN(GL_INVALID_ENUM);                                     \
  mglCompatSetCurrent(ctx, MGL_ATTR_TEXCOORD0 + unitOf(u), (GLfloat)s, 0, 0, 1); }                      \
static void cMultiTexCoord1##sfx##v(GLMContext ctx, GLenum u, const T *v)                               \
{ if (unitOf(u) >= MGL_COMPAT_UNITS) ERROR_RETURN(GL_INVALID_ENUM);                                     \
  mglCompatSetCurrent(ctx, MGL_ATTR_TEXCOORD0 + unitOf(u), (GLfloat)v[0], 0, 0, 1); }                   \
static void cMultiTexCoord2##sfx(GLMContext ctx, GLenum u, T s, T t)                                    \
{ if (unitOf(u) >= MGL_COMPAT_UNITS) ERROR_RETURN(GL_INVALID_ENUM);                                     \
  mglCompatSetCurrent(ctx, MGL_ATTR_TEXCOORD0 + unitOf(u), (GLfloat)s, (GLfloat)t, 0, 1); }             \
static void cMultiTexCoord2##sfx##v(GLMContext ctx, GLenum u, const T *v)                               \
{ if (unitOf(u) >= MGL_COMPAT_UNITS) ERROR_RETURN(GL_INVALID_ENUM);                                     \
  mglCompatSetCurrent(ctx, MGL_ATTR_TEXCOORD0 + unitOf(u), (GLfloat)v[0], (GLfloat)v[1], 0, 1); }       \
static void cMultiTexCoord3##sfx(GLMContext ctx, GLenum u, T s, T t, T r)                               \
{ if (unitOf(u) >= MGL_COMPAT_UNITS) ERROR_RETURN(GL_INVALID_ENUM);                                     \
  mglCompatSetCurrent(ctx, MGL_ATTR_TEXCOORD0 + unitOf(u), (GLfloat)s, (GLfloat)t, (GLfloat)r, 1); }    \
static void cMultiTexCoord3##sfx##v(GLMContext ctx, GLenum u, const T *v)                               \
{ if (unitOf(u) >= MGL_COMPAT_UNITS) ERROR_RETURN(GL_INVALID_ENUM);                                     \
  mglCompatSetCurrent(ctx, MGL_ATTR_TEXCOORD0 + unitOf(u), (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], 1); } \
static void cMultiTexCoord4##sfx(GLMContext ctx, GLenum u, T s, T t, T r, T q)                          \
{ if (unitOf(u) >= MGL_COMPAT_UNITS) ERROR_RETURN(GL_INVALID_ENUM);                                     \
  mglCompatSetCurrent(ctx, MGL_ATTR_TEXCOORD0 + unitOf(u), (GLfloat)s, (GLfloat)t, (GLfloat)r, (GLfloat)q); } \
static void cMultiTexCoord4##sfx##v(GLMContext ctx, GLenum u, const T *v)                               \
{ if (unitOf(u) >= MGL_COMPAT_UNITS) ERROR_RETURN(GL_INVALID_ENUM);                                     \
  mglCompatSetCurrent(ctx, MGL_ATTR_TEXCOORD0 + unitOf(u), (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3]); }

TEXCOORD_FAMILY(s, GLshort)
TEXCOORD_FAMILY(i, GLint)
TEXCOORD_FAMILY(f, GLfloat)
TEXCOORD_FAMILY(d, GLdouble)

static void cFogCoordf(GLMContext ctx, GLfloat f) { mglCompatSetCurrent(ctx, MGL_ATTR_FOG, f, 0, 0, 1); }
static void cFogCoordfv(GLMContext ctx, const GLfloat *f) { mglCompatSetCurrent(ctx, MGL_ATTR_FOG, f[0], 0, 0, 1); }
static void cFogCoordd(GLMContext ctx, GLdouble f) { mglCompatSetCurrent(ctx, MGL_ATTR_FOG, (GLfloat)f, 0, 0, 1); }
static void cFogCoorddv(GLMContext ctx, const GLdouble *f) { mglCompatSetCurrent(ctx, MGL_ATTR_FOG, (GLfloat)f[0], 0, 0, 1); }

// colour index mode does not exist here; the calls are accepted
#define INDEX_FAMILY(sfx, T)                                                    \
static void cIndex##sfx(GLMContext ctx, T c) { (void)ctx; (void)c; }            \
static void cIndex##sfx##v(GLMContext ctx, const T *c) { (void)ctx; (void)c; }

INDEX_FAMILY(d, GLdouble)
INDEX_FAMILY(f, GLfloat)
INDEX_FAMILY(i, GLint)
INDEX_FAMILY(s, GLshort)
INDEX_FAMILY(ub, GLubyte)

// --- client state and the old pointers ----------------------------------------

static int slotOf(GLMContext ctx, GLenum array)
{
    switch (array)
    {
        case GL_VERTEX_ARRAY: return MGL_ATTR_VERTEX;
        case GL_NORMAL_ARRAY: return MGL_ATTR_NORMAL;
        case GL_COLOR_ARRAY: return MGL_ATTR_COLOR;
        case GL_SECONDARY_COLOR_ARRAY: return MGL_ATTR_SECONDARY;
        case GL_FOG_COORD_ARRAY: return MGL_ATTR_FOG;
        case GL_TEXTURE_COORD_ARRAY: return MGL_ATTR_TEXCOORD0 + (int)C->client_active_texture;
    }

    return -1;
}

GLboolean mglCompatClientStateEnabled(GLMContext ctx, GLenum array)
{
    int slot = slotOf(ctx, array);

    if (array == GL_EDGE_FLAG_ARRAY)
        return C->edge_flag_array;

    if (slot < 0 || !STATE(vao))
        return GL_FALSE;

    return (STATE(vao)->enabled_attribs >> slot) & 1;
}

static void clientState(GLMContext ctx, GLenum array, GLboolean on)
{
    int slot = slotOf(ctx, array);

    if (array == GL_EDGE_FLAG_ARRAY)
    {
        C->edge_flag_array = on;
        return;
    }

    if (array == GL_INDEX_ARRAY)
        return;

    ERROR_CHECK_RETURN(slot >= 0, GL_INVALID_ENUM);

    if (on)
        mglEnableVertexAttribArray(ctx, (GLuint)slot);
    else
        mglDisableVertexAttribArray(ctx, (GLuint)slot);
}

static void cEnableClientState(GLMContext ctx, GLenum array) { clientState(ctx, array, GL_TRUE); }
static void cDisableClientState(GLMContext ctx, GLenum array) { clientState(ctx, array, GL_FALSE); }

static void cClientActiveTexture(GLMContext ctx, GLenum texture)
{
    ERROR_CHECK_RETURN(texture >= GL_TEXTURE0 && texture < GL_TEXTURE0 + MGL_COMPAT_UNITS, GL_INVALID_ENUM);
    C->client_active_texture = texture - GL_TEXTURE0;
}

static bool integerType(GLenum type)
{
    return type == GL_BYTE || type == GL_UNSIGNED_BYTE || type == GL_SHORT || type == GL_UNSIGNED_SHORT ||
           type == GL_INT || type == GL_UNSIGNED_INT;
}

static void cVertexPointer(GLMContext ctx, GLint size, GLenum type, GLsizei stride, const void *ptr)
{
    ERROR_CHECK_RETURN(size >= 2 && size <= 4, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(type == GL_SHORT || type == GL_INT || type == GL_FLOAT || type == GL_DOUBLE ||
                       type == GL_HALF_FLOAT || type == GL_INT_2_10_10_10_REV ||
                       type == GL_UNSIGNED_INT_2_10_10_10_REV, GL_INVALID_ENUM);
    mglVertexAttribPointer(ctx, MGL_ATTR_VERTEX, size, type, GL_FALSE, stride, ptr);
}

static void cNormalPointer(GLMContext ctx, GLenum type, GLsizei stride, const void *ptr)
{
    ERROR_CHECK_RETURN(type == GL_BYTE || type == GL_SHORT || type == GL_INT || type == GL_FLOAT ||
                       type == GL_DOUBLE || type == GL_HALF_FLOAT || type == GL_INT_2_10_10_10_REV ||
                       type == GL_UNSIGNED_INT_2_10_10_10_REV, GL_INVALID_ENUM);
    mglVertexAttribPointer(ctx, MGL_ATTR_NORMAL, 3, type, type != GL_FLOAT && type != GL_DOUBLE &&
                           type != GL_HALF_FLOAT, stride, ptr);
}

static void cColorPointer(GLMContext ctx, GLint size, GLenum type, GLsizei stride, const void *ptr)
{
    ERROR_CHECK_RETURN(size == 3 || size == 4, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(integerType(type) || type == GL_FLOAT || type == GL_DOUBLE || type == GL_HALF_FLOAT ||
                       type == GL_INT_2_10_10_10_REV || type == GL_UNSIGNED_INT_2_10_10_10_REV, GL_INVALID_ENUM);
    mglVertexAttribPointer(ctx, MGL_ATTR_COLOR, size, type, integerType(type) ||
                           type == GL_INT_2_10_10_10_REV || type == GL_UNSIGNED_INT_2_10_10_10_REV, stride, ptr);
}

static void cSecondaryColorPointer(GLMContext ctx, GLint size, GLenum type, GLsizei stride, const void *ptr)
{
    ERROR_CHECK_RETURN(size == 3, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(integerType(type) || type == GL_FLOAT || type == GL_DOUBLE || type == GL_HALF_FLOAT,
                       GL_INVALID_ENUM);
    mglVertexAttribPointer(ctx, MGL_ATTR_SECONDARY, size, type, integerType(type), stride, ptr);
}

static void cTexCoordPointer(GLMContext ctx, GLint size, GLenum type, GLsizei stride, const void *ptr)
{
    ERROR_CHECK_RETURN(size >= 1 && size <= 4, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(type == GL_SHORT || type == GL_INT || type == GL_FLOAT || type == GL_DOUBLE ||
                       type == GL_HALF_FLOAT || type == GL_INT_2_10_10_10_REV ||
                       type == GL_UNSIGNED_INT_2_10_10_10_REV, GL_INVALID_ENUM);
    mglVertexAttribPointer(ctx, MGL_ATTR_TEXCOORD0 + C->client_active_texture, size, type, GL_FALSE, stride, ptr);
}

static void cFogCoordPointer(GLMContext ctx, GLenum type, GLsizei stride, const void *ptr)
{
    ERROR_CHECK_RETURN(type == GL_FLOAT || type == GL_DOUBLE || type == GL_HALF_FLOAT, GL_INVALID_ENUM);
    mglVertexAttribPointer(ctx, MGL_ATTR_FOG, 1, type, GL_FALSE, stride, ptr);
}

static void cEdgeFlagPointer(GLMContext ctx, GLsizei stride, const void *ptr)
{
    ERROR_CHECK_RETURN(stride >= 0, GL_INVALID_VALUE);
    C->edge_flag_stride = stride;
    C->edge_flag_pointer = ptr;
}

static void cIndexPointer(GLMContext ctx, GLenum type, GLsizei stride, const void *ptr)
{
    ERROR_CHECK_RETURN(stride >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(type == GL_UNSIGNED_BYTE || type == GL_SHORT || type == GL_INT || type == GL_FLOAT ||
                       type == GL_DOUBLE, GL_INVALID_ENUM);
    (void)ptr;
}

// glInterleavedArrays' formats, from the table in the specification
static void cInterleavedArrays(GLMContext ctx, GLenum format, GLsizei stride, const void *pointer)
{
    const GLubyte *p = (const GLubyte *)pointer;
    int tc = 0, cc = 0, vc = 0, tex = 0, col = 0, nrm = 0;
    GLenum ctype = GL_FLOAT;
    GLsizei f = (GLsizei)sizeof(GLfloat), c4ub = 4, pc = 0, pn = 0, pv = 0, s;

    ERROR_CHECK_RETURN(stride >= 0, GL_INVALID_VALUE);

    switch (format)
    {
        case GL_V2F:             vc = 2; pv = 0; s = 2 * f; break;
        case GL_V3F:             vc = 3; pv = 0; s = 3 * f; break;
        case GL_C4UB_V2F:        col = 1; cc = 4; ctype = GL_UNSIGNED_BYTE; vc = 2; pv = c4ub; s = c4ub + 2 * f; break;
        case GL_C4UB_V3F:        col = 1; cc = 4; ctype = GL_UNSIGNED_BYTE; vc = 3; pv = c4ub; s = c4ub + 3 * f; break;
        case GL_C3F_V3F:         col = 1; cc = 3; vc = 3; pv = 3 * f; s = 6 * f; break;
        case GL_N3F_V3F:         nrm = 1; vc = 3; pv = 3 * f; s = 6 * f; break;
        case GL_C4F_N3F_V3F:     col = 1; cc = 4; nrm = 1; pn = 4 * f; vc = 3; pv = 7 * f; s = 10 * f; break;
        case GL_T2F_V3F:         tex = 1; tc = 2; vc = 3; pv = 2 * f; s = 5 * f; break;
        case GL_T4F_V4F:         tex = 1; tc = 4; vc = 4; pv = 4 * f; s = 8 * f; break;
        case GL_T2F_C4UB_V3F:    tex = 1; tc = 2; col = 1; cc = 4; ctype = GL_UNSIGNED_BYTE; pc = 2 * f;
                                 vc = 3; pv = c4ub + 2 * f; s = c4ub + 5 * f; break;
        case GL_T2F_C3F_V3F:     tex = 1; tc = 2; col = 1; cc = 3; pc = 2 * f; vc = 3; pv = 5 * f; s = 8 * f; break;
        case GL_T2F_N3F_V3F:     tex = 1; tc = 2; nrm = 1; pn = 2 * f; vc = 3; pv = 5 * f; s = 8 * f; break;
        case GL_T2F_C4F_N3F_V3F: tex = 1; tc = 2; col = 1; cc = 4; pc = 2 * f; nrm = 1; pn = 6 * f;
                                 vc = 3; pv = 9 * f; s = 12 * f; break;
        case GL_T4F_C4F_N3F_V4F: tex = 1; tc = 4; col = 1; cc = 4; pc = 4 * f; nrm = 1; pn = 8 * f;
                                 vc = 4; pv = 11 * f; s = 15 * f; break;
        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    if (stride == 0)
        stride = s;

    clientState(ctx, GL_EDGE_FLAG_ARRAY, GL_FALSE);
    clientState(ctx, GL_INDEX_ARRAY, GL_FALSE);
    clientState(ctx, GL_SECONDARY_COLOR_ARRAY, GL_FALSE);
    clientState(ctx, GL_FOG_COORD_ARRAY, GL_FALSE);

    if (tex)
    {
        clientState(ctx, GL_TEXTURE_COORD_ARRAY, GL_TRUE);
        cTexCoordPointer(ctx, tc, GL_FLOAT, stride, p);
    }
    else
    {
        clientState(ctx, GL_TEXTURE_COORD_ARRAY, GL_FALSE);
    }

    if (col)
    {
        clientState(ctx, GL_COLOR_ARRAY, GL_TRUE);
        cColorPointer(ctx, cc, ctype, stride, p + pc);
    }
    else
    {
        clientState(ctx, GL_COLOR_ARRAY, GL_FALSE);
    }

    if (nrm)
    {
        clientState(ctx, GL_NORMAL_ARRAY, GL_TRUE);
        cNormalPointer(ctx, GL_FLOAT, stride, p + pn);
    }
    else
    {
        clientState(ctx, GL_NORMAL_ARRAY, GL_FALSE);
    }

    clientState(ctx, GL_VERTEX_ARRAY, GL_TRUE);
    cVertexPointer(ctx, vc, GL_FLOAT, stride, p + pv);
}

static int pointerSlot(GLMContext ctx, GLenum pname)
{
    switch (pname)
    {
        case GL_VERTEX_ARRAY_POINTER: return MGL_ATTR_VERTEX;
        case GL_NORMAL_ARRAY_POINTER: return MGL_ATTR_NORMAL;
        case GL_COLOR_ARRAY_POINTER: return MGL_ATTR_COLOR;
        case GL_SECONDARY_COLOR_ARRAY_POINTER: return MGL_ATTR_SECONDARY;
        case GL_FOG_COORD_ARRAY_POINTER: return MGL_ATTR_FOG;
        case GL_TEXTURE_COORD_ARRAY_POINTER: return MGL_ATTR_TEXCOORD0 + (int)C->client_active_texture;
    }

    return -1;
}

static void cGetPointerv(GLMContext ctx, GLenum pname, void **params)
{
    int slot = pointerSlot(ctx, pname);

    if (slot >= 0)
    {
        *params = STATE(vao) ? (void *)STATE(vao)->attrib[slot].pointer : NULL;
        return;
    }

    switch (pname)
    {
        case GL_EDGE_FLAG_ARRAY_POINTER: *params = (void *)C->edge_flag_pointer; return;
        case GL_INDEX_ARRAY_POINTER: *params = NULL; return;
        case GL_SELECTION_BUFFER_POINTER: *params = C->select_buffer; return;
        case GL_FEEDBACK_BUFFER_POINTER: *params = C->feedback_buffer; return;
    }

    C->next.get_pointerv(ctx, pname, params);
}

int mglCompatArrayQuery(GLMContext ctx, GLenum pname, GLdouble *out)
{
    VertexArray *vao = STATE(vao);
    int slot = -1, what = 0;   // 1 size, 2 type, 3 stride, 4 buffer

    switch (pname)
    {
        case GL_VERTEX_ARRAY_SIZE: slot = MGL_ATTR_VERTEX; what = 1; break;
        case GL_VERTEX_ARRAY_TYPE: slot = MGL_ATTR_VERTEX; what = 2; break;
        case GL_VERTEX_ARRAY_STRIDE: slot = MGL_ATTR_VERTEX; what = 3; break;
        case GL_VERTEX_ARRAY_BUFFER_BINDING: slot = MGL_ATTR_VERTEX; what = 4; break;
        case GL_NORMAL_ARRAY_TYPE: slot = MGL_ATTR_NORMAL; what = 2; break;
        case GL_NORMAL_ARRAY_STRIDE: slot = MGL_ATTR_NORMAL; what = 3; break;
        case GL_NORMAL_ARRAY_BUFFER_BINDING: slot = MGL_ATTR_NORMAL; what = 4; break;
        case GL_COLOR_ARRAY_SIZE: slot = MGL_ATTR_COLOR; what = 1; break;
        case GL_COLOR_ARRAY_TYPE: slot = MGL_ATTR_COLOR; what = 2; break;
        case GL_COLOR_ARRAY_STRIDE: slot = MGL_ATTR_COLOR; what = 3; break;
        case GL_COLOR_ARRAY_BUFFER_BINDING: slot = MGL_ATTR_COLOR; what = 4; break;
        case GL_SECONDARY_COLOR_ARRAY_SIZE: slot = MGL_ATTR_SECONDARY; what = 1; break;
        case GL_SECONDARY_COLOR_ARRAY_TYPE: slot = MGL_ATTR_SECONDARY; what = 2; break;
        case GL_SECONDARY_COLOR_ARRAY_STRIDE: slot = MGL_ATTR_SECONDARY; what = 3; break;
        case GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING: slot = MGL_ATTR_SECONDARY; what = 4; break;
        case GL_FOG_COORD_ARRAY_TYPE: slot = MGL_ATTR_FOG; what = 2; break;
        case GL_FOG_COORD_ARRAY_STRIDE: slot = MGL_ATTR_FOG; what = 3; break;
        case GL_FOG_COORD_ARRAY_BUFFER_BINDING: slot = MGL_ATTR_FOG; what = 4; break;
        case GL_TEXTURE_COORD_ARRAY_SIZE: slot = MGL_ATTR_TEXCOORD0 + (int)C->client_active_texture; what = 1; break;
        case GL_TEXTURE_COORD_ARRAY_TYPE: slot = MGL_ATTR_TEXCOORD0 + (int)C->client_active_texture; what = 2; break;
        case GL_TEXTURE_COORD_ARRAY_STRIDE: slot = MGL_ATTR_TEXCOORD0 + (int)C->client_active_texture; what = 3; break;
        case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING:
            slot = MGL_ATTR_TEXCOORD0 + (int)C->client_active_texture; what = 4; break;
        case GL_EDGE_FLAG_ARRAY_STRIDE: out[0] = C->edge_flag_stride; return 1;
        case GL_EDGE_FLAG_ARRAY_BUFFER_BINDING: out[0] = 0; return 1;
        case GL_INDEX_ARRAY_TYPE: out[0] = GL_FLOAT; return 1;
        case GL_INDEX_ARRAY_STRIDE: out[0] = 0; return 1;
        default:
            return 0;
    }

    if (vao == NULL)
    {
        out[0] = 0;
        return 1;
    }

    switch (what)
    {
        case 1: out[0] = vao->attrib[slot].size; break;
        case 2: out[0] = vao->attrib[slot].type; break;
        case 3: out[0] = vao->attrib[slot].stride; break;
        default:
        {
            Buffer *b = vao->bindings[vao->attrib[slot].buffer_bindingindex].buffer;
            out[0] = b ? b->name : 0;
            break;
        }
    }

    return 1;
}

// --- vertex array objects --------------------------------------------------------

void mglBindVertexArray(GLMContext ctx, GLuint array);
void mglDeleteVertexArrays(GLMContext ctx, GLsizei n, const GLuint *arrays);

static void cBindVertexArray(GLMContext ctx, GLuint array)
{
    if (array == 0)
    {
        if (STATE(vao) != C->default_vao)
        {
            STATE(vao) = C->default_vao;
            STATE(dirty_bits) |= DIRTY_VAO;
        }
        return;
    }

    C->next.bind_vertex_array(ctx, array);
}

static void cDeleteVertexArrays(GLMContext ctx, GLsizei n, const GLuint *arrays)
{
    C->next.delete_vertex_arrays(ctx, n, arrays);

    // the core version unbinds to nothing
    if (STATE(vao) == NULL)
        cBindVertexArray(ctx, 0);
}

#define SET(field, fn) ctx->dispatch.field = fn

void mglCompatInstallArrays(GLMContext ctx)
{
#define COLOR_SET(sfx) SET(color3##sfx, cColor3##sfx); SET(color3##sfx##v, cColor3##sfx##v); \
    SET(color4##sfx, cColor4##sfx); SET(color4##sfx##v, cColor4##sfx##v); \
    SET(secondary_color3##sfx, cSecondaryColor3##sfx); SET(secondary_color3##sfx##v, cSecondaryColor3##sfx##v)

    COLOR_SET(b); COLOR_SET(ub); COLOR_SET(s); COLOR_SET(us);
    COLOR_SET(i); COLOR_SET(ui); COLOR_SET(f); COLOR_SET(d);

#define NORMAL_SET(sfx) SET(normal3##sfx, cNormal3##sfx); SET(normal3##sfx##v, cNormal3##sfx##v)

    NORMAL_SET(b); NORMAL_SET(s); NORMAL_SET(i); NORMAL_SET(f); NORMAL_SET(d);

#define TEX_SET(sfx) \
    SET(tex_coord1##sfx, cTexCoord1##sfx); SET(tex_coord1##sfx##v, cTexCoord1##sfx##v); \
    SET(tex_coord2##sfx, cTexCoord2##sfx); SET(tex_coord2##sfx##v, cTexCoord2##sfx##v); \
    SET(tex_coord3##sfx, cTexCoord3##sfx); SET(tex_coord3##sfx##v, cTexCoord3##sfx##v); \
    SET(tex_coord4##sfx, cTexCoord4##sfx); SET(tex_coord4##sfx##v, cTexCoord4##sfx##v); \
    SET(multi_tex_coord1##sfx, cMultiTexCoord1##sfx); SET(multi_tex_coord1##sfx##v, cMultiTexCoord1##sfx##v); \
    SET(multi_tex_coord2##sfx, cMultiTexCoord2##sfx); SET(multi_tex_coord2##sfx##v, cMultiTexCoord2##sfx##v); \
    SET(multi_tex_coord3##sfx, cMultiTexCoord3##sfx); SET(multi_tex_coord3##sfx##v, cMultiTexCoord3##sfx##v); \
    SET(multi_tex_coord4##sfx, cMultiTexCoord4##sfx); SET(multi_tex_coord4##sfx##v, cMultiTexCoord4##sfx##v)

    TEX_SET(s); TEX_SET(i); TEX_SET(f); TEX_SET(d);

    SET(fog_coordf, cFogCoordf);
    SET(fog_coordfv, cFogCoordfv);
    SET(fog_coordd, cFogCoordd);
    SET(fog_coorddv, cFogCoorddv);

    SET(indexd, cIndexd); SET(indexdv, cIndexdv);
    SET(indexf, cIndexf); SET(indexfv, cIndexfv);
    SET(indexi, cIndexi); SET(indexiv, cIndexiv);
    SET(indexs, cIndexs); SET(indexsv, cIndexsv);
    SET(indexub, cIndexub); SET(indexubv, cIndexubv);

    SET(enable_client_state, cEnableClientState);
    SET(disable_client_state, cDisableClientState);
    SET(client_active_texture, cClientActiveTexture);
    SET(vertex_pointer, cVertexPointer);
    SET(normal_pointer, cNormalPointer);
    SET(color_pointer, cColorPointer);
    SET(secondary_color_pointer, cSecondaryColorPointer);
    SET(tex_coord_pointer, cTexCoordPointer);
    SET(fog_coord_pointer, cFogCoordPointer);
    SET(edge_flag_pointer, cEdgeFlagPointer);
    SET(index_pointer, cIndexPointer);
    SET(interleaved_arrays, cInterleavedArrays);
    SET(get_pointerv, cGetPointerv);
    SET(bind_vertex_array, cBindVertexArray);
    SET(delete_vertex_arrays, cDeleteVertexArrays);

    C->default_vao = newVAO(ctx, 0);
    C->imm_vao = newVAO(ctx, 0);
    C->raster_vao = newVAO(ctx, 0);

    if (STATE(vao) == NULL)
        STATE(vao) = C->default_vao;

    // the legacy current values start where GL says
    mglCompatSetCurrent(ctx, MGL_ATTR_COLOR, 1.0f, 1.0f, 1.0f, 1.0f);
    mglCompatSetCurrent(ctx, MGL_ATTR_NORMAL, 0.0f, 0.0f, 1.0f, 1.0f);
}

void mglReleaseBufferStorage(GLMContext ctx, Buffer *ptr);

void mglCompatFreeArrays(GLMContext ctx)
{
    for (int b = 0; b < 16; b++)
    {
        if (C->staging[b])
        {
            mglReleaseBufferStorage(ctx, C->staging[b]);
            free(C->staging[b]);
            C->staging[b] = NULL;
        }
    }

    if (STATE(vao) == C->default_vao || STATE(vao) == C->imm_vao || STATE(vao) == C->raster_vao)
        STATE(vao) = NULL;

    if (C->stream)
    {
        mglReleaseBufferStorage(ctx, C->stream);
        free(C->stream);
        C->stream = NULL;
    }

    if (C->imm_buf)
    {
        mglReleaseBufferStorage(ctx, C->imm_buf);
        free(C->imm_buf);
        C->imm_buf = NULL;
    }

    free(C->default_vao);
    free(C->imm_vao);
    free(C->raster_vao);
    C->default_vao = C->imm_vao = C->raster_vao = NULL;
}

#endif /* MGL_COMPAT_CONTEXTS */
