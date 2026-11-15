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
 * compat_ff.c
 * MGL
 *
 * The fixed-function pipeline as generated GLSL. The state that changes the
 * shader (which lights, which texture targets and combine modes, fog, two-
 * sided lighting) is a key, and each key gets one program; the state that
 * does not (matrices, colours, positions) arrives as uniforms before each
 * draw. The same uniforms are what an application's own old-style shader
 * reads as gl_ModelViewMatrix, gl_LightSource and the rest.
 */

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
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
void mglGetProgramInfoLog(GLMContext ctx, GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
GLint mglGetUniformLocation(GLMContext ctx, GLuint program, const GLchar *name);
void mglProgramUniform1fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLfloat *value);
void mglProgramUniform3fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLfloat *value);
void mglProgramUniform4fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLfloat *value);
void mglProgramUniform1i(GLMContext ctx, GLuint program, GLint location, GLint v0);
void mglProgramUniformMatrix3fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void mglProgramUniformMatrix4fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
int mglLegacyFormatClass(GLenum internalformat);

// --- the key ---------------------------------------------------------------------

enum { T_NONE, T_1D, T_2D, T_3D, T_CUBE, T_RECT };

typedef struct {
    uint8_t target, mode, format;
    uint8_t combine_rgb, combine_alpha;
    uint8_t src_rgb[3], src_alpha[3], op_rgb[3], op_alpha[3];
    uint8_t coord_replace;
    uint8_t gen;            // S T R Q enabled
    uint8_t gen_mode[4];
} UnitKey;

typedef struct {
    uint8_t lighting, two_side, local_viewer, separate_specular;
    uint8_t lights, positional, spot;
    uint8_t color_material, cm_mode, cm_face;
    uint8_t normalize, flat, fog, fog_mode, fog_coord, color_sum;
    uint8_t points, sprite;
    uint8_t clip;
    UnitKey unit[MGL_COMPAT_UNITS];
} FFKey;

struct MGLFFProgram_t {
    FFKey key;
    GLuint name;
    Program *prog;
    GLint loc_matrix, loc_normal, loc_vec, loc_frag;
    // what was last written, so unchanged state is not written again
    GLfloat matrix[(2 + MGL_COMPAT_UNITS) * 16], normal[9], vec[132 * 4], frag[18 * 4];
    bool written;
    MGLFFProgram *next;
};

// small codes for enums so the key stays compact
static uint8_t code(GLenum e)
{
    switch (e)
    {
        case GL_MODULATE: return 1; case GL_DECAL: return 2; case GL_BLEND: return 3; case GL_REPLACE: return 4;
        case GL_ADD: return 5; case GL_COMBINE: return 6; case GL_ADD_SIGNED: return 7; case GL_INTERPOLATE: return 8;
        case GL_SUBTRACT: return 9; case GL_DOT3_RGB: return 10; case GL_DOT3_RGBA: return 11;
        case GL_TEXTURE: return 12; case GL_CONSTANT: return 13; case GL_PRIMARY_COLOR: return 14;
        case GL_PREVIOUS: return 15; case GL_SRC_COLOR: return 16; case GL_ONE_MINUS_SRC_COLOR: return 17;
        case GL_SRC_ALPHA: return 18; case GL_ONE_MINUS_SRC_ALPHA: return 19;
        case GL_OBJECT_LINEAR: return 20; case GL_EYE_LINEAR: return 21; case GL_SPHERE_MAP: return 22;
        case GL_REFLECTION_MAP: return 23; case GL_NORMAL_MAP: return 24;
        case GL_EMISSION: return 25; case GL_AMBIENT: return 26; case GL_DIFFUSE: return 27;
        case GL_SPECULAR: return 28; case GL_AMBIENT_AND_DIFFUSE: return 29;
        case GL_FRONT: return 30; case GL_BACK: return 31; case GL_FRONT_AND_BACK: return 32;
        case GL_LINEAR: return 33; case GL_EXP: return 34; case GL_EXP2: return 35;
    }

    if (e >= GL_TEXTURE0 && e < GL_TEXTURE0 + MGL_COMPAT_UNITS)
        return (uint8_t)(100 + (e - GL_TEXTURE0));

    return 0;
}

static Texture *unitTexture(GLMContext ctx, GLuint u, int target)
{
    int index;

    switch (target)
    {
        case T_1D: index = _TEXTURE_1D; break;
        case T_2D: index = _TEXTURE_2D; break;
        case T_3D: index = _TEXTURE_3D; break;
        case T_CUBE: index = _TEXTURE_CUBE_MAP; break;
        case T_RECT: index = _TEXTURE_RECTANGLE; break;
        default: return NULL;
    }

    return STATE(texture_units[u].textures[index]);
}

static void buildKey(GLMContext ctx, GLenum mode, FFKey *k)
{
    memset(k, 0, sizeof(*k));

    k->lighting = C->lighting;

    if (C->lighting)
    {
        k->two_side = C->two_side;
        k->local_viewer = C->local_viewer;
        k->separate_specular = C->color_control == GL_SEPARATE_SPECULAR_COLOR;
        k->lights = (uint8_t)C->lights_on;

        for (int i = 0; i < MGL_COMPAT_LIGHTS; i++)
        {
            if (!((C->lights_on >> i) & 1))
                continue;

            if (C->lights[i].position[3] != 0.0f)
                k->positional |= (uint8_t)(1 << i);

            if (C->lights[i].spot_cutoff != 180.0f)
                k->spot |= (uint8_t)(1 << i);
        }

        k->color_material = C->color_material;
        if (C->color_material)
        {
            k->cm_mode = code(C->color_material_mode);
            k->cm_face = code(C->color_material_face);
        }

        k->normalize = C->normalize || C->rescale_normal;
    }

    k->flat = C->shade_model == GL_FLAT;
    k->fog = C->fog;
    if (C->fog)
    {
        k->fog_mode = code(C->fog_mode);
        k->fog_coord = C->fog_coord_src == GL_FOG_COORD;
    }

    k->color_sum = C->color_sum;
    k->points = mode == GL_POINTS;
    k->sprite = k->points && C->point_sprite;

    for (int i = 0; i < MGL_COMPAT_PLANES; i++)
        if (STATE(caps.clip_distances[i]))
            k->clip |= (uint8_t)(1 << i);

    for (GLuint u = 0; u < MGL_COMPAT_UNITS; u++)
    {
        MGLTexEnv *e = &C->tex[u];
        UnitKey *uk = &k->unit[u];
        GLbitfield on = e->enabled;
        Texture *tex;

        uk->target = (on & MGL_TEX_CUBE) ? T_CUBE : (on & MGL_TEX_3D) ? T_3D : (on & MGL_TEX_RECT) ? T_RECT :
                     (on & MGL_TEX_2D) ? T_2D : (on & MGL_TEX_1D) ? T_1D : T_NONE;

        // a sprite's coordinate needs no texture to be meaningful, but without
        // one nothing reads it
        if (uk->target == T_NONE)
            continue;

        tex = unitTexture(ctx, u, uk->target);
        uk->format = (uint8_t)mglLegacyFormatClass(tex ? tex->internalformat : GL_RGBA);
        uk->mode = code(e->mode);

        if (e->mode == GL_COMBINE)
        {
            uk->combine_rgb = code(e->combine_rgb);
            uk->combine_alpha = code(e->combine_alpha);

            for (int s = 0; s < 3; s++)
            {
                uk->src_rgb[s] = code(e->src_rgb[s]);
                uk->src_alpha[s] = code(e->src_alpha[s]);
                uk->op_rgb[s] = code(e->op_rgb[s]);
                uk->op_alpha[s] = code(e->op_alpha[s]);
            }
        }

        uk->coord_replace = k->sprite && e->coord_replace;
        uk->gen = (uint8_t)e->gen_enabled;

        for (int c = 0; c < 4; c++)
            if ((e->gen_enabled >> c) & 1)
                uk->gen_mode[c] = code(e->gen_mode[c]);
    }
}

