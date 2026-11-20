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
 * compat_state.c
 * MGL
 *
 * The compatibility profile's state: the matrix stacks, lights, materials,
 * texture environments, fog, and the enables and queries that go with them.
 * Only a compatibility context has any of it; its entries replace the core
 * ones in that context's dispatch table and forward what they do not know.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "glm_context.h"
#include "compat.h"
#include "moogl_version.h"

#ifdef MGL_COMPAT_CONTEXTS

#define C (ctx->compat)

// --- matrices --------------------------------------------------------------

void mglMatIdentity(GLfloat *m)
{
    memset(m, 0, 16 * sizeof(GLfloat));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

// r = a * b, column major; r may be a or b
void mglMatMul(GLfloat *r, const GLfloat *a, const GLfloat *b)
{
    GLfloat t[16];

    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++)
            t[c * 4 + row] = a[row] * b[c * 4] + a[4 + row] * b[c * 4 + 1] +
                             a[8 + row] * b[c * 4 + 2] + a[12 + row] * b[c * 4 + 3];

    memcpy(r, t, sizeof(t));
}

void mglMatTransform(const GLfloat *m, const GLfloat *v, GLfloat *out)
{
    GLfloat t[4];

    for (int row = 0; row < 4; row++)
        t[row] = m[row] * v[0] + m[4 + row] * v[1] + m[8 + row] * v[2] + m[12 + row] * v[3];

    memcpy(out, t, sizeof(t));
}

bool mglMatInvert(const GLfloat *m, GLfloat *out)
{
    double inv[16], det;

    inv[0] = m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8] = m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5] = m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    inv[2] = m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
    inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
    inv[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
    inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
    inv[7] = m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
    inv[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0.0)
    {
        mglMatIdentity(out);
        return false;
    }

    for (int i = 0; i < 16; i++)
        out[i] = (GLfloat)(inv[i] / det);

    return true;
}

GLfloat *mglCompatModelview(GLMContext ctx)
{
    return C->modelview.s[C->modelview.depth].m;
}

GLfloat *mglCompatProjection(GLMContext ctx)
{
    return C->projection.s[C->projection.depth].m;
}

GLfloat *mglCompatTextureMatrix(GLMContext ctx, GLuint unit)
{
    return C->texture[unit].s[C->texture[unit].depth].m;
}

static MGLMatrixStack *currentStack(GLMContext ctx)
{
    switch (C->matrix_mode)
    {
        case GL_MODELVIEW:  return &C->modelview;
        case GL_PROJECTION: return &C->projection;
        case GL_TEXTURE:
            if (STATE(active_texture) < MGL_COMPAT_UNITS)
                return &C->texture[STATE(active_texture)];
            return NULL;
    }

    return NULL;
}

static GLfloat *currentMatrix(GLMContext ctx)
{
    MGLMatrixStack *s = currentStack(ctx);

    return s ? s->s[s->depth].m : NULL;
}

#define NOT_IN_BEGIN() ERROR_CHECK_RETURN(!C->in_begin, GL_INVALID_OPERATION)

static void matrixChanged(GLMContext ctx)
{
    C->matrix_serial++;
}

static void loadMatrix(GLMContext ctx, const GLfloat *m)
{
    GLfloat *cur;

    NOT_IN_BEGIN();
    cur = currentMatrix(ctx);
    if (!cur)
        return;

    memcpy(cur, m, 16 * sizeof(GLfloat));
    matrixChanged(ctx);
}

static void multMatrix(GLMContext ctx, const GLfloat *m)
{
    GLfloat *cur;

    NOT_IN_BEGIN();
    cur = currentMatrix(ctx);
    if (!cur)
        return;

    mglMatMul(cur, cur, m);
    matrixChanged(ctx);
}

static void transpose(const GLfloat *m, GLfloat *t)
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            t[c * 4 + r] = m[r * 4 + c];
}

static void cMatrixMode(GLMContext ctx, GLenum mode)
{
    NOT_IN_BEGIN();

    switch (mode)
    {
        case GL_MODELVIEW:
        case GL_PROJECTION:
        case GL_TEXTURE:
            C->matrix_mode = mode;
            return;
    }

    ERROR_RETURN(GL_INVALID_ENUM);
}

static void cLoadIdentity(GLMContext ctx)
{
    GLfloat m[16];

    mglMatIdentity(m);
    loadMatrix(ctx, m);
}

static void cLoadMatrixf(GLMContext ctx, const GLfloat *m)
{
    loadMatrix(ctx, m);
}

static void cLoadMatrixd(GLMContext ctx, const GLdouble *m)
{
    GLfloat f[16];

    for (int i = 0; i < 16; i++)
        f[i] = (GLfloat)m[i];

    loadMatrix(ctx, f);
}

static void cMultMatrixf(GLMContext ctx, const GLfloat *m)
{
    multMatrix(ctx, m);
}

static void cMultMatrixd(GLMContext ctx, const GLdouble *m)
{
    GLfloat f[16];

    for (int i = 0; i < 16; i++)
        f[i] = (GLfloat)m[i];

    multMatrix(ctx, f);
}

static void cLoadTransposeMatrixf(GLMContext ctx, const GLfloat *m)
{
    GLfloat t[16];

    transpose(m, t);
    loadMatrix(ctx, t);
}

static void cLoadTransposeMatrixd(GLMContext ctx, const GLdouble *m)
{
    GLfloat f[16], t[16];

    for (int i = 0; i < 16; i++)
        f[i] = (GLfloat)m[i];

    transpose(f, t);
    loadMatrix(ctx, t);
}

static void cMultTransposeMatrixf(GLMContext ctx, const GLfloat *m)
{
    GLfloat t[16];

    transpose(m, t);
    multMatrix(ctx, t);
}

static void cMultTransposeMatrixd(GLMContext ctx, const GLdouble *m)
{
    GLfloat f[16], t[16];

    for (int i = 0; i < 16; i++)
        f[i] = (GLfloat)m[i];

    transpose(f, t);
    multMatrix(ctx, t);
}

static void cPushMatrix(GLMContext ctx)
{
    MGLMatrixStack *s;

    NOT_IN_BEGIN();
    s = currentStack(ctx);
    if (!s)
        return;

    ERROR_CHECK_RETURN(s->depth + 1 < s->max, GL_STACK_OVERFLOW);

    s->s[s->depth + 1] = s->s[s->depth];
    s->depth++;
}

static void cPopMatrix(GLMContext ctx)
{
    MGLMatrixStack *s;

    NOT_IN_BEGIN();
    s = currentStack(ctx);
    if (!s)
        return;

    ERROR_CHECK_RETURN(s->depth > 0, GL_STACK_UNDERFLOW);

    s->depth--;
    matrixChanged(ctx);
}

static void translateMatrix(GLMContext ctx, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat m[16];

    mglMatIdentity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
    multMatrix(ctx, m);
}

static void scaleMatrix(GLMContext ctx, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat m[16];

    mglMatIdentity(m);
    m[0] = x;
    m[5] = y;
    m[10] = z;
    multMatrix(ctx, m);
}

static void rotateMatrix(GLMContext ctx, GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat m[16];
    GLfloat len = sqrtf(x * x + y * y + z * z);
    GLfloat r = angle * (GLfloat)M_PI / 180.0f;
    GLfloat c = cosf(r), s = sinf(r), t = 1.0f - c;

    if (len == 0.0f)
    {
        NOT_IN_BEGIN();
        return;
    }

    x /= len;
    y /= len;
    z /= len;

    mglMatIdentity(m);
    m[0] = x * x * t + c;      m[4] = x * y * t - z * s;  m[8] = x * z * t + y * s;
    m[1] = y * x * t + z * s;  m[5] = y * y * t + c;      m[9] = y * z * t - x * s;
    m[2] = x * z * t - y * s;  m[6] = y * z * t + x * s;  m[10] = z * z * t + c;
    multMatrix(ctx, m);
}

static void cTranslatef(GLMContext ctx, GLfloat x, GLfloat y, GLfloat z) { translateMatrix(ctx, x, y, z); }
static void cTranslated(GLMContext ctx, GLdouble x, GLdouble y, GLdouble z) { translateMatrix(ctx, (GLfloat)x, (GLfloat)y, (GLfloat)z); }
static void cScalef(GLMContext ctx, GLfloat x, GLfloat y, GLfloat z) { scaleMatrix(ctx, x, y, z); }
static void cScaled(GLMContext ctx, GLdouble x, GLdouble y, GLdouble z) { scaleMatrix(ctx, (GLfloat)x, (GLfloat)y, (GLfloat)z); }
static void cRotatef(GLMContext ctx, GLfloat a, GLfloat x, GLfloat y, GLfloat z) { rotateMatrix(ctx, a, x, y, z); }
static void cRotated(GLMContext ctx, GLdouble a, GLdouble x, GLdouble y, GLdouble z) { rotateMatrix(ctx, (GLfloat)a, (GLfloat)x, (GLfloat)y, (GLfloat)z); }

static void cOrtho(GLMContext ctx, GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
    GLfloat m[16];

    ERROR_CHECK_RETURN(l != r && b != t && n != f, GL_INVALID_VALUE);

    mglMatIdentity(m);
    m[0] = (GLfloat)(2.0 / (r - l));
    m[5] = (GLfloat)(2.0 / (t - b));
    m[10] = (GLfloat)(-2.0 / (f - n));
    m[12] = (GLfloat)(-(r + l) / (r - l));
    m[13] = (GLfloat)(-(t + b) / (t - b));
    m[14] = (GLfloat)(-(f + n) / (f - n));
    multMatrix(ctx, m);
}

static void cFrustum(GLMContext ctx, GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
    GLfloat m[16];

    ERROR_CHECK_RETURN(n > 0 && f > 0 && l != r && b != t && n != f, GL_INVALID_VALUE);

    memset(m, 0, sizeof(m));
    m[0] = (GLfloat)(2.0 * n / (r - l));
    m[5] = (GLfloat)(2.0 * n / (t - b));
    m[8] = (GLfloat)((r + l) / (r - l));
    m[9] = (GLfloat)((t + b) / (t - b));
    m[10] = (GLfloat)(-(f + n) / (f - n));
    m[11] = -1.0f;
    m[14] = (GLfloat)(-2.0 * f * n / (f - n));
    multMatrix(ctx, m);
}