// --- the GLSL --------------------------------------------------------------------

typedef struct {
    char *s;
    size_t len, cap;
} Str;

static void add(Str *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void add(Str *b, const char *fmt, ...)
{
    va_list ap;
    int n;

    for (;;)
    {
        va_start(ap, fmt);
        n = vsnprintf(b->s ? b->s + b->len : NULL, b->s ? b->cap - b->len : 0, fmt, ap);
        va_end(ap);

        if (n < 0)
            return;

        if (b->s && b->len + (size_t)n < b->cap)
        {
            b->len += (size_t)n;
            return;
        }

        size_t cap = (b->cap + (size_t)n + 1) * 2;
        char *s = (char *)realloc(b->s, cap);

        if (s == NULL)
            return;

        b->s = s;
        b->cap = cap;
    }
}

// the old built-in uniforms, which generated and application shaders share
const char *mglCompatBuiltinDecls =
    "struct mgl_LightSourceParameters {\n"
    "    vec4 ambient; vec4 diffuse; vec4 specular; vec4 position; vec4 halfVector;\n"
    "    vec3 spotDirection; float spotExponent; float spotCutoff; float spotCosCutoff;\n"
    "    float constantAttenuation; float linearAttenuation; float quadraticAttenuation;\n"
    "};\n"
    "struct mgl_MaterialParameters { vec4 emission; vec4 ambient; vec4 diffuse; vec4 specular; float shininess; };\n"
    "struct mgl_LightModelParameters { vec4 ambient; };\n"
    "struct mgl_LightModelProducts { vec4 sceneColor; };\n"
    "struct mgl_LightProducts { vec4 ambient; vec4 diffuse; vec4 specular; };\n"
    "struct mgl_FogParameters { vec4 color; float density; float start; float end; float scale; };\n"
    "struct mgl_PointParameters { float size; float sizeMin; float sizeMax; float fadeThresholdSize;\n"
    "    float distanceConstantAttenuation; float distanceLinearAttenuation; float distanceQuadraticAttenuation; };\n";

static const char *samplerType(int target)
{
    switch (target)
    {
        case T_1D: return "sampler1D";
        case T_3D: return "sampler3D";
        case T_CUBE: return "samplerCube";
        case T_RECT: return "sampler2DRect";
        default: return "sampler2D";
    }
}

// The generated programs read their state from four packed uniforms, which
// keeps them clear of Metal's buffer slots however much state is in use:
//   mgl_FFMatrix[10]  modelview, modelview-projection, texture 0-7
//   mgl_FFNormal      the normal matrix
//   mgl_FFVec[132]    vertex-stage vectors, laid out as the FV_ values below
//   mgl_FFFrag[18]    fragment-stage vectors, as the FG_ values below
enum {
    FV_POINT = 0,           // size, min, max
    FV_MODEL_AMBIENT = 1,
    FV_MATERIAL = 2,        // per face: emission, ambient, diffuse, specular, (shininess)
    FV_LIGHT = 12,          // per light: ambient, diffuse, specular, position,
                            // (spot direction, exponent), (cos cutoff, k0, k1, k2)
    FV_CLIP = 60,
    FV_TEXGEN = 68,         // per unit: object S T R Q, eye S T R Q
    FV_COUNT = 132,
};

enum {
    FG_ENV = 0,             // per unit: env colour
    FG_SCALE = 8,           // per unit: (lod bias, rgb scale, alpha scale)
    FG_FOG_COLOR = 16,
    FG_FOG = 17,            // density, start, end, scale
    FG_COUNT = 18,
};

#define FF_MATRICES (2 + MGL_COMPAT_UNITS)

// colour material replaces these material terms by the vertex colour
static void matTerm(const FFKey *k, int face, int term, char *out, size_t cap)
{
    static const GLenum terms[4] = { GL_EMISSION, GL_AMBIENT, GL_DIFFUSE, GL_SPECULAR };
    bool faceOn = k->color_material && (k->cm_face == code(GL_FRONT_AND_BACK) ||
                                        k->cm_face == code(face ? GL_BACK : GL_FRONT));
    bool tracks = false;

    if (faceOn)
    {
        GLenum t = terms[term];

        tracks = k->cm_mode == code(t) ||
                 (k->cm_mode == code(GL_AMBIENT_AND_DIFFUSE) && (t == GL_AMBIENT || t == GL_DIFFUSE));
    }

    if (tracks)
        snprintf(out, cap, "mgl_Color");
    else
        snprintf(out, cap, "mgl_FFVec[%d]", FV_MATERIAL + face * 5 + term);
}

static void lightFunction(Str *b, const FFKey *k, int face)
{
    char t[4][32];

    for (int i = 0; i < 4; i++)
        matTerm(k, face, i, t[i], sizeof(t[i]));

    add(b, "void mgl_light%s(vec3 n, vec3 eye, out vec4 color, out vec4 spec) {\n", face ? "Back" : "Front");
    add(b, "    vec4 emi = %s, amb = %s, dif = %s, spc = %s;\n", t[0], t[1], t[2], t[3]);
    add(b, "    float shin = mgl_FFVec[%d].x;\n", FV_MATERIAL + face * 5 + 4);
    add(b, "    vec3 c = emi.rgb + amb.rgb * mgl_FFVec[%d].rgb;\n", FV_MODEL_AMBIENT);
    add(b, "    vec3 s = vec3(0.0);\n");

    for (int i = 0; i < MGL_COMPAT_LIGHTS; i++)
    {
        int L = FV_LIGHT + i * 6;

        if (!((k->lights >> i) & 1))
            continue;

        add(b, "    {\n");

        if ((k->positional >> i) & 1)
        {
            add(b, "        vec3 d = mgl_FFVec[%d].xyz / mgl_FFVec[%d].w - eye;\n", L + 3, L + 3);
            add(b, "        float dist = length(d);\n");
            add(b, "        vec3 VP = d / max(dist, 1e-20);\n");
            add(b, "        vec4 k = mgl_FFVec[%d];\n", L + 5);
            add(b, "        float att = 1.0 / max(k.y + k.z * dist + k.w * dist * dist, 1e-20);\n");
        }
        else
        {
            add(b, "        vec3 VP = normalize(mgl_FFVec[%d].xyz);\n", L + 3);
            add(b, "        float att = 1.0;\n");
        }

        if ((k->spot >> i) & 1)
        {
            add(b, "        float sd = dot(-VP, normalize(mgl_FFVec[%d].xyz));\n", L + 4);
            add(b, "        att *= sd >= mgl_FFVec[%d].x ? pow(max(sd, 0.0), mgl_FFVec[%d].w) : 0.0;\n", L + 5, L + 4);
        }

        add(b, "        float nl = max(dot(n, VP), 0.0);\n");
        add(b, "        vec3 h = normalize(VP + %s);\n", k->local_viewer ? "normalize(-eye)" : "vec3(0.0, 0.0, 1.0)");
        add(b, "        float nh = max(dot(n, h), 0.0);\n");
        add(b, "        float sf = nl > 0.0 ? (shin == 0.0 ? 1.0 : pow(nh, shin)) : 0.0;\n");
        add(b, "        c += att * (amb.rgb * mgl_FFVec[%d].rgb + nl * dif.rgb * mgl_FFVec[%d].rgb);\n", L, L + 1);
        add(b, "        s += att * sf * spc.rgb * mgl_FFVec[%d].rgb;\n", L + 2);
        add(b, "    }\n");
    }

    if (k->separate_specular)
    {
        add(b, "    color = vec4(clamp(c, 0.0, 1.0), dif.a);\n");
        add(b, "    spec = vec4(clamp(s, 0.0, 1.0), 1.0);\n");
    }
    else
    {
        add(b, "    color = vec4(clamp(c + s, 0.0, 1.0), dif.a);\n");
        add(b, "    spec = vec4(0.0, 0.0, 0.0, 1.0);\n");
    }

    add(b, "}\n");
}

static void texGen(Str *b, const UnitKey *uk, int u)
{
    static const char *comp = "xyzw";

    for (int c = 0; c < 4; c++)
    {
        if (!((uk->gen >> c) & 1))
            continue;

        uint8_t m = uk->gen_mode[c];

        if (m == code(GL_OBJECT_LINEAR))
            add(b, "    tc.%c = dot(mgl_Vertex, mgl_FFVec[%d]);\n", comp[c], FV_TEXGEN + u * 8 + c);
        else if (m == code(GL_EYE_LINEAR))
            add(b, "    tc.%c = dot(eye4, mgl_FFVec[%d]);\n", comp[c], FV_TEXGEN + u * 8 + 4 + c);
        else if (m == code(GL_SPHERE_MAP))
            add(b, "    tc.%c = sphere.%c;\n", comp[c], comp[c]);
        else if (m == code(GL_REFLECTION_MAP))
            add(b, "    tc.%c = refl.%c;\n", comp[c], comp[c]);
        else if (m == code(GL_NORMAL_MAP))
            add(b, "    tc.%c = n.%c;\n", comp[c], comp[c]);
    }
}

static char *vertexShader(const FFKey *k)
{
    Str b = { 0 };
    bool needs_normal = k->lighting;

    for (int u = 0; u < MGL_COMPAT_UNITS; u++)
        if (k->unit[u].gen)
            needs_normal = true;

    add(&b, "#version 410 core\n");
    add(&b, "layout(location = 0) in vec4 mgl_Vertex;\n");
    add(&b, "layout(location = 2) in vec3 mgl_Normal;\n");
    add(&b, "layout(location = 3) in vec4 mgl_Color;\n");
    add(&b, "layout(location = 4) in vec4 mgl_SecondaryColor;\n");
    add(&b, "layout(location = 5) in float mgl_FogCoord;\n");

    for (int u = 0; u < MGL_COMPAT_UNITS; u++)
        if (k->unit[u].target)
            add(&b, "layout(location = %d) in vec4 mgl_MultiTexCoord%d;\n", MGL_ATTR_TEXCOORD0 + u, u);

    add(&b, "uniform mat4 mgl_FFMatrix[%d];\n", FF_MATRICES);
    add(&b, "uniform mat3 mgl_FFNormal;\n");
    add(&b, "uniform vec4 mgl_FFVec[%d];\n", FV_COUNT);

    const char *q = k->flat ? "flat " : "";

    add(&b, "%sout vec4 v_color;\n%sout vec4 v_secondary;\n", q, q);
    if (k->two_side)
        add(&b, "%sout vec4 v_bcolor;\n%sout vec4 v_bsecondary;\n", q, q);
    if (k->fog)
        add(&b, "out float v_fog;\n");

    for (int u = 0; u < MGL_COMPAT_UNITS; u++)
        if (k->unit[u].target)
            add(&b, "out vec4 v_tc%d;\n", u);

    if (k->clip)
    {
        int n = 0;
        for (int i = 0; i < MGL_COMPAT_PLANES; i++)
            if ((k->clip >> i) & 1)
                n = i + 1;
        add(&b, "out float gl_ClipDistance[%d];\n", n);
    }

    if (k->lighting)
    {
        lightFunction(&b, k, 0);
        if (k->two_side)
            lightFunction(&b, k, 1);
    }

    add(&b, "void main() {\n");
    add(&b, "    vec4 eye4 = mgl_FFMatrix[0] * mgl_Vertex;\n");
    add(&b, "    vec3 eye = eye4.xyz / eye4.w;\n");
    add(&b, "    gl_Position = mgl_FFMatrix[1] * mgl_Vertex;\n");

    if (needs_normal)
    {
        add(&b, "    vec3 n = mgl_FFNormal * mgl_Normal;\n");
        if (k->normalize || !k->lighting)
            add(&b, "    n = normalize(n);\n");
    }

    if (k->lighting)
    {
        add(&b, "    mgl_lightFront(n, eye, v_color, v_secondary);\n");
        if (k->two_side)
            add(&b, "    mgl_lightBack(-n, eye, v_bcolor, v_bsecondary);\n");
    }
    else
    {
        add(&b, "    v_color = mgl_Color;\n");
        add(&b, "    v_secondary = mgl_SecondaryColor;\n");
        if (k->two_side)
            add(&b, "    v_bcolor = mgl_Color;\n    v_bsecondary = mgl_SecondaryColor;\n");
    }

    if (k->fog)
        add(&b, "    v_fog = %s;\n", k->fog_coord ? "abs(mgl_FogCoord)" : "abs(eye4.z)");

    for (int u = 0; u < MGL_COMPAT_UNITS; u++)
    {
        const UnitKey *uk = &k->unit[u];

        if (!uk->target)
            continue;

        add(&b, "    {\n");
        add(&b, "    vec4 tc = mgl_MultiTexCoord%d;\n", u);

        if (uk->gen)
        {
            add(&b, "    vec3 u = normalize(eye);\n");
            add(&b, "    vec3 refl = reflect(u, n);\n");
            add(&b, "    float m = 2.0 * sqrt(refl.x * refl.x + refl.y * refl.y + (refl.z + 1.0) * (refl.z + 1.0));\n");
            add(&b, "    vec2 sphere = refl.xy / max(m, 1e-20) + 0.5;\n");
            texGen(&b, uk, u);
        }

        add(&b, "    v_tc%d = mgl_FFMatrix[%d] * tc;\n", u, 2 + u);
        add(&b, "    }\n");
    }

    for (int i = 0; i < MGL_COMPAT_PLANES; i++)
        if ((k->clip >> i) & 1)
            add(&b, "    gl_ClipDistance[%d] = dot(eye4, mgl_FFVec[%d]);\n", i, FV_CLIP + i);

    if (k->points)
        add(&b, "    gl_PointSize = clamp(mgl_FFVec[%d].x, max(mgl_FFVec[%d].y, 1.0), mgl_FFVec[%d].z);\n",
            FV_POINT, FV_POINT, FV_POINT);

    add(&b, "}\n");

    return b.s;
}

// what a combine source and operand read, as GLSL
static void combineArg(int u, uint8_t src, uint8_t op, bool alpha, char *out, size_t cap)
{
    char s[32];

    if (src == code(GL_TEXTURE))
        snprintf(s, sizeof(s), "t%d", u);
    else if (src >= 100)
        snprintf(s, sizeof(s), "t%d", src - 100);
    else if (src == code(GL_CONSTANT))
        snprintf(s, sizeof(s), "mgl_FFFrag[%d]", FG_ENV + u);
    else if (src == code(GL_PRIMARY_COLOR))
        snprintf(s, sizeof(s), "primary");
    else
        snprintf(s, sizeof(s), "prev");

    if (alpha)
        snprintf(out, cap, op == code(GL_ONE_MINUS_SRC_ALPHA) ? "(1.0 - %s.a)" : "%s.a", s);
    else if (op == code(GL_SRC_ALPHA))
        snprintf(out, cap, "vec3(%s.a)", s);
    else if (op == code(GL_ONE_MINUS_SRC_ALPHA))
        snprintf(out, cap, "vec3(1.0 - %s.a)", s);
    else if (op == code(GL_ONE_MINUS_SRC_COLOR))
        snprintf(out, cap, "(1.0 - %s.rgb)", s);
    else
        snprintf(out, cap, "%s.rgb", s);
}

static void combineExpr(Str *b, uint8_t fn, const char *a0, const char *a1, const char *a2, bool alpha)
{
    if (fn == code(GL_REPLACE)) add(b, "%s", a0);
    else if (fn == code(GL_MODULATE)) add(b, "(%s * %s)", a0, a1);
    else if (fn == code(GL_ADD)) add(b, "(%s + %s)", a0, a1);
    else if (fn == code(GL_ADD_SIGNED)) add(b, "(%s + %s - 0.5)", a0, a1);
    else if (fn == code(GL_INTERPOLATE)) add(b, "(%s * %s + %s * (1.0 - %s))", a0, a2, a1, a2);
    else if (fn == code(GL_SUBTRACT)) add(b, "(%s - %s)", a0, a1);
    else if (alpha) add(b, "%s", a0);
    else add(b, "vec3(4.0 * dot(%s - 0.5, %s - 0.5))", a0, a1);
}

enum { F_RGBA, F_RGB, F_ALPHA, F_LUMINANCE, F_LUMINANCE_ALPHA, F_INTENSITY };

static void envMode(Str *b, const UnitKey *uk, int u)
{
    bool has_color = uk->format != F_ALPHA;
    bool has_alpha = uk->format == F_RGBA || uk->format == F_ALPHA || uk->format == F_LUMINANCE_ALPHA ||
                     uk->format == F_INTENSITY;
    bool intensity = uk->format == F_INTENSITY;
    char t[8], env[24];

    snprintf(t, sizeof(t), "t%d", u);
    snprintf(env, sizeof(env), "mgl_FFFrag[%d]", FG_ENV + u);

    if (uk->mode == code(GL_REPLACE))
    {
        if (has_color) add(b, "    prev.rgb = %s.rgb;\n", t);
        if (has_alpha) add(b, "    prev.a = %s.a;\n", t);
    }
    else if (uk->mode == code(GL_MODULATE))
    {
        if (has_color) add(b, "    prev.rgb *= %s.rgb;\n", t);
        if (has_alpha) add(b, "    prev.a *= %s.a;\n", t);
    }
    else if (uk->mode == code(GL_DECAL))
    {
        if (uk->format == F_RGB) add(b, "    prev.rgb = %s.rgb;\n", t);
        else if (uk->format == F_RGBA) add(b, "    prev.rgb = mix(prev.rgb, %s.rgb, %s.a);\n", t, t);
    }
    else if (uk->mode == code(GL_BLEND))
    {
        if (has_color) add(b, "    prev.rgb = mix(prev.rgb, %s.rgb, %s.rgb);\n", env, t);
        if (intensity) add(b, "    prev.a = mix(prev.a, %s.a, %s.a);\n", env, t);
        else if (has_alpha) add(b, "    prev.a *= %s.a;\n", t);
    }
    else if (uk->mode == code(GL_ADD))
    {
        if (has_color) add(b, "    prev.rgb += %s.rgb;\n", t);
        if (intensity) add(b, "    prev.a += %s.a;\n", t);
        else if (has_alpha) add(b, "    prev.a *= %s.a;\n", t);
    }
    else if (uk->mode == code(GL_COMBINE))
    {
        char a[3][48], c[3][48];

        for (int s = 0; s < 3; s++)
        {
            combineArg(u, uk->src_rgb[s], uk->op_rgb[s], false, c[s], sizeof(c[s]));
            combineArg(u, uk->src_alpha[s], uk->op_alpha[s], true, a[s], sizeof(a[s]));
        }

        add(b, "    {\n    vec3 rgb = ");
        combineExpr(b, uk->combine_rgb, c[0], c[1], c[2], false);
        add(b, ";\n    float al = ");

        if (uk->combine_rgb == code(GL_DOT3_RGBA))
            add(b, "rgb.r");
        else
            combineExpr(b, uk->combine_alpha, a[0], a[1], a[2], true);

        add(b, ";\n    prev = clamp(vec4(rgb * mgl_FFFrag[%d].y, al * mgl_FFFrag[%d].z), 0.0, 1.0);\n    }\n",
            FG_SCALE + u, FG_SCALE + u);
        return;
    }

    add(b, "    prev = clamp(prev, 0.0, 1.0);\n");
}

static char *fragmentShader(const FFKey *k)
{
    Str b = { 0 };
    const char *q = k->flat ? "flat " : "";

    add(&b, "#version 410 core\n");
    add(&b, "%sin vec4 v_color;\n%sin vec4 v_secondary;\n", q, q);
    if (k->two_side)
        add(&b, "%sin vec4 v_bcolor;\n%sin vec4 v_bsecondary;\n", q, q);
    if (k->fog)
        add(&b, "in float v_fog;\n");

    for (int u = 0; u < MGL_COMPAT_UNITS; u++)
    {
        if (!k->unit[u].target)
            continue;
        add(&b, "in vec4 v_tc%d;\n", u);
        add(&b, "uniform %s mgl_Texture%d;\n", samplerType(k->unit[u].target), u);
    }

    add(&b, "uniform vec4 mgl_FFFrag[%d];\n", FG_COUNT);
    add(&b, "layout(location = 0) out vec4 mgl_FragColor;\n");
    add(&b, "void main() {\n");

    if (k->two_side)
    {
        add(&b, "    vec4 primary = gl_FrontFacing ? v_color : v_bcolor;\n");
        add(&b, "    vec4 secondary = gl_FrontFacing ? v_secondary : v_bsecondary;\n");
    }
    else
    {
        add(&b, "    vec4 primary = v_color;\n    vec4 secondary = v_secondary;\n");
    }

    add(&b, "    vec4 prev = primary;\n");

    // every unit is sampled first, since a combine may read any of them
    for (int u = 0; u < MGL_COMPAT_UNITS; u++)
    {
        const UnitKey *uk = &k->unit[u];

        if (!uk->target)
            continue;

        if (uk->coord_replace)
            add(&b, "    vec4 c%d = vec4(gl_PointCoord, 0.0, 1.0);\n", u);
        else
            add(&b, "    vec4 c%d = v_tc%d;\n", u, u);

        switch (uk->target)
        {
            case T_CUBE:
                add(&b, "    vec4 t%d = texture(mgl_Texture%d, c%d.xyz, mgl_FFFrag[%d].x);\n", u, u, u, FG_SCALE + u);
                break;
            case T_RECT:
                add(&b, "    vec4 t%d = textureProj(mgl_Texture%d, c%d);\n", u, u, u);
                break;
            default:
                add(&b, "    vec4 t%d = textureProj(mgl_Texture%d, c%d, mgl_FFFrag[%d].x);\n", u, u, u, FG_SCALE + u);
                break;
        }
    }

    for (int u = 0; u < MGL_COMPAT_UNITS; u++)
        if (k->unit[u].target)
            envMode(&b, &k->unit[u], u);

    if (k->separate_specular || (k->color_sum && !k->lighting))
        add(&b, "    prev.rgb = clamp(prev.rgb + secondary.rgb, 0.0, 1.0);\n");

    if (k->fog)
    {
        add(&b, "    vec4 fog = mgl_FFFrag[%d];\n", FG_FOG);

        if (k->fog_mode == code(GL_LINEAR))
            add(&b, "    float f = (fog.z - v_fog) * fog.w;\n");
        else if (k->fog_mode == code(GL_EXP))
            add(&b, "    float f = exp(-fog.x * v_fog);\n");
        else
            add(&b, "    float f = exp(-(fog.x * v_fog) * (fog.x * v_fog));\n");

        add(&b, "    prev.rgb = mix(mgl_FFFrag[%d].rgb, prev.rgb, clamp(f, 0.0, 1.0));\n", FG_FOG_COLOR);
    }

    add(&b, "    mgl_FragColor = prev;\n}\n");

    return b.s;
}

// --- building and finding the programs -----------------------------------------

static GLuint compile(GLMContext ctx, GLenum type, const char *src)
{
    GLuint s = mglCreateShader(ctx, type);

    mglShaderSource(ctx, s, 1, &src, NULL);
    mglCompileShader(ctx, s);
    return s;
}

static MGLFFProgram *buildProgram(GLMContext ctx, const FFKey *k)
{
    char *vs = vertexShader(k), *fs = fragmentShader(k);
    MGLFFProgram *ff = NULL;
    GLuint v, f, p;
    Program *prog;
    GLenum err = STATE(error);

    if (vs == NULL || fs == NULL)
        goto out;

    ctx->error_suppress++;

    v = compile(ctx, GL_VERTEX_SHADER, vs);
    f = compile(ctx, GL_FRAGMENT_SHADER, fs);
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
        char log[2048] = "";

        mglGetProgramInfoLog(ctx, p, sizeof(log), NULL, log);
        MGL_ERR("MGL Error: the fixed-function program did not build: %s\n%s\n%s\n", log, vs, fs);
        mglDeleteProgram(ctx, p);
        STATE(error) = err;
        goto out;
    }

    ff = (MGLFFProgram *)calloc(1, sizeof(MGLFFProgram));
    if (ff == NULL)
    {
        mglDeleteProgram(ctx, p);
        goto out;
    }

    ff->key = *k;
    ff->name = p;
    ff->prog = prog;
    ff->loc_matrix = mglGetUniformLocation(ctx, p, "mgl_FFMatrix");
    ff->loc_normal = mglGetUniformLocation(ctx, p, "mgl_FFNormal");
    ff->loc_vec = mglGetUniformLocation(ctx, p, "mgl_FFVec");
    ff->loc_frag = mglGetUniformLocation(ctx, p, "mgl_FFFrag");

    for (int u = 0; u < MGL_COMPAT_UNITS; u++)
    {
        char name[32];

        if (!k->unit[u].target)
            continue;

        snprintf(name, sizeof(name), "mgl_Texture%d", u);
        GLint loc = mglGetUniformLocation(ctx, p, name);
        if (loc >= 0)
            mglProgramUniform1i(ctx, p, loc, u);
    }

    STATE(error) = err;

out:
    free(vs);
    free(fs);
    return ff;
}

Program *mglCompatFFProgram(GLMContext ctx, GLenum mode)
{
    FFKey k;
    MGLFFProgram *ff;

    buildKey(ctx, mode, &k);

    for (ff = C->ff_programs; ff; ff = ff->next)
        if (!memcmp(&ff->key, &k, sizeof(k)))
            return ff->prog;

    ff = buildProgram(ctx, &k);
    if (ff == NULL)
        return NULL;

    ff->next = C->ff_programs;
    C->ff_programs = ff;
    return ff->prog;
}

void mglCompatFreeFF(GLMContext ctx)
{
    MGLFFProgram *ff = C->ff_programs, *next;

    for (; ff; ff = next)
    {
        next = ff->next;
        mglDeleteProgram(ctx, ff->name);
        free(ff);
    }

    C->ff_programs = NULL;
}

// --- the built-in uniforms -------------------------------------------------------

enum { K_MAT4, K_MAT3, K_VEC4, K_VEC3, K_FLOAT };

typedef struct {
    const char *name;     // with %d for the index, when count > 0
    int count;
    int kind;
    int what;
} Builtin;

enum {
    W_MV, W_P, W_MVP, W_MV_INV, W_P_INV, W_MVP_INV, W_MV_T, W_P_T, W_MVP_T, W_MV_INV_T, W_P_INV_T, W_MVP_INV_T,
    W_NORMAL, W_NORMAL_SCALE, W_TEX, W_TEX_INV, W_TEX_T, W_TEX_INV_T, W_CLIP,
    W_L_AMB, W_L_DIF, W_L_SPC, W_L_POS, W_L_HALF, W_L_SDIR, W_L_SEXP, W_L_SCUT, W_L_SCOS, W_L_K0, W_L_K1, W_L_K2,
    W_MODEL_AMB, W_FM_EMI, W_FM_AMB, W_FM_DIF, W_FM_SPC, W_FM_SHIN, W_BM_EMI, W_BM_AMB, W_BM_DIF, W_BM_SPC, W_BM_SHIN,
    W_F_SCENE, W_B_SCENE, W_FP_AMB, W_FP_DIF, W_FP_SPC, W_BP_AMB, W_BP_DIF, W_BP_SPC,
    W_ENV_COLOR, W_LOD_BIAS, W_TEX_SCALE, W_EYE_S, W_EYE_T, W_EYE_R, W_EYE_Q, W_OBJ_S, W_OBJ_T, W_OBJ_R, W_OBJ_Q,
    W_FOG_COLOR, W_FOG_DENSITY, W_FOG_START, W_FOG_END, W_FOG_SCALE,
    W_PT_SIZE, W_PT_MIN, W_PT_MAX, W_PT_FADE, W_PT_A, W_PT_B, W_PT_C,
};