// --- lights and materials ----------------------------------------------------

static void defaultLight(MGLLight *l, int i)
{
    memset(l, 0, sizeof(*l));
    l->ambient[3] = 1.0f;
    l->diffuse[3] = 1.0f;
    l->specular[3] = 1.0f;

    if (i == 0)
    {
        l->diffuse[0] = l->diffuse[1] = l->diffuse[2] = 1.0f;
        l->specular[0] = l->specular[1] = l->specular[2] = 1.0f;
    }

    l->position[2] = 1.0f;
    l->spot_direction[2] = -1.0f;
    l->spot_cutoff = 180.0f;
    l->attenuation[0] = 1.0f;
}

static void defaultMaterial(MGLMaterial *m)
{
    memset(m, 0, sizeof(*m));
    m->ambient[0] = m->ambient[1] = m->ambient[2] = 0.2f;
    m->ambient[3] = 1.0f;
    m->diffuse[0] = m->diffuse[1] = m->diffuse[2] = 0.8f;
    m->diffuse[3] = 1.0f;
    m->specular[3] = 1.0f;
    m->emission[3] = 1.0f;
}

static int pnameCount(GLenum pname)
{
    switch (pname)
    {
        case GL_AMBIENT: case GL_DIFFUSE: case GL_SPECULAR: case GL_POSITION: case GL_EMISSION:
        case GL_AMBIENT_AND_DIFFUSE:
            return 4;
        case GL_SPOT_DIRECTION: case GL_COLOR_INDEXES:
            return 3;
    }

    return 1;
}