static const Builtin builtins[] = {
    { "mgl_ModelViewMatrix", 0, K_MAT4, W_MV },
    { "mgl_ProjectionMatrix", 0, K_MAT4, W_P },
    { "mgl_ModelViewProjectionMatrix", 0, K_MAT4, W_MVP },
    { "mgl_ModelViewMatrixInverse", 0, K_MAT4, W_MV_INV },
    { "mgl_ProjectionMatrixInverse", 0, K_MAT4, W_P_INV },
    { "mgl_ModelViewProjectionMatrixInverse", 0, K_MAT4, W_MVP_INV },
    { "mgl_ModelViewMatrixTranspose", 0, K_MAT4, W_MV_T },
    { "mgl_ProjectionMatrixTranspose", 0, K_MAT4, W_P_T },
    { "mgl_ModelViewProjectionMatrixTranspose", 0, K_MAT4, W_MVP_T },
    { "mgl_ModelViewMatrixInverseTranspose", 0, K_MAT4, W_MV_INV_T },
    { "mgl_ProjectionMatrixInverseTranspose", 0, K_MAT4, W_P_INV_T },
    { "mgl_ModelViewProjectionMatrixInverseTranspose", 0, K_MAT4, W_MVP_INV_T },
    { "mgl_NormalMatrix", 0, K_MAT3, W_NORMAL },
    { "mgl_NormalScale", 0, K_FLOAT, W_NORMAL_SCALE },
    { "mgl_TextureMatrix[%d]", MGL_COMPAT_UNITS, K_MAT4, W_TEX },
    { "mgl_TextureMatrixInverse[%d]", MGL_COMPAT_UNITS, K_MAT4, W_TEX_INV },
    { "mgl_TextureMatrixTranspose[%d]", MGL_COMPAT_UNITS, K_MAT4, W_TEX_T },
    { "mgl_TextureMatrixInverseTranspose[%d]", MGL_COMPAT_UNITS, K_MAT4, W_TEX_INV_T },
    { "mgl_ClipPlane[%d]", MGL_COMPAT_PLANES, K_VEC4, W_CLIP },
    { "mgl_LightSource[%d].ambient", MGL_COMPAT_LIGHTS, K_VEC4, W_L_AMB },
    { "mgl_LightSource[%d].diffuse", MGL_COMPAT_LIGHTS, K_VEC4, W_L_DIF },
    { "mgl_LightSource[%d].specular", MGL_COMPAT_LIGHTS, K_VEC4, W_L_SPC },
    { "mgl_LightSource[%d].position", MGL_COMPAT_LIGHTS, K_VEC4, W_L_POS },
    { "mgl_LightSource[%d].halfVector", MGL_COMPAT_LIGHTS, K_VEC4, W_L_HALF },
    { "mgl_LightSource[%d].spotDirection", MGL_COMPAT_LIGHTS, K_VEC3, W_L_SDIR },
    { "mgl_LightSource[%d].spotExponent", MGL_COMPAT_LIGHTS, K_FLOAT, W_L_SEXP },
    { "mgl_LightSource[%d].spotCutoff", MGL_COMPAT_LIGHTS, K_FLOAT, W_L_SCUT },
    { "mgl_LightSource[%d].spotCosCutoff", MGL_COMPAT_LIGHTS, K_FLOAT, W_L_SCOS },
    { "mgl_LightSource[%d].constantAttenuation", MGL_COMPAT_LIGHTS, K_FLOAT, W_L_K0 },
    { "mgl_LightSource[%d].linearAttenuation", MGL_COMPAT_LIGHTS, K_FLOAT, W_L_K1 },
    { "mgl_LightSource[%d].quadraticAttenuation", MGL_COMPAT_LIGHTS, K_FLOAT, W_L_K2 },
    { "mgl_LightModel.ambient", 0, K_VEC4, W_MODEL_AMB },
    { "mgl_FrontMaterial.emission", 0, K_VEC4, W_FM_EMI },
    { "mgl_FrontMaterial.ambient", 0, K_VEC4, W_FM_AMB },
    { "mgl_FrontMaterial.diffuse", 0, K_VEC4, W_FM_DIF },
    { "mgl_FrontMaterial.specular", 0, K_VEC4, W_FM_SPC },
    { "mgl_FrontMaterial.shininess", 0, K_FLOAT, W_FM_SHIN },
    { "mgl_BackMaterial.emission", 0, K_VEC4, W_BM_EMI },
    { "mgl_BackMaterial.ambient", 0, K_VEC4, W_BM_AMB },
    { "mgl_BackMaterial.diffuse", 0, K_VEC4, W_BM_DIF },
    { "mgl_BackMaterial.specular", 0, K_VEC4, W_BM_SPC },
    { "mgl_BackMaterial.shininess", 0, K_FLOAT, W_BM_SHIN },
    { "mgl_FrontLightModelProduct.sceneColor", 0, K_VEC4, W_F_SCENE },
    { "mgl_BackLightModelProduct.sceneColor", 0, K_VEC4, W_B_SCENE },
    { "mgl_FrontLightProduct[%d].ambient", MGL_COMPAT_LIGHTS, K_VEC4, W_FP_AMB },
    { "mgl_FrontLightProduct[%d].diffuse", MGL_COMPAT_LIGHTS, K_VEC4, W_FP_DIF },
    { "mgl_FrontLightProduct[%d].specular", MGL_COMPAT_LIGHTS, K_VEC4, W_FP_SPC },
    { "mgl_BackLightProduct[%d].ambient", MGL_COMPAT_LIGHTS, K_VEC4, W_BP_AMB },
    { "mgl_BackLightProduct[%d].diffuse", MGL_COMPAT_LIGHTS, K_VEC4, W_BP_DIF },
    { "mgl_BackLightProduct[%d].specular", MGL_COMPAT_LIGHTS, K_VEC4, W_BP_SPC },
    { "mgl_TextureEnvColor[%d]", MGL_COMPAT_UNITS, K_VEC4, W_ENV_COLOR },
    { "mgl_TexLodBias[%d]", MGL_COMPAT_UNITS, K_FLOAT, W_LOD_BIAS },
    { "mgl_TexScale[%d]", MGL_COMPAT_UNITS, K_VEC4, W_TEX_SCALE },
    { "mgl_EyePlaneS[%d]", MGL_COMPAT_UNITS, K_VEC4, W_EYE_S },
    { "mgl_EyePlaneT[%d]", MGL_COMPAT_UNITS, K_VEC4, W_EYE_T },
    { "mgl_EyePlaneR[%d]", MGL_COMPAT_UNITS, K_VEC4, W_EYE_R },
    { "mgl_EyePlaneQ[%d]", MGL_COMPAT_UNITS, K_VEC4, W_EYE_Q },
    { "mgl_ObjectPlaneS[%d]", MGL_COMPAT_UNITS, K_VEC4, W_OBJ_S },
    { "mgl_ObjectPlaneT[%d]", MGL_COMPAT_UNITS, K_VEC4, W_OBJ_T },
    { "mgl_ObjectPlaneR[%d]", MGL_COMPAT_UNITS, K_VEC4, W_OBJ_R },
    { "mgl_ObjectPlaneQ[%d]", MGL_COMPAT_UNITS, K_VEC4, W_OBJ_Q },
    { "mgl_Fog.color", 0, K_VEC4, W_FOG_COLOR },
    { "mgl_Fog.density", 0, K_FLOAT, W_FOG_DENSITY },
    { "mgl_Fog.start", 0, K_FLOAT, W_FOG_START },
    { "mgl_Fog.end", 0, K_FLOAT, W_FOG_END },
    { "mgl_Fog.scale", 0, K_FLOAT, W_FOG_SCALE },
    { "mgl_Point.size", 0, K_FLOAT, W_PT_SIZE },
    { "mgl_Point.sizeMin", 0, K_FLOAT, W_PT_MIN },
    { "mgl_Point.sizeMax", 0, K_FLOAT, W_PT_MAX },
    { "mgl_Point.fadeThresholdSize", 0, K_FLOAT, W_PT_FADE },
    { "mgl_Point.distanceConstantAttenuation", 0, K_FLOAT, W_PT_A },
    { "mgl_Point.distanceLinearAttenuation", 0, K_FLOAT, W_PT_B },
    { "mgl_Point.distanceQuadraticAttenuation", 0, K_FLOAT, W_PT_C },
};

#define NUM_BUILTINS (int)(sizeof(builtins) / sizeof(builtins[0]))

typedef struct {
    GLint loc;
    short builtin, index;
    GLfloat last[16];
    bool written;
} CacheEntry;

struct MGLBuiltinCache_t {
    CacheEntry *e;
    int n;
};

static void transpose4(const GLfloat *m, GLfloat *t)
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            t[c * 4 + r] = m[r * 4 + c];
}

static void lightProduct(const GLfloat *a, const GLfloat *b, GLfloat *out)
{
    for (int i = 0; i < 4; i++)
        out[i] = a[i] * b[i];
}

static int value(GLMContext ctx, int what, int i, GLfloat *out)
{
    GLfloat t[16];
    const GLfloat *mv = mglCompatModelview(ctx), *p = mglCompatProjection(ctx);
    MGLLight *L = &C->lights[i < MGL_COMPAT_LIGHTS ? i : 0];
    MGLTexEnv *e = &C->tex[i < MGL_COMPAT_UNITS ? i : 0];
    GLfloat ps = STATE_VAR(point_size);

    switch (what)
    {
        case W_MV: memcpy(out, mv, 64); return 16;
        case W_P: memcpy(out, p, 64); return 16;
        case W_MVP: mglMatMul(out, p, mv); return 16;
        case W_MV_INV: mglMatInvert(mv, out); return 16;
        case W_P_INV: mglMatInvert(p, out); return 16;
        case W_MVP_INV: mglMatMul(t, p, mv); mglMatInvert(t, out); return 16;
        case W_MV_T: transpose4(mv, out); return 16;
        case W_P_T: transpose4(p, out); return 16;
        case W_MVP_T: mglMatMul(t, p, mv); transpose4(t, out); return 16;
        case W_MV_INV_T: mglMatInvert(mv, t); transpose4(t, out); return 16;
        case W_P_INV_T: mglMatInvert(p, t); transpose4(t, out); return 16;
        case W_MVP_INV_T: { GLfloat u[16]; mglMatMul(u, p, mv); mglMatInvert(u, t); transpose4(t, out); return 16; }
        case W_NORMAL:
        {
            // the transpose of the inverse of the upper 3x3
            mglMatInvert(mv, t);
            for (int c = 0; c < 3; c++)
                for (int r = 0; r < 3; r++)
                    out[c * 3 + r] = t[r * 4 + c];
            return 9;
        }
        case W_NORMAL_SCALE:
        {
            GLfloat len = sqrtf(mv[2] * mv[2] + mv[6] * mv[6] + mv[10] * mv[10]);
            out[0] = len != 0.0f ? 1.0f / len : 1.0f;
            return 1;
        }
        case W_TEX: memcpy(out, mglCompatTextureMatrix(ctx, i), 64); return 16;
        case W_TEX_INV: mglMatInvert(mglCompatTextureMatrix(ctx, i), out); return 16;
        case W_TEX_T: transpose4(mglCompatTextureMatrix(ctx, i), out); return 16;
        case W_TEX_INV_T: mglMatInvert(mglCompatTextureMatrix(ctx, i), t); transpose4(t, out); return 16;
        case W_CLIP: memcpy(out, C->clip_planes[i], 16); return 4;
        case W_L_AMB: memcpy(out, L->ambient, 16); return 4;
        case W_L_DIF: memcpy(out, L->diffuse, 16); return 4;
        case W_L_SPC: memcpy(out, L->specular, 16); return 4;
        case W_L_POS: memcpy(out, L->position, 16); return 4;
        case W_L_HALF:
        {
            GLfloat h[3] = { L->position[0], L->position[1], L->position[2] + 1.0f };
            GLfloat len = sqrtf(h[0] * h[0] + h[1] * h[1] + h[2] * h[2]);
            for (int k = 0; k < 3; k++)
                out[k] = len != 0.0f ? h[k] / len : 0.0f;
            out[3] = 1.0f;
            return 4;
        }
        case W_L_SDIR: memcpy(out, L->spot_direction, 12); return 3;
        case W_L_SEXP: out[0] = L->spot_exponent; return 1;
        case W_L_SCUT: out[0] = L->spot_cutoff; return 1;
        case W_L_SCOS: out[0] = cosf(L->spot_cutoff * (GLfloat)M_PI / 180.0f); return 1;
        case W_L_K0: out[0] = L->attenuation[0]; return 1;
        case W_L_K1: out[0] = L->attenuation[1]; return 1;
        case W_L_K2: out[0] = L->attenuation[2]; return 1;
        case W_MODEL_AMB: memcpy(out, C->model_ambient, 16); return 4;
        case W_FM_EMI: memcpy(out, C->material[0].emission, 16); return 4;
        case W_FM_AMB: memcpy(out, C->material[0].ambient, 16); return 4;
        case W_FM_DIF: memcpy(out, C->material[0].diffuse, 16); return 4;
        case W_FM_SPC: memcpy(out, C->material[0].specular, 16); return 4;
        case W_FM_SHIN: out[0] = C->material[0].shininess; return 1;
        case W_BM_EMI: memcpy(out, C->material[1].emission, 16); return 4;
        case W_BM_AMB: memcpy(out, C->material[1].ambient, 16); return 4;
        case W_BM_DIF: memcpy(out, C->material[1].diffuse, 16); return 4;
        case W_BM_SPC: memcpy(out, C->material[1].specular, 16); return 4;
        case W_BM_SHIN: out[0] = C->material[1].shininess; return 1;
        case W_F_SCENE:
        case W_B_SCENE:
        {
            MGLMaterial *m = &C->material[what == W_B_SCENE];
            for (int k = 0; k < 4; k++)
                out[k] = m->emission[k] + m->ambient[k] * C->model_ambient[k];
            out[3] = m->diffuse[3];
            return 4;
        }
        case W_FP_AMB: lightProduct(C->material[0].ambient, L->ambient, out); return 4;
        case W_FP_DIF: lightProduct(C->material[0].diffuse, L->diffuse, out); return 4;
        case W_FP_SPC: lightProduct(C->material[0].specular, L->specular, out); return 4;
        case W_BP_AMB: lightProduct(C->material[1].ambient, L->ambient, out); return 4;
        case W_BP_DIF: lightProduct(C->material[1].diffuse, L->diffuse, out); return 4;
        case W_BP_SPC: lightProduct(C->material[1].specular, L->specular, out); return 4;
        case W_ENV_COLOR: memcpy(out, e->color, 16); return 4;
        case W_LOD_BIAS: out[0] = e->lod_bias; return 1;
        case W_TEX_SCALE: out[0] = e->rgb_scale; out[1] = e->alpha_scale; out[2] = out[3] = 1.0f; return 4;
        case W_EYE_S: case W_EYE_T: case W_EYE_R: case W_EYE_Q:
            memcpy(out, e->eye_plane[what - W_EYE_S], 16); return 4;
        case W_OBJ_S: case W_OBJ_T: case W_OBJ_R: case W_OBJ_Q:
            memcpy(out, e->obj_plane[what - W_OBJ_S], 16); return 4;
        case W_FOG_COLOR: memcpy(out, C->fog_color, 16); return 4;
        case W_FOG_DENSITY: out[0] = C->fog_density; return 1;
        case W_FOG_START: out[0] = C->fog_start; return 1;
        case W_FOG_END: out[0] = C->fog_end; return 1;
        case W_FOG_SCALE: out[0] = C->fog_end != C->fog_start ? 1.0f / (C->fog_end - C->fog_start) : 0.0f; return 1;
        case W_PT_SIZE: out[0] = ps > 0.0f ? ps : 1.0f; return 1;
        case W_PT_MIN: out[0] = 0.0f; return 1;
        case W_PT_MAX: out[0] = 64.0f; return 1;
        case W_PT_FADE: out[0] = 1.0f; return 1;
        case W_PT_A: out[0] = 1.0f; return 1;
        case W_PT_B: out[0] = 0.0f; return 1;
        case W_PT_C: out[0] = 0.0f; return 1;
    }

    return 0;
}