static void lightv(GLMContext ctx, GLenum light, GLenum pname, const GLfloat *p)
{
    C->values_serial++;
    MGLLight *l;

    ERROR_CHECK_RETURN(light >= GL_LIGHT0 && light < GL_LIGHT0 + MGL_COMPAT_LIGHTS, GL_INVALID_ENUM);
    l = &C->lights[light - GL_LIGHT0];

    switch (pname)
    {
        case GL_AMBIENT:  memcpy(l->ambient, p, 4 * sizeof(GLfloat)); break;
        case GL_DIFFUSE:  memcpy(l->diffuse, p, 4 * sizeof(GLfloat)); break;
        case GL_SPECULAR: memcpy(l->specular, p, 4 * sizeof(GLfloat)); break;
        case GL_POSITION:
            mglMatTransform(mglCompatModelview(ctx), p, l->position);
            break;
        case GL_SPOT_DIRECTION:
        {
            GLfloat d[4] = { p[0], p[1], p[2], 0.0f };
            mglMatTransform(mglCompatModelview(ctx), d, l->spot_direction);
            break;
        }
        case GL_SPOT_EXPONENT:
            ERROR_CHECK_RETURN(p[0] >= 0.0f && p[0] <= 128.0f, GL_INVALID_VALUE);
            l->spot_exponent = p[0];
            break;
        case GL_SPOT_CUTOFF:
            ERROR_CHECK_RETURN((p[0] >= 0.0f && p[0] <= 90.0f) || p[0] == 180.0f, GL_INVALID_VALUE);
            l->spot_cutoff = p[0];
            break;
        case GL_CONSTANT_ATTENUATION:
        case GL_LINEAR_ATTENUATION:
        case GL_QUADRATIC_ATTENUATION:
            ERROR_CHECK_RETURN(p[0] >= 0.0f, GL_INVALID_VALUE);
            l->attenuation[pname - GL_CONSTANT_ATTENUATION] = p[0];
            break;
        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

static void cLightf(GLMContext ctx, GLenum light, GLenum pname, GLfloat param)
{
    ERROR_CHECK_RETURN(pnameCount(pname) == 1, GL_INVALID_ENUM);
    lightv(ctx, light, pname, &param);
}

static void cLightfv(GLMContext ctx, GLenum light, GLenum pname, const GLfloat *params)
{
    lightv(ctx, light, pname, params);
}

static void cLighti(GLMContext ctx, GLenum light, GLenum pname, GLint param)
{
    GLfloat f = (GLfloat)param;

    ERROR_CHECK_RETURN(pnameCount(pname) == 1, GL_INVALID_ENUM);
    lightv(ctx, light, pname, &f);
}

// integer colours map their range onto [-1, 1]
static GLfloat intColor(GLint v)
{
    return (GLfloat)((2.0 * v + 1.0) / 4294967295.0);
}

static void cLightiv(GLMContext ctx, GLenum light, GLenum pname, const GLint *params)
{
    GLfloat f[4];
    int n = pnameCount(pname);
    bool color = pname == GL_AMBIENT || pname == GL_DIFFUSE || pname == GL_SPECULAR;

    for (int i = 0; i < n; i++)
        f[i] = color ? intColor(params[i]) : (GLfloat)params[i];

    lightv(ctx, light, pname, f);
}

static void getLight(GLMContext ctx, GLenum light, GLenum pname, GLfloat *out)
{
    MGLLight *l;
    GLfloat inv[16];

    ERROR_CHECK_RETURN(light >= GL_LIGHT0 && light < GL_LIGHT0 + MGL_COMPAT_LIGHTS, GL_INVALID_ENUM);
    l = &C->lights[light - GL_LIGHT0];

    switch (pname)
    {
        case GL_AMBIENT:  memcpy(out, l->ambient, 4 * sizeof(GLfloat)); break;
        case GL_DIFFUSE:  memcpy(out, l->diffuse, 4 * sizeof(GLfloat)); break;
        case GL_SPECULAR: memcpy(out, l->specular, 4 * sizeof(GLfloat)); break;
        // returned in eye coordinates, as stored
        case GL_POSITION: memcpy(out, l->position, 4 * sizeof(GLfloat)); break;
        case GL_SPOT_DIRECTION: memcpy(out, l->spot_direction, 3 * sizeof(GLfloat)); break;
        case GL_SPOT_EXPONENT: out[0] = l->spot_exponent; break;
        case GL_SPOT_CUTOFF: out[0] = l->spot_cutoff; break;
        case GL_CONSTANT_ATTENUATION:
        case GL_LINEAR_ATTENUATION:
        case GL_QUADRATIC_ATTENUATION:
            out[0] = l->attenuation[pname - GL_CONSTANT_ATTENUATION];
            break;
        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    (void)inv;
}

static void cGetLightfv(GLMContext ctx, GLenum light, GLenum pname, GLfloat *params)
{
    getLight(ctx, light, pname, params);
}

static void cGetLightiv(GLMContext ctx, GLenum light, GLenum pname, GLint *params)
{
    GLfloat f[4] = { 0 };
    int n = pnameCount(pname);

    getLight(ctx, light, pname, f);

    for (int i = 0; i < n; i++)
        params[i] = (GLint)lroundf(f[i]);
}

static void materialv(GLMContext ctx, GLenum face, GLenum pname, const GLfloat *p)
{
    C->values_serial++;
    int first, last;

    switch (face)
    {
        case GL_FRONT: first = last = 0; break;
        case GL_BACK: first = last = 1; break;
        case GL_FRONT_AND_BACK: first = 0; last = 1; break;
        default: ERROR_RETURN(GL_INVALID_ENUM);
    }

    for (int f = first; f <= last; f++)
    {
        MGLMaterial *m = &C->material[f];

        switch (pname)
        {
            case GL_AMBIENT:  memcpy(m->ambient, p, 4 * sizeof(GLfloat)); break;
            case GL_DIFFUSE:  memcpy(m->diffuse, p, 4 * sizeof(GLfloat)); break;
            case GL_SPECULAR: memcpy(m->specular, p, 4 * sizeof(GLfloat)); break;
            case GL_EMISSION: memcpy(m->emission, p, 4 * sizeof(GLfloat)); break;
            case GL_AMBIENT_AND_DIFFUSE:
                memcpy(m->ambient, p, 4 * sizeof(GLfloat));
                memcpy(m->diffuse, p, 4 * sizeof(GLfloat));
                break;
            case GL_SHININESS:
                ERROR_CHECK_RETURN(p[0] >= 0.0f && p[0] <= 128.0f, GL_INVALID_VALUE);
                m->shininess = p[0];
                break;
            case GL_COLOR_INDEXES:
                break;
            default:
                ERROR_RETURN(GL_INVALID_ENUM);
        }
    }
}

static void cMaterialf(GLMContext ctx, GLenum face, GLenum pname, GLfloat param)
{
    ERROR_CHECK_RETURN(pname == GL_SHININESS, GL_INVALID_ENUM);
    materialv(ctx, face, pname, &param);
}

static void cMaterialfv(GLMContext ctx, GLenum face, GLenum pname, const GLfloat *params)
{
    materialv(ctx, face, pname, params);
}

static void cMateriali(GLMContext ctx, GLenum face, GLenum pname, GLint param)
{
    GLfloat f = (GLfloat)param;

    ERROR_CHECK_RETURN(pname == GL_SHININESS, GL_INVALID_ENUM);
    materialv(ctx, face, pname, &f);
}

static void cMaterialiv(GLMContext ctx, GLenum face, GLenum pname, const GLint *params)
{
    GLfloat f[4];
    int n = pnameCount(pname);

    for (int i = 0; i < n; i++)
        f[i] = (pname == GL_SHININESS || pname == GL_COLOR_INDEXES) ? (GLfloat)params[i] : intColor(params[i]);

    materialv(ctx, face, pname, f);
}

static void getMaterial(GLMContext ctx, GLenum face, GLenum pname, GLfloat *out)
{
    MGLMaterial *m;

    ERROR_CHECK_RETURN(face == GL_FRONT || face == GL_BACK, GL_INVALID_ENUM);
    m = &C->material[face == GL_BACK];

    switch (pname)
    {
        case GL_AMBIENT:  memcpy(out, m->ambient, 4 * sizeof(GLfloat)); break;
        case GL_DIFFUSE:  memcpy(out, m->diffuse, 4 * sizeof(GLfloat)); break;
        case GL_SPECULAR: memcpy(out, m->specular, 4 * sizeof(GLfloat)); break;
        case GL_EMISSION: memcpy(out, m->emission, 4 * sizeof(GLfloat)); break;
        case GL_SHININESS: out[0] = m->shininess; break;
        case GL_COLOR_INDEXES: out[0] = 0; out[1] = 1; out[2] = 1; break;
        default: ERROR_RETURN(GL_INVALID_ENUM);
    }
}

static void cGetMaterialfv(GLMContext ctx, GLenum face, GLenum pname, GLfloat *params)
{
    getMaterial(ctx, face, pname, params);
}

static void cGetMaterialiv(GLMContext ctx, GLenum face, GLenum pname, GLint *params)
{
    GLfloat f[4] = { 0 };
    int n = pnameCount(pname);

    getMaterial(ctx, face, pname, f);

    for (int i = 0; i < n; i++)
        params[i] = (GLint)lroundf(f[i]);
}

static void lightModelv(GLMContext ctx, GLenum pname, const GLfloat *p)
{
    C->values_serial++;
    switch (pname)
    {
        case GL_LIGHT_MODEL_AMBIENT: memcpy(C->model_ambient, p, 4 * sizeof(GLfloat)); break;
        case GL_LIGHT_MODEL_LOCAL_VIEWER: C->local_viewer = p[0] != 0.0f; break;
        case GL_LIGHT_MODEL_TWO_SIDE: C->two_side = p[0] != 0.0f; break;
        case GL_LIGHT_MODEL_COLOR_CONTROL:
        {
            GLenum v = (GLenum)p[0];
            ERROR_CHECK_RETURN(v == GL_SINGLE_COLOR || v == GL_SEPARATE_SPECULAR_COLOR, GL_INVALID_ENUM);
            C->color_control = v;
            break;
        }
        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

static void cLightModelf(GLMContext ctx, GLenum pname, GLfloat param)
{
    ERROR_CHECK_RETURN(pname != GL_LIGHT_MODEL_AMBIENT, GL_INVALID_ENUM);
    lightModelv(ctx, pname, &param);
}

static void cLightModelfv(GLMContext ctx, GLenum pname, const GLfloat *params)
{
    lightModelv(ctx, pname, params);
}

static void cLightModeli(GLMContext ctx, GLenum pname, GLint param)
{
    GLfloat f = (GLfloat)param;

    ERROR_CHECK_RETURN(pname != GL_LIGHT_MODEL_AMBIENT, GL_INVALID_ENUM);
    lightModelv(ctx, pname, &f);
}

static void cLightModeliv(GLMContext ctx, GLenum pname, const GLint *params)
{
    GLfloat f[4];

    if (pname == GL_LIGHT_MODEL_AMBIENT)
    {
        for (int i = 0; i < 4; i++)
            f[i] = intColor(params[i]);
    }
    else
    {
        f[0] = (GLfloat)params[0];
    }

    lightModelv(ctx, pname, f);
}

static void cColorMaterial(GLMContext ctx, GLenum face, GLenum mode)
{
    ERROR_CHECK_RETURN(face == GL_FRONT || face == GL_BACK || face == GL_FRONT_AND_BACK, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(mode == GL_EMISSION || mode == GL_AMBIENT || mode == GL_DIFFUSE ||
                       mode == GL_SPECULAR || mode == GL_AMBIENT_AND_DIFFUSE, GL_INVALID_ENUM);
    C->color_material_face = face;
    C->color_material_mode = mode;
}

static void cShadeModel(GLMContext ctx, GLenum mode)
{
    ERROR_CHECK_RETURN(mode == GL_FLAT || mode == GL_SMOOTH, GL_INVALID_ENUM);
    C->shade_model = mode;
}

// --- texture environment and coordinate generation ---------------------------

static MGLTexEnv *activeEnv(GLMContext ctx)
{
    return STATE(active_texture) < MGL_COMPAT_UNITS ? &C->tex[STATE(active_texture)] : NULL;
}

static void defaultTexEnv(MGLTexEnv *e)
{
    memset(e, 0, sizeof(*e));
    e->mode = GL_MODULATE;
    e->combine_rgb = e->combine_alpha = GL_MODULATE;
    e->src_rgb[0] = e->src_alpha[0] = GL_TEXTURE;
    e->src_rgb[1] = e->src_alpha[1] = GL_PREVIOUS;
    e->src_rgb[2] = e->src_alpha[2] = GL_CONSTANT;
    e->op_rgb[0] = e->op_rgb[1] = GL_SRC_COLOR;
    e->op_rgb[2] = GL_SRC_ALPHA;
    e->op_alpha[0] = e->op_alpha[1] = e->op_alpha[2] = GL_SRC_ALPHA;
    e->rgb_scale = e->alpha_scale = 1.0f;

    for (int c = 0; c < 4; c++)
        e->gen_mode[c] = GL_EYE_LINEAR;

    e->obj_plane[0][0] = e->eye_plane[0][0] = 1.0f;
    e->obj_plane[1][1] = e->eye_plane[1][1] = 1.0f;
}

static bool validSource(GLenum s)
{
    if (s >= GL_TEXTURE0 && s < GL_TEXTURE0 + MGL_COMPAT_UNITS)
        return true;

    return s == GL_TEXTURE || s == GL_CONSTANT || s == GL_PRIMARY_COLOR || s == GL_PREVIOUS;
}

static void texEnvv(GLMContext ctx, GLenum target, GLenum pname, const GLfloat *p, bool ints)
{
    C->values_serial++;
    MGLTexEnv *e = activeEnv(ctx);
    GLenum v = (GLenum)p[0];

    ERROR_CHECK_RETURN(e, GL_INVALID_OPERATION);

    if (target == GL_POINT_SPRITE)
    {
        ERROR_CHECK_RETURN(pname == GL_COORD_REPLACE, GL_INVALID_ENUM);
        e->coord_replace = p[0] != 0.0f;
        return;
    }

    if (target == GL_TEXTURE_FILTER_CONTROL)
    {
        ERROR_CHECK_RETURN(pname == GL_TEXTURE_LOD_BIAS, GL_INVALID_ENUM);
        e->lod_bias = p[0];
        return;
    }

    ERROR_CHECK_RETURN(target == GL_TEXTURE_ENV, GL_INVALID_ENUM);

    switch (pname)
    {
        case GL_TEXTURE_ENV_MODE:
            ERROR_CHECK_RETURN(v == GL_MODULATE || v == GL_DECAL || v == GL_BLEND || v == GL_REPLACE ||
                               v == GL_ADD || v == GL_COMBINE, GL_INVALID_ENUM);
            e->mode = v;
            break;

        case GL_TEXTURE_ENV_COLOR:
            for (int i = 0; i < 4; i++)
                e->color[i] = ints ? intColor((GLint)p[i]) : p[i];
            break;

        case GL_COMBINE_RGB:
            ERROR_CHECK_RETURN(v == GL_REPLACE || v == GL_MODULATE || v == GL_ADD || v == GL_ADD_SIGNED ||
                               v == GL_INTERPOLATE || v == GL_SUBTRACT || v == GL_DOT3_RGB ||
                               v == GL_DOT3_RGBA, GL_INVALID_ENUM);
            e->combine_rgb = v;
            break;

        case GL_COMBINE_ALPHA:
            ERROR_CHECK_RETURN(v == GL_REPLACE || v == GL_MODULATE || v == GL_ADD || v == GL_ADD_SIGNED ||
                               v == GL_INTERPOLATE || v == GL_SUBTRACT, GL_INVALID_ENUM);
            e->combine_alpha = v;
            break;

        case GL_SRC0_RGB: case GL_SRC1_RGB: case GL_SRC2_RGB:
            ERROR_CHECK_RETURN(validSource(v), GL_INVALID_ENUM);
            e->src_rgb[pname - GL_SRC0_RGB] = v;
            break;

        case GL_SRC0_ALPHA: case GL_SRC1_ALPHA: case GL_SRC2_ALPHA:
            ERROR_CHECK_RETURN(validSource(v), GL_INVALID_ENUM);
            e->src_alpha[pname - GL_SRC0_ALPHA] = v;
            break;

        case GL_OPERAND0_RGB: case GL_OPERAND1_RGB: case GL_OPERAND2_RGB:
            ERROR_CHECK_RETURN(v == GL_SRC_COLOR || v == GL_ONE_MINUS_SRC_COLOR || v == GL_SRC_ALPHA ||
                               v == GL_ONE_MINUS_SRC_ALPHA, GL_INVALID_ENUM);
            e->op_rgb[pname - GL_OPERAND0_RGB] = v;
            break;

        case GL_OPERAND0_ALPHA: case GL_OPERAND1_ALPHA: case GL_OPERAND2_ALPHA:
            ERROR_CHECK_RETURN(v == GL_SRC_ALPHA || v == GL_ONE_MINUS_SRC_ALPHA, GL_INVALID_ENUM);
            e->op_alpha[pname - GL_OPERAND0_ALPHA] = v;
            break;

        case GL_RGB_SCALE:
            ERROR_CHECK_RETURN(p[0] == 1.0f || p[0] == 2.0f || p[0] == 4.0f, GL_INVALID_VALUE);
            e->rgb_scale = p[0];
            break;

        case GL_ALPHA_SCALE:
            ERROR_CHECK_RETURN(p[0] == 1.0f || p[0] == 2.0f || p[0] == 4.0f, GL_INVALID_VALUE);
            e->alpha_scale = p[0];
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

static void cTexEnvf(GLMContext ctx, GLenum target, GLenum pname, GLfloat param)
{
    ERROR_CHECK_RETURN(pname != GL_TEXTURE_ENV_COLOR, GL_INVALID_ENUM);
    texEnvv(ctx, target, pname, &param, false);
}

static void cTexEnvfv(GLMContext ctx, GLenum target, GLenum pname, const GLfloat *params)
{
    texEnvv(ctx, target, pname, params, false);
}

static void cTexEnvi(GLMContext ctx, GLenum target, GLenum pname, GLint param)
{
    GLfloat f = (GLfloat)param;

    ERROR_CHECK_RETURN(pname != GL_TEXTURE_ENV_COLOR, GL_INVALID_ENUM);
    texEnvv(ctx, target, pname, &f, false);
}

static void cTexEnviv(GLMContext ctx, GLenum target, GLenum pname, const GLint *params)
{
    GLfloat f[4];
    int n = pname == GL_TEXTURE_ENV_COLOR ? 4 : 1;

    for (int i = 0; i < n; i++)
        f[i] = (GLfloat)params[i];

    texEnvv(ctx, target, pname, f, pname == GL_TEXTURE_ENV_COLOR);
}

static int getTexEnv(GLMContext ctx, GLenum target, GLenum pname, GLfloat *out)
{
    MGLTexEnv *e = activeEnv(ctx);

    ERROR_CHECK_RETURN_VALUE(e, GL_INVALID_OPERATION, 0);

    if (target == GL_POINT_SPRITE && pname == GL_COORD_REPLACE)
    {
        out[0] = e->coord_replace;
        return 1;
    }

    if (target == GL_TEXTURE_FILTER_CONTROL && pname == GL_TEXTURE_LOD_BIAS)
    {
        out[0] = e->lod_bias;
        return 1;
    }

    ERROR_CHECK_RETURN_VALUE(target == GL_TEXTURE_ENV, GL_INVALID_ENUM, 0);

    switch (pname)
    {
        case GL_TEXTURE_ENV_MODE: out[0] = (GLfloat)e->mode; return 1;
        case GL_TEXTURE_ENV_COLOR: memcpy(out, e->color, 4 * sizeof(GLfloat)); return 4;
        case GL_COMBINE_RGB: out[0] = (GLfloat)e->combine_rgb; return 1;
        case GL_COMBINE_ALPHA: out[0] = (GLfloat)e->combine_alpha; return 1;
        case GL_SRC0_RGB: case GL_SRC1_RGB: case GL_SRC2_RGB:
            out[0] = (GLfloat)e->src_rgb[pname - GL_SRC0_RGB]; return 1;
        case GL_SRC0_ALPHA: case GL_SRC1_ALPHA: case GL_SRC2_ALPHA:
            out[0] = (GLfloat)e->src_alpha[pname - GL_SRC0_ALPHA]; return 1;
        case GL_OPERAND0_RGB: case GL_OPERAND1_RGB: case GL_OPERAND2_RGB:
            out[0] = (GLfloat)e->op_rgb[pname - GL_OPERAND0_RGB]; return 1;
        case GL_OPERAND0_ALPHA: case GL_OPERAND1_ALPHA: case GL_OPERAND2_ALPHA:
            out[0] = (GLfloat)e->op_alpha[pname - GL_OPERAND0_ALPHA]; return 1;
        case GL_RGB_SCALE: out[0] = e->rgb_scale; return 1;
        case GL_ALPHA_SCALE: out[0] = e->alpha_scale; return 1;
    }

    ERROR_RETURN_VALUE(GL_INVALID_ENUM, 0);
}

static void cGetTexEnvfv(GLMContext ctx, GLenum target, GLenum pname, GLfloat *params)
{
    getTexEnv(ctx, target, pname, params);
}

static void cGetTexEnviv(GLMContext ctx, GLenum target, GLenum pname, GLint *params)
{
    GLfloat f[4];
    int n = getTexEnv(ctx, target, pname, f);

    for (int i = 0; i < n; i++)
        params[i] = pname == GL_TEXTURE_ENV_COLOR ? (GLint)(f[i] * 2147483647.0f) : (GLint)f[i];
}

static void texGenv(GLMContext ctx, GLenum coord, GLenum pname, const GLfloat *p)
{
    C->values_serial++;
    MGLTexEnv *e = activeEnv(ctx);
    int c = (int)coord - GL_S;

    ERROR_CHECK_RETURN(e, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(c >= 0 && c < 4, GL_INVALID_ENUM);

    switch (pname)
    {
        case GL_TEXTURE_GEN_MODE:
        {
            GLenum m = (GLenum)p[0];
            bool ok = m == GL_OBJECT_LINEAR || m == GL_EYE_LINEAR ||
                      (m == GL_SPHERE_MAP && c < 2) ||
                      ((m == GL_REFLECTION_MAP || m == GL_NORMAL_MAP) && c < 3);

            ERROR_CHECK_RETURN(ok, GL_INVALID_ENUM);
            e->gen_mode[c] = m;
            break;
        }

        case GL_OBJECT_PLANE:
            memcpy(e->obj_plane[c], p, 4 * sizeof(GLfloat));
            break;

        case GL_EYE_PLANE:
        {
            // stored as p * inverse(modelview), as GL specifies
            GLfloat inv[16];

            mglMatInvert(mglCompatModelview(ctx), inv);

            for (int i = 0; i < 4; i++)
                e->eye_plane[c][i] = p[0] * inv[i * 4] + p[1] * inv[i * 4 + 1] + p[2] * inv[i * 4 + 2] +
                                     p[3] * inv[i * 4 + 3];
            break;
        }

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

static void cTexGenf(GLMContext ctx, GLenum coord, GLenum pname, GLfloat param)
{
    ERROR_CHECK_RETURN(pname == GL_TEXTURE_GEN_MODE, GL_INVALID_ENUM);
    texGenv(ctx, coord, pname, &param);
}

static void cTexGenfv(GLMContext ctx, GLenum coord, GLenum pname, const GLfloat *params)
{
    texGenv(ctx, coord, pname, params);
}

static void cTexGeni(GLMContext ctx, GLenum coord, GLenum pname, GLint param)
{
    GLfloat f = (GLfloat)param;

    ERROR_CHECK_RETURN(pname == GL_TEXTURE_GEN_MODE, GL_INVALID_ENUM);
    texGenv(ctx, coord, pname, &f);
}

static void cTexGeniv(GLMContext ctx, GLenum coord, GLenum pname, const GLint *params)
{
    GLfloat f[4];
    int n = pname == GL_TEXTURE_GEN_MODE ? 1 : 4;

    for (int i = 0; i < n; i++)
        f[i] = (GLfloat)params[i];

    texGenv(ctx, coord, pname, f);
}

static void cTexGend(GLMContext ctx, GLenum coord, GLenum pname, GLdouble param)
{
    GLfloat f = (GLfloat)param;

    ERROR_CHECK_RETURN(pname == GL_TEXTURE_GEN_MODE, GL_INVALID_ENUM);
    texGenv(ctx, coord, pname, &f);
}

static void cTexGendv(GLMContext ctx, GLenum coord, GLenum pname, const GLdouble *params)
{
    GLfloat f[4];
    int n = pname == GL_TEXTURE_GEN_MODE ? 1 : 4;

    for (int i = 0; i < n; i++)
        f[i] = (GLfloat)params[i];

    texGenv(ctx, coord, pname, f);
}

static int getTexGen(GLMContext ctx, GLenum coord, GLenum pname, GLfloat *out)
{
    MGLTexEnv *e = activeEnv(ctx);
    int c = (int)coord - GL_S;

    ERROR_CHECK_RETURN_VALUE(e, GL_INVALID_OPERATION, 0);
    ERROR_CHECK_RETURN_VALUE(c >= 0 && c < 4, GL_INVALID_ENUM, 0);

    switch (pname)
    {
        case GL_TEXTURE_GEN_MODE: out[0] = (GLfloat)e->gen_mode[c]; return 1;
        case GL_OBJECT_PLANE: memcpy(out, e->obj_plane[c], 4 * sizeof(GLfloat)); return 4;
        case GL_EYE_PLANE: memcpy(out, e->eye_plane[c], 4 * sizeof(GLfloat)); return 4;
    }

    ERROR_RETURN_VALUE(GL_INVALID_ENUM, 0);
}

static void cGetTexGenfv(GLMContext ctx, GLenum coord, GLenum pname, GLfloat *params)
{
    getTexGen(ctx, coord, pname, params);
}

static void cGetTexGeniv(GLMContext ctx, GLenum coord, GLenum pname, GLint *params)
{
    GLfloat f[4];
    int n = getTexGen(ctx, coord, pname, f);

    for (int i = 0; i < n; i++)
        params[i] = (GLint)f[i];
}

static void cGetTexGendv(GLMContext ctx, GLenum coord, GLenum pname, GLdouble *params)
{
    GLfloat f[4];
    int n = getTexGen(ctx, coord, pname, f);

    for (int i = 0; i < n; i++)
        params[i] = f[i];
}

// --- fog -----------------------------------------------------------------------

static void fogv(GLMContext ctx, GLenum pname, const GLfloat *p, bool ints)
{
    C->values_serial++;
    GLenum v = (GLenum)p[0];

    switch (pname)
    {
        case GL_FOG_MODE:
            ERROR_CHECK_RETURN(v == GL_LINEAR || v == GL_EXP || v == GL_EXP2, GL_INVALID_ENUM);
            C->fog_mode = v;
            break;
        case GL_FOG_DENSITY:
            ERROR_CHECK_RETURN(p[0] >= 0.0f, GL_INVALID_VALUE);
            C->fog_density = p[0];
            break;
        case GL_FOG_START: C->fog_start = p[0]; break;
        case GL_FOG_END: C->fog_end = p[0]; break;
        case GL_FOG_INDEX: C->fog_index = p[0]; break;
        case GL_FOG_COLOR:
            for (int i = 0; i < 4; i++)
                C->fog_color[i] = ints ? intColor((GLint)p[i]) : p[i];
            break;
        case GL_FOG_COORD_SRC:
            ERROR_CHECK_RETURN(v == GL_FOG_COORD || v == GL_FRAGMENT_DEPTH, GL_INVALID_ENUM);
            C->fog_coord_src = v;
            break;
        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

static void cFogf(GLMContext ctx, GLenum pname, GLfloat param)
{
    ERROR_CHECK_RETURN(pname != GL_FOG_COLOR, GL_INVALID_ENUM);
    fogv(ctx, pname, &param, false);
}

static void cFogfv(GLMContext ctx, GLenum pname, const GLfloat *params)
{
    fogv(ctx, pname, params, false);
}

static void cFogi(GLMContext ctx, GLenum pname, GLint param)
{
    GLfloat f = (GLfloat)param;

    ERROR_CHECK_RETURN(pname != GL_FOG_COLOR, GL_INVALID_ENUM);
    fogv(ctx, pname, &f, false);
}

static void cFogiv(GLMContext ctx, GLenum pname, const GLint *params)
{
    GLfloat f[4];
    int n = pname == GL_FOG_COLOR ? 4 : 1;

    for (int i = 0; i < n; i++)
        f[i] = (GLfloat)params[i];

    fogv(ctx, pname, f, pname == GL_FOG_COLOR);
}

// --- clip planes, polygons, points and the other leftovers -------------------

static void cClipPlane(GLMContext ctx, GLenum plane, const GLdouble *eq)
{
    C->values_serial++;
    GLfloat inv[16], p[4];
    GLuint i = plane - GL_CLIP_PLANE0;

    NOT_IN_BEGIN();
    ERROR_CHECK_RETURN(i < MGL_COMPAT_PLANES, GL_INVALID_ENUM);

    for (int k = 0; k < 4; k++)
        p[k] = (GLfloat)eq[k];

    mglMatInvert(mglCompatModelview(ctx), inv);

    for (int k = 0; k < 4; k++)
        C->clip_planes[i][k] = p[0] * inv[k * 4] + p[1] * inv[k * 4 + 1] + p[2] * inv[k * 4 + 2] + p[3] * inv[k * 4 + 3];
}

static void cGetClipPlane(GLMContext ctx, GLenum plane, GLdouble *eq)
{
    GLuint i = plane - GL_CLIP_PLANE0;

    ERROR_CHECK_RETURN(i < MGL_COMPAT_PLANES, GL_INVALID_ENUM);

    for (int k = 0; k < 4; k++)
        eq[k] = C->clip_planes[i][k];
}

void mglPolygonMode(GLMContext ctx, GLenum face, GLenum mode);

static void cPolygonMode(GLMContext ctx, GLenum face, GLenum mode)
{
    ERROR_CHECK_RETURN(face == GL_FRONT || face == GL_BACK || face == GL_FRONT_AND_BACK, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(mode == GL_POINT || mode == GL_LINE || mode == GL_FILL, GL_INVALID_ENUM);

    if (face != GL_BACK)
        C->polygon_mode[0] = mode;
    if (face != GL_FRONT)
        C->polygon_mode[1] = mode;

    // the core state holds the mode while both faces agree; the draw splits
    // in two when they do not
    C->next.polygon_mode(ctx, GL_FRONT_AND_BACK, C->polygon_mode[0]);
}

static void cEdgeFlag(GLMContext ctx, GLboolean flag)
{
    C->edge_flag = flag ? GL_TRUE : GL_FALSE;
}

static void cEdgeFlagv(GLMContext ctx, const GLboolean *flag)
{
    C->edge_flag = flag[0] ? GL_TRUE : GL_FALSE;
}

static void cLineStipple(GLMContext ctx, GLint factor, GLushort pattern)
{
    C->stipple_factor = factor < 1 ? 1 : factor > 256 ? 256 : factor;
    C->stipple_pattern = pattern;
}

static void cPolygonStipple(GLMContext ctx, const GLubyte *mask)
{
    memcpy(C->polygon_stipple_mask, mask, sizeof(C->polygon_stipple_mask));
}

static void cGetPolygonStipple(GLMContext ctx, GLubyte *mask)
{
    memcpy(mask, C->polygon_stipple_mask, sizeof(C->polygon_stipple_mask));
}

static void cPixelZoom(GLMContext ctx, GLfloat x, GLfloat y)
{
    C->zoom_x = x;
    C->zoom_y = y;
}

static void pixelTransfer(GLMContext ctx, GLenum pname, GLfloat v)
{
    switch (pname)
    {
        case GL_RED_SCALE: C->pixel_scale[0] = v; break;
        case GL_GREEN_SCALE: C->pixel_scale[1] = v; break;
        case GL_BLUE_SCALE: C->pixel_scale[2] = v; break;
        case GL_ALPHA_SCALE: C->pixel_scale[3] = v; break;
        case GL_RED_BIAS: C->pixel_bias[0] = v; break;
        case GL_GREEN_BIAS: C->pixel_bias[1] = v; break;
        case GL_BLUE_BIAS: C->pixel_bias[2] = v; break;
        case GL_ALPHA_BIAS: C->pixel_bias[3] = v; break;
        case GL_MAP_COLOR: case GL_MAP_STENCIL: case GL_INDEX_SHIFT: case GL_INDEX_OFFSET:
        case GL_DEPTH_SCALE: case GL_DEPTH_BIAS:
            break;
        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

static void cPixelTransferf(GLMContext ctx, GLenum pname, GLfloat param) { pixelTransfer(ctx, pname, param); }
static void cPixelTransferi(GLMContext ctx, GLenum pname, GLint param) { pixelTransfer(ctx, pname, (GLfloat)param); }

static void cClearIndex(GLMContext ctx, GLfloat c) { C->clear_index = c; }
static void cIndexMask(GLMContext ctx, GLuint mask) { (void)ctx; (void)mask; }

static void cClearAccum(GLMContext ctx, GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    C->accum_clear[0] = r;
    C->accum_clear[1] = g;
    C->accum_clear[2] = b;
    C->accum_clear[3] = a;
}

// there is no accumulation buffer: GL_ACCUM_*_BITS answer 0
static void cAccum(GLMContext ctx, GLenum op, GLfloat value)
{
    (void)value;
    ERROR_CHECK_RETURN(op == GL_ACCUM || op == GL_LOAD || op == GL_RETURN || op == GL_MULT || op == GL_ADD,
                       GL_INVALID_ENUM);
    ERROR_RETURN(GL_INVALID_OPERATION);
}

static void cPointParameterfv(GLMContext ctx, GLenum pname, const GLfloat *params)
{
    switch (pname)
    {
        case GL_POINT_SIZE_MIN:
        case GL_POINT_SIZE_MAX:
        case GL_POINT_DISTANCE_ATTENUATION:
            // stored so they read back; points are not attenuated
            return;
    }

    C->next.point_parameterfv(ctx, pname, params);
}

static void cPointParameterf(GLMContext ctx, GLenum pname, GLfloat param)
{
    if (pname == GL_POINT_SIZE_MIN || pname == GL_POINT_SIZE_MAX)
        return;

    C->next.point_parameterf(ctx, pname, param);
}

static void cHint(GLMContext ctx, GLenum target, GLenum mode)
{
    GLenum *slot = NULL;

    switch (target)
    {
        case GL_PERSPECTIVE_CORRECTION_HINT: slot = &C->perspective_hint; break;
        case GL_POINT_SMOOTH_HINT: slot = &C->point_hint; break;
        case GL_FOG_HINT: slot = &C->fog_hint; break;
        case GL_GENERATE_MIPMAP_HINT: slot = &C->polygon_hint; break;
    }

    if (slot == NULL)
    {
        C->next.hint(ctx, target, mode);
        return;
    }

    ERROR_CHECK_RETURN(mode == GL_FASTEST || mode == GL_NICEST || mode == GL_DONT_CARE, GL_INVALID_ENUM);
    *slot = mode;
}

// --- enables -------------------------------------------------------------------

// Returns the switch a legacy cap lives in, or NULL when the cap is core's.
static GLboolean *legacyCap(GLMContext ctx, GLenum cap, GLbitfield *bit, GLbitfield **bits)
{
    MGLTexEnv *e = activeEnv(ctx);

    *bits = NULL;

    if (cap >= GL_LIGHT0 && cap < GL_LIGHT0 + MGL_COMPAT_LIGHTS)
    {
        *bits = &C->lights_on;
        *bit = 1u << (cap - GL_LIGHT0);
        return NULL;
    }

    switch (cap)
    {
        case GL_LIGHTING: return &C->lighting;
        case GL_COLOR_MATERIAL: return &C->color_material;
        case GL_NORMALIZE: return &C->normalize;
        case GL_RESCALE_NORMAL: return &C->rescale_normal;
        case GL_FOG: return &C->fog;
        case GL_COLOR_SUM: return &C->color_sum;
        case GL_POINT_SPRITE: return &C->point_sprite;
        case GL_POINT_SMOOTH: return &C->point_smooth;
        case GL_LINE_STIPPLE: return &C->line_stipple;
        case GL_POLYGON_STIPPLE: return &C->polygon_stipple;
        case GL_AUTO_NORMAL: return &C->auto_normal;
        case GL_INDEX_LOGIC_OP: return &C->index_logic_op;
    }

    if (cap >= GL_MAP1_COLOR_4 && cap <= GL_MAP1_VERTEX_4)
    {
        *bits = &C->map1_on;
        *bit = 1u << (cap - GL_MAP1_COLOR_4);
        return NULL;
    }

    if (cap >= GL_MAP2_COLOR_4 && cap <= GL_MAP2_VERTEX_4)
    {
        *bits = &C->map2_on;
        *bit = 1u << (cap - GL_MAP2_COLOR_4);
        return NULL;
    }

    if (e == NULL)
        return NULL;

    switch (cap)
    {
        case GL_TEXTURE_1D: *bits = &e->enabled; *bit = MGL_TEX_1D; return NULL;
        case GL_TEXTURE_2D: *bits = &e->enabled; *bit = MGL_TEX_2D; return NULL;
        case GL_TEXTURE_3D: *bits = &e->enabled; *bit = MGL_TEX_3D; return NULL;
        case GL_TEXTURE_CUBE_MAP: *bits = &e->enabled; *bit = MGL_TEX_CUBE; return NULL;
        case GL_TEXTURE_RECTANGLE: *bits = &e->enabled; *bit = MGL_TEX_RECT; return NULL;
        case GL_TEXTURE_GEN_S: case GL_TEXTURE_GEN_T: case GL_TEXTURE_GEN_R: case GL_TEXTURE_GEN_Q:
            *bits = &e->gen_enabled;
            *bit = 1u << (cap - GL_TEXTURE_GEN_S);
            return NULL;
    }

    return NULL;
}

static bool isEvaluatorCap(GLenum cap)
{
    return (cap >= GL_MAP1_COLOR_4 && cap <= GL_MAP1_VERTEX_4) ||
           (cap >= GL_MAP2_COLOR_4 && cap <= GL_MAP2_VERTEX_4);
}

static bool setLegacyCap(GLMContext ctx, GLenum cap, GLboolean on)
{
    GLbitfield bit = 0, *bits;
    GLboolean *flag = legacyCap(ctx, cap, &bit, &bits);

    if (flag)
    {
        *flag = on;
        return true;
    }

    if (bits)
    {
        if (on)
            *bits |= bit;
        else
            *bits &= ~bit;
        return true;
    }

    return false;
}

static void cEnable(GLMContext ctx, GLenum cap)
{
    if (setLegacyCap(ctx, cap, GL_TRUE))
        return;

    if (cap == GL_VERTEX_PROGRAM_TWO_SIDE)
    {
        C->two_side = GL_TRUE;
        return;
    }

    C->next.enable(ctx, cap);
}

static void cDisable(GLMContext ctx, GLenum cap)
{
    if (setLegacyCap(ctx, cap, GL_FALSE))
        return;

    if (cap == GL_VERTEX_PROGRAM_TWO_SIDE)
    {
        C->two_side = GL_FALSE;
        return;
    }

    C->next.disable(ctx, cap);
}

static GLboolean cIsEnabled(GLMContext ctx, GLenum cap)
{
    GLbitfield bit = 0, *bits;
    GLboolean *flag = legacyCap(ctx, cap, &bit, &bits);

    if (flag)
        return *flag;

    if (bits)
        return (*bits & bit) != 0;

    if (cap == GL_VERTEX_PROGRAM_TWO_SIDE)
        return C->two_side;

    switch (cap)
    {
        case GL_VERTEX_ARRAY: case GL_NORMAL_ARRAY: case GL_COLOR_ARRAY: case GL_SECONDARY_COLOR_ARRAY:
        case GL_FOG_COORD_ARRAY: case GL_TEXTURE_COORD_ARRAY: case GL_EDGE_FLAG_ARRAY: case GL_INDEX_ARRAY:
            return mglCompatClientStateEnabled(ctx, cap);
    }

    return C->next.is_enabled(ctx, cap);
}

// --- queries -------------------------------------------------------------------

static int depthOf(MGLMatrixStack *s)
{
    return s->depth + 1;
}

static GLint framebufferBits(GLMContext ctx, GLenum pname);

// Fills out with a legacy query's values. Returns how many, or 0 when the
// name is not a legacy one.
static int legacyGet(GLMContext ctx, GLenum pname, GLdouble *out)
{
    MGLTexEnv *e = activeEnv(ctx);
    GLuint unit = STATE(active_texture) < MGL_COMPAT_UNITS ? STATE(active_texture) : 0;
    const GLfloat *m = NULL;
    bool transposed = false;
    GLboolean is_on;
    int n = mglCompatEvalGet(ctx, pname, out);

    if (n)
        return n;

    if (isEvaluatorCap(pname))
    {
        out[0] = cIsEnabled(ctx, pname);
        return 1;
    }

    switch (pname)
    {
        case GL_NUM_EXTENSIONS: out[0] = mglCompatNumExtensions(ctx); return 1;
        case GL_UNPACK_CLIENT_STORAGE_APPLE: out[0] = C->client_storage; return 1;
        case GL_MATRIX_MODE: out[0] = C->matrix_mode; return 1;
        case GL_MODELVIEW_MATRIX: m = mglCompatModelview(ctx); break;
        case GL_PROJECTION_MATRIX: m = mglCompatProjection(ctx); break;
        case GL_TEXTURE_MATRIX: m = mglCompatTextureMatrix(ctx, unit); break;
        case GL_TRANSPOSE_MODELVIEW_MATRIX: m = mglCompatModelview(ctx); transposed = true; break;
        case GL_TRANSPOSE_PROJECTION_MATRIX: m = mglCompatProjection(ctx); transposed = true; break;
        case GL_TRANSPOSE_TEXTURE_MATRIX: m = mglCompatTextureMatrix(ctx, unit); transposed = true; break;
        case GL_MODELVIEW_STACK_DEPTH: out[0] = depthOf(&C->modelview); return 1;
        case GL_PROJECTION_STACK_DEPTH: out[0] = depthOf(&C->projection); return 1;
        case GL_TEXTURE_STACK_DEPTH: out[0] = depthOf(&C->texture[unit]); return 1;
        case GL_MAX_MODELVIEW_STACK_DEPTH: out[0] = MGL_MODELVIEW_DEPTH; return 1;
        case GL_MAX_PROJECTION_STACK_DEPTH: out[0] = MGL_PROJECTION_DEPTH; return 1;
        case GL_MAX_TEXTURE_STACK_DEPTH: out[0] = MGL_TEXTURE_DEPTH; return 1;
        case GL_MAX_NAME_STACK_DEPTH: out[0] = MGL_NAME_DEPTH; return 1;
        case GL_MAX_ATTRIB_STACK_DEPTH: out[0] = MGL_ATTRIB_DEPTH; return 1;
        case GL_MAX_CLIENT_ATTRIB_STACK_DEPTH: out[0] = MGL_ATTRIB_DEPTH; return 1;
        case GL_MAX_LIST_NESTING: out[0] = MGL_LIST_NESTING; return 1;
        case GL_MAX_LIGHTS: out[0] = MGL_COMPAT_LIGHTS; return 1;
        case GL_MAX_TEXTURE_UNITS: out[0] = MGL_COMPAT_UNITS; return 1;
        case GL_MAX_TEXTURE_COORDS: out[0] = MGL_COMPAT_UNITS; return 1;
        case GL_MAX_PIXEL_MAP_TABLE: out[0] = 256; return 1;
        case GL_NAME_STACK_DEPTH: out[0] = C->name_depth; return 1;
        case GL_ATTRIB_STACK_DEPTH: out[0] = C->attrib_depth; return 1;
        case GL_CLIENT_ATTRIB_STACK_DEPTH: out[0] = C->client_depth; return 1;
        case GL_CLIENT_ACTIVE_TEXTURE: out[0] = GL_TEXTURE0 + C->client_active_texture; return 1;
        case GL_RENDER_MODE: out[0] = C->render_mode; return 1;
        case GL_SELECTION_BUFFER_SIZE: out[0] = C->select_size; return 1;
        case GL_FEEDBACK_BUFFER_SIZE: out[0] = C->feedback_size; return 1;
        case GL_FEEDBACK_BUFFER_TYPE: out[0] = C->feedback_type; return 1;
        case GL_LIST_BASE: out[0] = C->list_base; return 1;
        case GL_LIST_INDEX: out[0] = C->compiling ? C->compiling_name : 0; return 1;
        case GL_LIST_MODE: out[0] = C->compiling ? C->list_mode : 0; return 1;
        case GL_SHADE_MODEL: out[0] = C->shade_model; return 1;
        case GL_COLOR_MATERIAL_FACE: out[0] = C->color_material_face; return 1;
        case GL_COLOR_MATERIAL_PARAMETER: out[0] = C->color_material_mode; return 1;
        case GL_LIGHT_MODEL_AMBIENT:
            for (int i = 0; i < 4; i++) out[i] = C->model_ambient[i];
            return 4;
        case GL_LIGHT_MODEL_LOCAL_VIEWER: out[0] = C->local_viewer; return 1;
        case GL_LIGHT_MODEL_TWO_SIDE: out[0] = C->two_side; return 1;
        case GL_LIGHT_MODEL_COLOR_CONTROL: out[0] = C->color_control; return 1;
        case GL_FOG_MODE: out[0] = C->fog_mode; return 1;
        case GL_FOG_DENSITY: out[0] = C->fog_density; return 1;
        case GL_FOG_START: out[0] = C->fog_start; return 1;
        case GL_FOG_END: out[0] = C->fog_end; return 1;
        case GL_FOG_INDEX: out[0] = C->fog_index; return 1;
        case GL_FOG_COORD_SRC: out[0] = C->fog_coord_src; return 1;
        case GL_FOG_COLOR:
            for (int i = 0; i < 4; i++) out[i] = C->fog_color[i];
            return 4;
        case GL_POLYGON_MODE: out[0] = C->polygon_mode[0]; out[1] = C->polygon_mode[1]; return 2;
        case GL_EDGE_FLAG: out[0] = C->edge_flag; return 1;
        case GL_LINE_STIPPLE_PATTERN: out[0] = C->stipple_pattern; return 1;
        case GL_LINE_STIPPLE_REPEAT: out[0] = C->stipple_factor; return 1;
        case GL_ZOOM_X: out[0] = C->zoom_x; return 1;
        case GL_ZOOM_Y: out[0] = C->zoom_y; return 1;
        case GL_RED_SCALE: out[0] = C->pixel_scale[0]; return 1;
        case GL_GREEN_SCALE: out[0] = C->pixel_scale[1]; return 1;
        case GL_BLUE_SCALE: out[0] = C->pixel_scale[2]; return 1;
        case GL_ALPHA_SCALE: out[0] = C->pixel_scale[3]; return 1;
        case GL_RED_BIAS: out[0] = C->pixel_bias[0]; return 1;
        case GL_GREEN_BIAS: out[0] = C->pixel_bias[1]; return 1;
        case GL_BLUE_BIAS: out[0] = C->pixel_bias[2]; return 1;
        case GL_ALPHA_BIAS: out[0] = C->pixel_bias[3]; return 1;
        case GL_PERSPECTIVE_CORRECTION_HINT: out[0] = C->perspective_hint; return 1;
        case GL_POINT_SMOOTH_HINT: out[0] = C->point_hint; return 1;
        case GL_FOG_HINT: out[0] = C->fog_hint; return 1;
        case GL_GENERATE_MIPMAP_HINT: out[0] = C->polygon_hint; return 1;
        case GL_INDEX_CLEAR_VALUE: out[0] = C->clear_index; return 1;
        case GL_ACCUM_CLEAR_VALUE:
            for (int i = 0; i < 4; i++) out[i] = C->accum_clear[i];
            return 4;
        case GL_CURRENT_RASTER_POSITION:
            for (int i = 0; i < 4; i++) out[i] = C->raster_pos[i];
            return 4;
        case GL_CURRENT_RASTER_POSITION_VALID: out[0] = C->raster_valid; return 1;
        case GL_CURRENT_RASTER_COLOR:
            for (int i = 0; i < 4; i++) out[i] = C->raster_color[i];
            return 4;
        case GL_CURRENT_RASTER_SECONDARY_COLOR:
            for (int i = 0; i < 4; i++) out[i] = C->raster_secondary[i];
            return 4;
        case GL_CURRENT_RASTER_TEXTURE_COORDS:
            for (int i = 0; i < 4; i++) out[i] = C->raster_tex[i];
            return 4;
        case GL_CURRENT_RASTER_DISTANCE: out[0] = C->raster_distance; return 1;
        case GL_CURRENT_RASTER_INDEX: out[0] = 1; return 1;
        case GL_CURRENT_INDEX: out[0] = 1; return 1;
        case GL_CURRENT_COLOR:
        case GL_CURRENT_NORMAL:
        case GL_CURRENT_SECONDARY_COLOR:
        case GL_CURRENT_FOG_COORD:
        case GL_CURRENT_TEXTURE_COORDS:
            return mglCompatCurrentValue(ctx, pname, out);
        case GL_POINT_SIZE_MIN: out[0] = 0.0; return 1;
        case GL_POINT_SIZE_MAX: out[0] = 64.0; return 1;
        case GL_POINT_DISTANCE_ATTENUATION: out[0] = 1.0; out[1] = 0.0; out[2] = 0.0; return 3;
        case GL_ALIASED_POINT_SIZE_RANGE:
        case GL_SMOOTH_POINT_SIZE_RANGE:
            out[0] = 1.0; out[1] = 64.0; return 2;
        case GL_RED_BITS: case GL_GREEN_BITS: case GL_BLUE_BITS: case GL_ALPHA_BITS:
        case GL_DEPTH_BITS: case GL_STENCIL_BITS:
            out[0] = framebufferBits(ctx, pname); return 1;
        case GL_ACCUM_RED_BITS: case GL_ACCUM_GREEN_BITS: case GL_ACCUM_BLUE_BITS: case GL_ACCUM_ALPHA_BITS:
        case GL_INDEX_BITS: case GL_AUX_BUFFERS:
            out[0] = 0; return 1;
        case GL_RGBA_MODE: out[0] = 1; return 1;
        case GL_INDEX_MODE: out[0] = 0; return 1;
        case GL_TEXTURE_ENV_MODE:
            out[0] = e ? e->mode : GL_MODULATE; return 1;
        case GL_LIGHTING: case GL_COLOR_MATERIAL: case GL_NORMALIZE: case GL_RESCALE_NORMAL: case GL_FOG:
        case GL_COLOR_SUM: case GL_POINT_SPRITE: case GL_POINT_SMOOTH: case GL_LINE_STIPPLE:
        case GL_POLYGON_STIPPLE: case GL_AUTO_NORMAL: case GL_TEXTURE_1D: case GL_TEXTURE_2D:
        case GL_TEXTURE_3D: case GL_TEXTURE_CUBE_MAP: case GL_TEXTURE_RECTANGLE: case GL_TEXTURE_GEN_S:
        case GL_TEXTURE_GEN_T: case GL_TEXTURE_GEN_R: case GL_TEXTURE_GEN_Q:
        case GL_VERTEX_ARRAY: case GL_NORMAL_ARRAY: case GL_COLOR_ARRAY: case GL_SECONDARY_COLOR_ARRAY:
        case GL_FOG_COORD_ARRAY: case GL_TEXTURE_COORD_ARRAY: case GL_EDGE_FLAG_ARRAY:
            is_on = cIsEnabled(ctx, pname);
            out[0] = is_on;
            return 1;
    }

    if (pname >= GL_LIGHT0 && pname < GL_LIGHT0 + MGL_COMPAT_LIGHTS)
    {
        out[0] = (C->lights_on >> (pname - GL_LIGHT0)) & 1;
        return 1;
    }

    if (m)
    {
        for (int i = 0; i < 16; i++)
            out[i] = transposed ? m[(i % 4) * 4 + i / 4] : m[i];
        return 16;
    }

    return mglCompatArrayQuery(ctx, pname, out);
}

void mglGetFramebufferAttachmentParameteriv(GLMContext ctx, GLenum target, GLenum attachment, GLenum pname, GLint *params);

static GLint framebufferBits(GLMContext ctx, GLenum pname)
{
    GLint v = 0;
    GLenum attachment, what;
    GLenum err = STATE(error);

    switch (pname)
    {
        case GL_RED_BITS: what = GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE; break;
        case GL_GREEN_BITS: what = GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE; break;
        case GL_BLUE_BITS: what = GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE; break;
        case GL_ALPHA_BITS: what = GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE; break;
        case GL_DEPTH_BITS: what = GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE; break;
        default: what = GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE; break;
    }

    if (STATE(framebuffer))
        attachment = pname == GL_DEPTH_BITS ? GL_DEPTH_ATTACHMENT :
                     pname == GL_STENCIL_BITS ? GL_STENCIL_ATTACHMENT : GL_COLOR_ATTACHMENT0;
    else
        attachment = pname == GL_DEPTH_BITS ? GL_DEPTH : pname == GL_STENCIL_BITS ? GL_STENCIL : GL_BACK_LEFT;

    ctx->error_suppress++;
    mglGetFramebufferAttachmentParameteriv(ctx, GL_DRAW_FRAMEBUFFER, attachment, what, &v);
    ctx->error_suppress--;
    STATE(error) = err;

    return v;
}

static void cGetDoublev(GLMContext ctx, GLenum pname, GLdouble *data)
{
    GLdouble v[16];
    int n = legacyGet(ctx, pname, v);

    if (n == 0)
    {
        C->next.get_doublev(ctx, pname, data);
        return;
    }

    for (int i = 0; i < n; i++)
        data[i] = v[i];
}

static void cGetFloatv(GLMContext ctx, GLenum pname, GLfloat *data)
{
    GLdouble v[16];
    int n = legacyGet(ctx, pname, v);

    if (n == 0)
    {
        C->next.get_floatv(ctx, pname, data);
        return;
    }

    for (int i = 0; i < n; i++)
        data[i] = (GLfloat)v[i];
}

// colours read as integers map [-1, 1] onto the whole range
static bool isColorQuery(GLenum pname)
{
    switch (pname)
    {
        case GL_CURRENT_COLOR: case GL_CURRENT_SECONDARY_COLOR: case GL_CURRENT_RASTER_COLOR:
        case GL_CURRENT_RASTER_SECONDARY_COLOR: case GL_FOG_COLOR: case GL_LIGHT_MODEL_AMBIENT:
        case GL_ACCUM_CLEAR_VALUE:
            return true;
    }

    return false;
}

static void cGetIntegerv(GLMContext ctx, GLenum pname, GLint *data)
{
    GLdouble v[16];
    int n = legacyGet(ctx, pname, v);

    if (n == 0)
    {
        C->next.get_integerv(ctx, pname, data);
        return;
    }

    for (int i = 0; i < n; i++)
    {
        if (isColorQuery(pname))
            data[i] = (GLint)fmax(fmin(v[i] * 2147483647.0, 2147483647.0), -2147483648.0);
        else
            data[i] = (GLint)lround(v[i]);
    }
}

static void cGetBooleanv(GLMContext ctx, GLenum pname, GLboolean *data)
{
    GLdouble v[16];
    int n = legacyGet(ctx, pname, v);

    if (n == 0)
    {
        C->next.get_booleanv(ctx, pname, data);
        return;
    }

    for (int i = 0; i < n; i++)
        data[i] = v[i] != 0.0;
}

static void cGetInteger64v(GLMContext ctx, GLenum pname, GLint64 *data)
{
    GLdouble v[16];
    int n = legacyGet(ctx, pname, v);

    if (n == 0)
    {
        C->next.get_integer64v(ctx, pname, data);
        return;
    }

    for (int i = 0; i < n; i++)
        data[i] = (GLint64)llround(v[i]);
}

// --- strings -------------------------------------------------------------------

// what a compatibility context has on top of the core list
static const char *compat_extensions[] = {
    "GL_ARB_multitexture",
    "GL_ARB_transpose_matrix",
    "GL_ARB_texture_env_add",
    "GL_ARB_texture_env_combine",
    "GL_ARB_texture_env_crossbar",
    "GL_ARB_texture_env_dot3",
    "GL_EXT_texture_env_add",
    "GL_EXT_texture_env_combine",
    "GL_ARB_point_sprite",
    "GL_ARB_window_pos",
    "GL_EXT_secondary_color",
    "GL_EXT_fog_coord",
    "GL_EXT_bgra",
    "GL_EXT_texture_edge_clamp",
    "GL_ARB_texture_border_clamp",
    "GL_ARB_texture_cube_map",
    "GL_ARB_vertex_buffer_object",
    "GL_ARB_texture_non_power_of_two",
    "GL_EXT_rescale_normal",
    "GL_EXT_separate_specular_color",
    "GL_EXT_texture_rectangle",
    "GL_APPLE_client_storage",
    "GL_APPLE_texture_range",
    "GL_APPLE_ycbcr_422",
    "GL_APPLE_packed_pixels",
    "GL_EXT_framebuffer_object",
    "GL_EXT_framebuffer_blit",
    "GL_EXT_framebuffer_multisample",
    "GL_EXT_packed_depth_stencil",
    "GL_ARB_shader_objects",
    "GL_ARB_vertex_shader",
    "GL_ARB_fragment_shader",
    "GL_ARB_shading_language_100",
    "GL_EXT_blend_minmax",
    "GL_EXT_blend_subtract",
    "GL_EXT_blend_equation_separate",
    "GL_EXT_blend_func_separate",
    "GL_SGIS_texture_edge_clamp",
};

#define COMPAT_EXTENSIONS (GLuint)(sizeof(compat_extensions) / sizeof(compat_extensions[0]))

GLuint mglNumExtensions(void);
const GLubyte *mglGetStringi(GLMContext ctx, GLenum name, GLuint index);

static const GLubyte *cGetString(GLMContext ctx, GLenum name)
{
    if (name == GL_VERSION)
        return (const GLubyte *)"4.6.0 Compatibility Profile MooGL " MOOGL_VERSION;

    if (name != GL_EXTENSIONS)
        return C->next.get_string(ctx, name);

    if (C->extension_string == NULL)
    {
        size_t len = 1;
        GLuint core = mglNumExtensions();

        for (GLuint i = 0; i < core; i++)
            len += strlen((const char *)mglGetStringi(ctx, GL_EXTENSIONS, i)) + 1;

        for (GLuint i = 0; i < COMPAT_EXTENSIONS; i++)
            len += strlen(compat_extensions[i]) + 1;

        C->extension_string = (char *)calloc(1, len);
        if (C->extension_string == NULL)
            return (const GLubyte *)"";

        for (GLuint i = 0; i < COMPAT_EXTENSIONS; i++)
        {
            strcat(C->extension_string, compat_extensions[i]);
            strcat(C->extension_string, " ");
        }

        for (GLuint i = 0; i < core; i++)
        {
            strcat(C->extension_string, (const char *)mglGetStringi(ctx, GL_EXTENSIONS, i));
            strcat(C->extension_string, " ");
        }
    }

    return (const GLubyte *)C->extension_string;
}

static const GLubyte *cGetStringi(GLMContext ctx, GLenum name, GLuint index)
{
    GLuint core = mglNumExtensions();

    if (name == GL_EXTENSIONS && index >= core)
    {
        ERROR_CHECK_RETURN_VALUE(index - core < COMPAT_EXTENSIONS, GL_INVALID_VALUE, NULL);
        return (const GLubyte *)compat_extensions[index - core];
    }

    return C->next.get_stringi(ctx, name, index);
}

GLint mglCompatNumExtensions(GLMContext ctx)
{
    return (GLint)(mglNumExtensions() + COMPAT_EXTENSIONS);
}

// --- set up --------------------------------------------------------------------

static void initStack(MGLMatrixStack *s, int max)
{
    s->depth = 0;
    s->max = max;
    mglMatIdentity(s->s[0].m);
}

static void setDefaults(GLMContext ctx)
{
    C->values_serial++;
    C->matrix_mode = GL_MODELVIEW;
    initStack(&C->modelview, MGL_MODELVIEW_DEPTH);
    initStack(&C->projection, MGL_PROJECTION_DEPTH);

    for (int i = 0; i < MGL_COMPAT_UNITS; i++)
    {
        initStack(&C->texture[i], MGL_TEXTURE_DEPTH);
        defaultTexEnv(&C->tex[i]);
    }

    for (int i = 0; i < MGL_COMPAT_LIGHTS; i++)
        defaultLight(&C->lights[i], i);

    defaultMaterial(&C->material[0]);
    defaultMaterial(&C->material[1]);

    C->model_ambient[0] = C->model_ambient[1] = C->model_ambient[2] = 0.2f;
    C->model_ambient[3] = 1.0f;
    C->color_control = GL_SINGLE_COLOR;
    C->color_material_face = GL_FRONT_AND_BACK;
    C->color_material_mode = GL_AMBIENT_AND_DIFFUSE;
    C->shade_model = GL_SMOOTH;

    C->fog_mode = GL_EXP;
    C->fog_density = 1.0f;
    C->fog_end = 1.0f;
    C->fog_coord_src = GL_FRAGMENT_DEPTH;

    C->polygon_mode[0] = C->polygon_mode[1] = GL_FILL;
    C->edge_flag = GL_TRUE;
    C->stipple_factor = 1;
    C->stipple_pattern = 0xFFFF;
    memset(C->polygon_stipple_mask, 0xFF, sizeof(C->polygon_stipple_mask));

    C->perspective_hint = C->point_hint = C->line_hint = C->polygon_hint = C->fog_hint = GL_DONT_CARE;

    C->raster_valid = GL_TRUE;
    C->raster_pos[3] = 1.0f;
    C->raster_color[0] = C->raster_color[1] = C->raster_color[2] = C->raster_color[3] = 1.0f;
    C->raster_secondary[3] = 1.0f;
    C->raster_tex[3] = 1.0f;
    C->zoom_x = C->zoom_y = 1.0f;

    for (int i = 0; i < 4; i++)
        C->pixel_scale[i] = 1.0f;

    C->render_mode = GL_RENDER;
    C->sprite_origin = GL_UPPER_LEFT;
}

#define SET(field, fn) ctx->dispatch.field = fn

static void installState(GLMContext ctx)
{
    SET(matrix_mode, cMatrixMode);
    SET(load_identity, cLoadIdentity);
    SET(load_matrixf, cLoadMatrixf);
    SET(load_matrixd, cLoadMatrixd);
    SET(mult_matrixf, cMultMatrixf);
    SET(mult_matrixd, cMultMatrixd);
    SET(load_transpose_matrixf, cLoadTransposeMatrixf);
    SET(load_transpose_matrixd, cLoadTransposeMatrixd);
    SET(mult_transpose_matrixf, cMultTransposeMatrixf);
    SET(mult_transpose_matrixd, cMultTransposeMatrixd);
    SET(push_matrix, cPushMatrix);
    SET(pop_matrix, cPopMatrix);
    SET(translatef, cTranslatef);
    SET(translated, cTranslated);
    SET(scalef, cScalef);
    SET(scaled, cScaled);
    SET(rotatef, cRotatef);
    SET(rotated, cRotated);
    SET(ortho, cOrtho);
    SET(frustum, cFrustum);

    SET(lightf, cLightf);
    SET(lightfv, cLightfv);
    SET(lighti, cLighti);
    SET(lightiv, cLightiv);
    SET(get_lightfv, cGetLightfv);
    SET(get_lightiv, cGetLightiv);
    SET(materialf, cMaterialf);
    SET(materialfv, cMaterialfv);
    SET(materiali, cMateriali);
    SET(materialiv, cMaterialiv);
    SET(get_materialfv, cGetMaterialfv);
    SET(get_materialiv, cGetMaterialiv);
    SET(light_modelf, cLightModelf);
    SET(light_modelfv, cLightModelfv);
    SET(light_modeli, cLightModeli);
    SET(light_modeliv, cLightModeliv);
    SET(color_material, cColorMaterial);
    SET(shade_model, cShadeModel);

    SET(tex_envf, cTexEnvf);
    SET(tex_envfv, cTexEnvfv);
    SET(tex_envi, cTexEnvi);
    SET(tex_enviv, cTexEnviv);
    SET(get_tex_envfv, cGetTexEnvfv);
    SET(get_tex_enviv, cGetTexEnviv);
    SET(tex_genf, cTexGenf);
    SET(tex_genfv, cTexGenfv);
    SET(tex_geni, cTexGeni);
    SET(tex_geniv, cTexGeniv);
    SET(tex_gend, cTexGend);
    SET(tex_gendv, cTexGendv);
    SET(get_tex_genfv, cGetTexGenfv);
    SET(get_tex_geniv, cGetTexGeniv);
    SET(get_tex_gendv, cGetTexGendv);

    SET(fogf, cFogf);
    SET(fogfv, cFogfv);
    SET(fogi, cFogi);
    SET(fogiv, cFogiv);

    SET(clip_plane, cClipPlane);
    SET(get_clip_plane, cGetClipPlane);
    SET(polygon_mode, cPolygonMode);
    SET(edge_flag, cEdgeFlag);
    SET(edge_flagv, cEdgeFlagv);
    SET(line_stipple, cLineStipple);
    SET(polygon_stipple, cPolygonStipple);
    SET(get_polygon_stipple, cGetPolygonStipple);
    SET(pixel_zoom, cPixelZoom);
    SET(pixel_transferf, cPixelTransferf);
    SET(pixel_transferi, cPixelTransferi);
    SET(clear_index, cClearIndex);
    SET(index_mask, cIndexMask);
    SET(clear_accum, cClearAccum);
    SET(accum, cAccum);
    SET(point_parameterfv, cPointParameterfv);
    SET(point_parameterf, cPointParameterf);
    SET(hint, cHint);

    SET(enable, cEnable);
    SET(disable, cDisable);
    SET(is_enabled, cIsEnabled);
    SET(get_doublev, cGetDoublev);
    SET(get_floatv, cGetFloatv);
    SET(get_integerv, cGetIntegerv);
    SET(get_booleanv, cGetBooleanv);
    SET(get_integer64v, cGetInteger64v);
    SET(get_string, cGetString);
    SET(get_stringi, cGetStringi);
}

void mglCompatInit(GLMContext ctx)
{
    ctx->compat = (MGLCompat *)calloc(1, sizeof(MGLCompat));

    if (ctx->compat == NULL)
        return;

    C->next = ctx->dispatch;

    setDefaults(ctx);
    installState(ctx);
    mglCompatInstallArrays(ctx);
    mglCompatInstallDraw(ctx);
    mglCompatInstallImmediate(ctx);
    mglCompatInstallAttrib(ctx);
    mglCompatInstallLists(ctx);
    mglCompatInstallRaster(ctx);
    mglCompatInstallSelect(ctx);
    mglCompatInstallApple(ctx);
    mglCompatInstallEval(ctx);
}

void mglCompatFree(GLMContext ctx)
{
    if (ctx->compat == NULL)
        return;

    mglCompatFreeArrays(ctx);
    mglCompatFreeFF(ctx);
    mglCompatFreeAttrib(ctx);
    mglCompatFreeRaster(ctx);
    mglCompatFreeEval(ctx);

    free(C->imm);
    free(C->extension_string);
    free(ctx->compat);
    ctx->compat = NULL;
}

#endif /* MGL_COMPAT_CONTEXTS */