static struct MGLBuiltinCache_t *buildCache(GLMContext ctx, Program *prog)
{
    struct MGLBuiltinCache_t *cache = (struct MGLBuiltinCache_t *)calloc(1, sizeof(*cache));
    GLenum err = STATE(error);
    int cap = 0;

    if (cache == NULL)
        return NULL;

    ctx->error_suppress++;

    for (int b = 0; b < NUM_BUILTINS; b++)
    {
        int count = builtins[b].count ? builtins[b].count : 1;

        // a whole family is skipped when its first element is not there
        for (int i = 0; i < count; i++)
        {
            char name[96];
            GLint loc;

            if (builtins[b].count)
                snprintf(name, sizeof(name), builtins[b].name, i);
            else
                snprintf(name, sizeof(name), "%s", builtins[b].name);

            loc = mglGetUniformLocation(ctx, prog->name, name);

            if (loc < 0)
                continue;

            if (cache->n == cap)
            {
                cap = cap ? cap * 2 : 16;
                CacheEntry *grown = (CacheEntry *)realloc(cache->e, (size_t)cap * sizeof(CacheEntry));
                if (grown == NULL)
                    break;
                cache->e = grown;
            }

            memset(&cache->e[cache->n], 0, sizeof(CacheEntry));
            cache->e[cache->n].loc = loc;
            cache->e[cache->n].builtin = (short)b;
            cache->e[cache->n].index = (short)i;
            cache->n++;
        }
    }
    ctx->error_suppress--;
    STATE(error) = err;

    return cache;
}

void mglCompatForgetBuiltins(Program *prog)
{
    struct MGLBuiltinCache_t *cache = (struct MGLBuiltinCache_t *)prog->compat_cache;

    if (cache)
    {
        free(cache->e);
        free(cache);
    }

    prog->compat_cache = NULL;
}

static void ffValues(GLMContext ctx, GLfloat *mat, GLfloat *normal, GLfloat *vec, GLfloat *frag)
{
    const GLfloat *mv = mglCompatModelview(ctx);
    GLfloat inv[16], ps = STATE_VAR(point_size);

    memcpy(mat, mv, 64);
    mglMatMul(mat + 16, mglCompatProjection(ctx), mv);
    for (int u = 0; u < MGL_COMPAT_UNITS; u++)
        memcpy(mat + (2 + u) * 16, mglCompatTextureMatrix(ctx, (GLuint)u), 64);

    mglMatInvert(mv, inv);
    for (int c = 0; c < 3; c++)
        for (int r = 0; r < 3; r++)
            normal[c * 3 + r] = inv[r * 4 + c];

    memset(vec, 0, FV_COUNT * 4 * sizeof(GLfloat));
    vec[FV_POINT * 4 + 0] = ps > 0.0f ? ps : 1.0f;
    vec[FV_POINT * 4 + 1] = 0.0f;
    vec[FV_POINT * 4 + 2] = 64.0f;
    memcpy(vec + FV_MODEL_AMBIENT * 4, C->model_ambient, 16);

    for (int f = 0; f < 2; f++)
    {
        GLfloat *m = vec + (FV_MATERIAL + f * 5) * 4;

        memcpy(m, C->material[f].emission, 16);
        memcpy(m + 4, C->material[f].ambient, 16);
        memcpy(m + 8, C->material[f].diffuse, 16);
        memcpy(m + 12, C->material[f].specular, 16);
        m[16] = C->material[f].shininess;
    }

    for (int i = 0; i < MGL_COMPAT_LIGHTS; i++)
    {
        MGLLight *L = &C->lights[i];
        GLfloat *v = vec + (FV_LIGHT + i * 6) * 4;

        memcpy(v, L->ambient, 16);
        memcpy(v + 4, L->diffuse, 16);
        memcpy(v + 8, L->specular, 16);
        memcpy(v + 12, L->position, 16);
        memcpy(v + 16, L->spot_direction, 12);
        v[19] = L->spot_exponent;
        v[20] = cosf(L->spot_cutoff * (GLfloat)M_PI / 180.0f);
        memcpy(v + 21, L->attenuation, 12);
    }

    for (int i = 0; i < MGL_COMPAT_PLANES; i++)
        memcpy(vec + (FV_CLIP + i) * 4, C->clip_planes[i], 16);

    for (int u = 0; u < MGL_COMPAT_UNITS; u++)
    {
        GLfloat *g = vec + (FV_TEXGEN + u * 8) * 4;

        memcpy(g, C->tex[u].obj_plane, 64);
        memcpy(g + 16, C->tex[u].eye_plane, 64);

        memcpy(frag + (FG_ENV + u) * 4, C->tex[u].color, 16);
        frag[(FG_SCALE + u) * 4 + 0] = C->tex[u].lod_bias;
        frag[(FG_SCALE + u) * 4 + 1] = C->tex[u].rgb_scale;
        frag[(FG_SCALE + u) * 4 + 2] = C->tex[u].alpha_scale;
        frag[(FG_SCALE + u) * 4 + 3] = 1.0f;
    }

    memcpy(frag + FG_FOG_COLOR * 4, C->fog_color, 16);
    frag[FG_FOG * 4 + 0] = C->fog_density;
    frag[FG_FOG * 4 + 1] = C->fog_start;
    frag[FG_FOG * 4 + 2] = C->fog_end;
    frag[FG_FOG * 4 + 3] = C->fog_end != C->fog_start ? 1.0f / (C->fog_end - C->fog_start) : 0.0f;
}

static void writeFF(GLMContext ctx, MGLFFProgram *ff)
{
    GLfloat mat[FF_MATRICES * 16], normal[9], vec[FV_COUNT * 4], frag[FG_COUNT * 4];
    GLenum err = STATE(error);

    ffValues(ctx, mat, normal, vec, frag);

    ctx->error_suppress++;

    if (ff->loc_matrix >= 0 && (!ff->written || memcmp(mat, ff->matrix, sizeof(mat))))
        mglProgramUniformMatrix4fv(ctx, ff->name, ff->loc_matrix, FF_MATRICES, GL_FALSE, mat);
    if (ff->loc_normal >= 0 && (!ff->written || memcmp(normal, ff->normal, sizeof(normal))))
        mglProgramUniformMatrix3fv(ctx, ff->name, ff->loc_normal, 1, GL_FALSE, normal);
    if (ff->loc_vec >= 0 && (!ff->written || memcmp(vec, ff->vec, sizeof(vec))))
        mglProgramUniform4fv(ctx, ff->name, ff->loc_vec, FV_COUNT, vec);
    if (ff->loc_frag >= 0 && (!ff->written || memcmp(frag, ff->frag, sizeof(frag))))
        mglProgramUniform4fv(ctx, ff->name, ff->loc_frag, FG_COUNT, frag);

    ctx->error_suppress--;
    STATE(error) = err;

    memcpy(ff->matrix, mat, sizeof(mat));
    memcpy(ff->normal, normal, sizeof(normal));
    memcpy(ff->vec, vec, sizeof(vec));
    memcpy(ff->frag, frag, sizeof(frag));
    ff->written = true;
}

void mglCompatWriteBuiltins(GLMContext ctx, Program *prog)
{
    struct MGLBuiltinCache_t *cache;
    GLenum err = STATE(error);

    if (prog == NULL || !prog->executable)
        return;

    for (MGLFFProgram *ff = C->ff_programs; ff; ff = ff->next)
    {
        if (ff->prog == prog)
        {
            writeFF(ctx, ff);
            return;
        }
    }

    if (prog->compat_cache == NULL)
        prog->compat_cache = buildCache(ctx, prog);

    cache = (struct MGLBuiltinCache_t *)prog->compat_cache;
    if (cache == NULL || cache->n == 0)
        return;

    ctx->error_suppress++;

    for (int i = 0; i < cache->n; i++)
    {
        CacheEntry *e = &cache->e[i];
        const Builtin *b = &builtins[e->builtin];
        GLfloat v[16];
        int n = value(ctx, b->what, e->index, v);

        if (n == 0 || (e->written && !memcmp(e->last, v, (size_t)n * sizeof(GLfloat))))
            continue;

        switch (b->kind)
        {
            case K_MAT4: mglProgramUniformMatrix4fv(ctx, prog->name, e->loc, 1, GL_FALSE, v); break;
            case K_MAT3: mglProgramUniformMatrix3fv(ctx, prog->name, e->loc, 1, GL_FALSE, v); break;
            case K_VEC4: mglProgramUniform4fv(ctx, prog->name, e->loc, 1, v); break;
            case K_VEC3: mglProgramUniform3fv(ctx, prog->name, e->loc, 1, v); break;
            default: mglProgramUniform1fv(ctx, prog->name, e->loc, 1, v); break;
        }

        memcpy(e->last, v, (size_t)n * sizeof(GLfloat));
        e->written = true;
    }

    ctx->error_suppress--;
    STATE(error) = err;
}

#endif /* MGL_COMPAT_CONTEXTS */
