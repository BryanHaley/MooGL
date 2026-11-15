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
 * compat_glsl.c
 * MGL
 *
 * Old GLSL in a compatibility context. A shader written for GLSL 1.10 to
 * 1.30, or for a compatibility profile, reads gl_ModelViewMatrix, gl_Vertex,
 * gl_TexCoord and the rest, which glslang will not put in SPIR-V. They are
 * renamed to declarations of the driver's own, which compat_ff.c fills in
 * before each draw, and the result is compiled as GLSL 4.10.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glm_context.h"
#include "compat.h"

#ifdef MGL_COMPAT_CONTEXTS

extern const char *mglCompatBuiltinDecls;

typedef struct {
    char *s;
    size_t len, cap;
} Out;

static void put(Out *o, const char *p, size_t n)
{
    if (o->len + n + 1 > o->cap)
    {
        size_t cap = (o->len + n + 1) * 2;
        char *s = (char *)realloc(o->s, cap);

        if (s == NULL)
            return;

        o->s = s;
        o->cap = cap;
    }

    memcpy(o->s + o->len, p, n);
    o->len += n;
    o->s[o->len] = 0;
}

static void puts_(Out *o, const char *p)
{
    put(o, p, strlen(p));
}

static void printf_(Out *o, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void printf_(Out *o, const char *fmt, ...)
{
    char buf[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    puts_(o, buf);
}

// --- what the old names become -------------------------------------------------------

enum { ST_ANY, ST_VS, ST_FS };

typedef struct {
    const char *name;
    int stage;            // where the mapping applies
    const char *to;       // the new name
    const char *decl;     // what has to be declared for it, once
} Rename;

static const Rename renames[] = {
    { "gl_Vertex", ST_VS, "mgl_Vertex", "layout(location = 0) in vec4 mgl_Vertex;" },
    { "gl_Normal", ST_VS, "mgl_Normal", "layout(location = 2) in vec3 mgl_Normal;" },
    { "gl_Color", ST_VS, "mgl_Color", "layout(location = 3) in vec4 mgl_Color;" },
    { "gl_SecondaryColor", ST_VS, "mgl_SecondaryColor", "layout(location = 4) in vec4 mgl_SecondaryColor;" },
    { "gl_FogCoord", ST_VS, "mgl_FogCoord", "layout(location = 5) in float mgl_FogCoord;" },
    { "gl_MultiTexCoord0", ST_VS, "mgl_MultiTexCoord0", "layout(location = 8) in vec4 mgl_MultiTexCoord0;" },
    { "gl_MultiTexCoord1", ST_VS, "mgl_MultiTexCoord1", "layout(location = 9) in vec4 mgl_MultiTexCoord1;" },
    { "gl_MultiTexCoord2", ST_VS, "mgl_MultiTexCoord2", "layout(location = 10) in vec4 mgl_MultiTexCoord2;" },
    { "gl_MultiTexCoord3", ST_VS, "mgl_MultiTexCoord3", "layout(location = 11) in vec4 mgl_MultiTexCoord3;" },
    { "gl_MultiTexCoord4", ST_VS, "mgl_MultiTexCoord4", "layout(location = 12) in vec4 mgl_MultiTexCoord4;" },
    { "gl_MultiTexCoord5", ST_VS, "mgl_MultiTexCoord5", "layout(location = 13) in vec4 mgl_MultiTexCoord5;" },
    { "gl_MultiTexCoord6", ST_VS, "mgl_MultiTexCoord6", "layout(location = 14) in vec4 mgl_MultiTexCoord6;" },
    { "gl_MultiTexCoord7", ST_VS, "mgl_MultiTexCoord7", "layout(location = 15) in vec4 mgl_MultiTexCoord7;" },
    { "gl_FrontColor", ST_VS, "mgl_FrontColor", "out vec4 mgl_FrontColor;" },
    { "gl_BackColor", ST_VS, "mgl_BackColor", "out vec4 mgl_BackColor;" },
    { "gl_FrontSecondaryColor", ST_VS, "mgl_FrontSecondaryColor", "out vec4 mgl_FrontSecondaryColor;" },
    { "gl_BackSecondaryColor", ST_VS, "mgl_BackSecondaryColor", "out vec4 mgl_BackSecondaryColor;" },
    { "gl_TexCoord", ST_VS, "mgl_TexCoord", "out vec4 mgl_TexCoord[8];" },
    { "gl_FogFragCoord", ST_VS, "mgl_FogFragCoord", "out float mgl_FogFragCoord;" },
    { "gl_ClipVertex", ST_VS, "mgl_ClipVertex", "vec4 mgl_ClipVertex;" },
    { "gl_Color", ST_FS, "mgl_FrontColor", "in vec4 mgl_FrontColor;" },
    { "gl_SecondaryColor", ST_FS, "mgl_FrontSecondaryColor", "in vec4 mgl_FrontSecondaryColor;" },
    { "gl_TexCoord", ST_FS, "mgl_TexCoord", "in vec4 mgl_TexCoord[8];" },
    { "gl_FogFragCoord", ST_FS, "mgl_FogFragCoord", "in float mgl_FogFragCoord;" },
    { "gl_FragColor", ST_FS, "mgl_FragColor", "layout(location = 0) out vec4 mgl_FragColor;" },
    { "gl_FragData", ST_FS, "mgl_FragData", "layout(location = 0) out vec4 mgl_FragData[8];" },

    { "gl_ModelViewMatrix", ST_ANY, "mgl_ModelViewMatrix", "uniform mat4 mgl_ModelViewMatrix;" },
    { "gl_ProjectionMatrix", ST_ANY, "mgl_ProjectionMatrix", "uniform mat4 mgl_ProjectionMatrix;" },
    { "gl_ModelViewProjectionMatrix", ST_ANY, "mgl_ModelViewProjectionMatrix", "uniform mat4 mgl_ModelViewProjectionMatrix;" },
    { "gl_ModelViewMatrixInverse", ST_ANY, "mgl_ModelViewMatrixInverse", "uniform mat4 mgl_ModelViewMatrixInverse;" },
    { "gl_ProjectionMatrixInverse", ST_ANY, "mgl_ProjectionMatrixInverse", "uniform mat4 mgl_ProjectionMatrixInverse;" },
    { "gl_ModelViewProjectionMatrixInverse", ST_ANY, "mgl_ModelViewProjectionMatrixInverse",
      "uniform mat4 mgl_ModelViewProjectionMatrixInverse;" },
    { "gl_ModelViewMatrixTranspose", ST_ANY, "mgl_ModelViewMatrixTranspose", "uniform mat4 mgl_ModelViewMatrixTranspose;" },
    { "gl_ProjectionMatrixTranspose", ST_ANY, "mgl_ProjectionMatrixTranspose", "uniform mat4 mgl_ProjectionMatrixTranspose;" },
    { "gl_ModelViewProjectionMatrixTranspose", ST_ANY, "mgl_ModelViewProjectionMatrixTranspose",
      "uniform mat4 mgl_ModelViewProjectionMatrixTranspose;" },
    { "gl_ModelViewMatrixInverseTranspose", ST_ANY, "mgl_ModelViewMatrixInverseTranspose",
      "uniform mat4 mgl_ModelViewMatrixInverseTranspose;" },
    { "gl_ProjectionMatrixInverseTranspose", ST_ANY, "mgl_ProjectionMatrixInverseTranspose",
      "uniform mat4 mgl_ProjectionMatrixInverseTranspose;" },
    { "gl_ModelViewProjectionMatrixInverseTranspose", ST_ANY, "mgl_ModelViewProjectionMatrixInverseTranspose",
      "uniform mat4 mgl_ModelViewProjectionMatrixInverseTranspose;" },
    { "gl_NormalMatrix", ST_ANY, "mgl_NormalMatrix", "uniform mat3 mgl_NormalMatrix;" },
    { "gl_NormalScale", ST_ANY, "mgl_NormalScale", "uniform float mgl_NormalScale;" },
    { "gl_TextureMatrix", ST_ANY, "mgl_TextureMatrix", "uniform mat4 mgl_TextureMatrix[8];" },
    { "gl_TextureMatrixInverse", ST_ANY, "mgl_TextureMatrixInverse", "uniform mat4 mgl_TextureMatrixInverse[8];" },
    { "gl_TextureMatrixTranspose", ST_ANY, "mgl_TextureMatrixTranspose", "uniform mat4 mgl_TextureMatrixTranspose[8];" },
    { "gl_TextureMatrixInverseTranspose", ST_ANY, "mgl_TextureMatrixInverseTranspose",
      "uniform mat4 mgl_TextureMatrixInverseTranspose[8];" },
    { "gl_ClipPlane", ST_ANY, "mgl_ClipPlane", "uniform vec4 mgl_ClipPlane[8];" },
    { "gl_TextureEnvColor", ST_ANY, "mgl_TextureEnvColor", "uniform vec4 mgl_TextureEnvColor[8];" },
    { "gl_EyePlaneS", ST_ANY, "mgl_EyePlaneS", "uniform vec4 mgl_EyePlaneS[8];" },
    { "gl_EyePlaneT", ST_ANY, "mgl_EyePlaneT", "uniform vec4 mgl_EyePlaneT[8];" },
    { "gl_EyePlaneR", ST_ANY, "mgl_EyePlaneR", "uniform vec4 mgl_EyePlaneR[8];" },
    { "gl_EyePlaneQ", ST_ANY, "mgl_EyePlaneQ", "uniform vec4 mgl_EyePlaneQ[8];" },
    { "gl_ObjectPlaneS", ST_ANY, "mgl_ObjectPlaneS", "uniform vec4 mgl_ObjectPlaneS[8];" },
    { "gl_ObjectPlaneT", ST_ANY, "mgl_ObjectPlaneT", "uniform vec4 mgl_ObjectPlaneT[8];" },
    { "gl_ObjectPlaneR", ST_ANY, "mgl_ObjectPlaneR", "uniform vec4 mgl_ObjectPlaneR[8];" },
    { "gl_ObjectPlaneQ", ST_ANY, "mgl_ObjectPlaneQ", "uniform vec4 mgl_ObjectPlaneQ[8];" },
    { "gl_LightSource", ST_ANY, "mgl_LightSource", "uniform mgl_LightSourceParameters mgl_LightSource[8];" },
    { "gl_LightModel", ST_ANY, "mgl_LightModel", "uniform mgl_LightModelParameters mgl_LightModel;" },
    { "gl_FrontMaterial", ST_ANY, "mgl_FrontMaterial", "uniform mgl_MaterialParameters mgl_FrontMaterial;" },
    { "gl_BackMaterial", ST_ANY, "mgl_BackMaterial", "uniform mgl_MaterialParameters mgl_BackMaterial;" },
    { "gl_FrontLightModelProduct", ST_ANY, "mgl_FrontLightModelProduct",
      "uniform mgl_LightModelProducts mgl_FrontLightModelProduct;" },
    { "gl_BackLightModelProduct", ST_ANY, "mgl_BackLightModelProduct",
      "uniform mgl_LightModelProducts mgl_BackLightModelProduct;" },
    { "gl_FrontLightProduct", ST_ANY, "mgl_FrontLightProduct", "uniform mgl_LightProducts mgl_FrontLightProduct[8];" },
    { "gl_BackLightProduct", ST_ANY, "mgl_BackLightProduct", "uniform mgl_LightProducts mgl_BackLightProduct[8];" },
    { "gl_Fog", ST_ANY, "mgl_Fog", "uniform mgl_FogParameters mgl_Fog;" },
    { "gl_Point", ST_ANY, "mgl_Point", "uniform mgl_PointParameters mgl_Point;" },
    { "gl_MaxLights", ST_ANY, "8", NULL },
    { "gl_MaxClipPlanes", ST_ANY, "8", NULL },
    { "gl_MaxTextureUnits", ST_ANY, "8", NULL },
    { "gl_MaxTextureCoords", ST_ANY, "8", NULL },
};

#define RENAMES (int)(sizeof(renames) / sizeof(renames[0]))

// the old texture functions, as overloads of the new ones; a variable named
// texture, common in old shaders, then hides nothing they need
static const struct { const char *name; const char *defs; bool fs_bias; } lookups[] = {
    { "texture1D", "vec4 mgl_texture1D(sampler1D s, float c) { return texture(s, c); }\n",
      true },
    { "texture2D", "vec4 mgl_texture2D(sampler2D s, vec2 c) { return texture(s, c); }\n", true },
    { "texture3D", "vec4 mgl_texture3D(sampler3D s, vec3 c) { return texture(s, c); }\n", true },
    { "textureCube", "vec4 mgl_textureCube(samplerCube s, vec3 c) { return texture(s, c); }\n", true },
    { "texture2DRect", "vec4 mgl_texture2DRect(sampler2DRect s, vec2 c) { return texture(s, c); }\n", false },
    { "texture1DProj",
      "vec4 mgl_texture1DProj(sampler1D s, vec2 c) { return textureProj(s, c); }\n"
      "vec4 mgl_texture1DProj(sampler1D s, vec4 c) { return textureProj(s, c); }\n", true },
    { "texture2DProj",
      "vec4 mgl_texture2DProj(sampler2D s, vec3 c) { return textureProj(s, c); }\n"
      "vec4 mgl_texture2DProj(sampler2D s, vec4 c) { return textureProj(s, c); }\n", true },
    { "texture3DProj", "vec4 mgl_texture3DProj(sampler3D s, vec4 c) { return textureProj(s, c); }\n", true },
    { "texture2DRectProj",
      "vec4 mgl_texture2DRectProj(sampler2DRect s, vec3 c) { return textureProj(s, c); }\n"
      "vec4 mgl_texture2DRectProj(sampler2DRect s, vec4 c) { return textureProj(s, c); }\n", false },
    { "texture1DLod", "vec4 mgl_texture1DLod(sampler1D s, float c, float l) { return textureLod(s, c, l); }\n", false },
    { "texture2DLod", "vec4 mgl_texture2DLod(sampler2D s, vec2 c, float l) { return textureLod(s, c, l); }\n", false },
    { "texture3DLod", "vec4 mgl_texture3DLod(sampler3D s, vec3 c, float l) { return textureLod(s, c, l); }\n", false },
    { "textureCubeLod", "vec4 mgl_textureCubeLod(samplerCube s, vec3 c, float l) { return textureLod(s, c, l); }\n",
      false },
    { "texture2DProjLod",
      "vec4 mgl_texture2DProjLod(sampler2D s, vec3 c, float l) { return textureProjLod(s, c, l); }\n"
      "vec4 mgl_texture2DProjLod(sampler2D s, vec4 c, float l) { return textureProjLod(s, c, l); }\n", false },
    { "shadow1D", "vec4 mgl_shadow1D(sampler1DShadow s, vec3 c) { return vec4(texture(s, c)); }\n", true },
    { "shadow2D", "vec4 mgl_shadow2D(sampler2DShadow s, vec3 c) { return vec4(texture(s, c)); }\n", true },
    { "shadow2DProj", "vec4 mgl_shadow2DProj(sampler2DShadow s, vec4 c) { return vec4(textureProj(s, c)); }\n", true },
    { "shadow2DLod",
      "vec4 mgl_shadow2DLod(sampler2DShadow s, vec3 c, float l) { return vec4(textureLod(s, c, l)); }\n", false },
    { "shadow2DRect", "vec4 mgl_shadow2DRect(sampler2DRectShadow s, vec3 c) { return vec4(texture(s, c)); }\n", false },
};

#define LOOKUPS (int)(sizeof(lookups) / sizeof(lookups[0]))

// the bias overload a fragment shader may also call, from the plain one
static void biasOverloads(Out *o, const char *defs)
{
    const char *p = defs;

    while (*p)
    {
        const char *eol = strchr(p, '\n');
        size_t n = eol ? (size_t)(eol - p) : strlen(p);
        char line[512], out[600];
        char *paren, *ret;

        if (n >= sizeof(line))
            n = sizeof(line) - 1;

        memcpy(line, p, n);
        line[n] = 0;

        // "... vec2 c) { return texture(s, c); }" -> "... vec2 c, float b) { return texture(s, c, b); }"
        paren = strstr(line, ") {");
        ret = strstr(line, "(s, c)");

        if (paren && ret && paren < ret)
        {
            *paren = 0;
            *ret = 0;
            snprintf(out, sizeof(out), "%s, float b) {%s(s, c, b)%s\n", line, paren + 3, ret + 6);
            puts_(o, out);
        }

        p += n + (eol ? 1 : 0);
    }
}

static bool identStart(char c)
{
    return isalpha((unsigned char)c) || c == '_';
}

static bool identChar(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

// Whether a shader with this #version line is old enough, or asks for the
// compatibility profile. es shaders are left alone.
static bool wantsRewrite(const char *src, int *version)
{
    const char *v = strstr(src, "#version");

    *version = 110;

    if (v == NULL)
        return true;

    if (sscanf(v, "#version %d", version) != 1)
        return false;

    const char *eol = strchr(v, '\n');
    size_t n = eol ? (size_t)(eol - v) : strlen(v);

    if (memmem(v, n, " es", 3))
        return false;

    if (memmem(v, n, "compatibility", 13))
        return true;

    return *version < 140;
}

char *mglCompatRewriteGLSL(const char *src, GLenum type)
{
    int version, stage = type == GL_VERTEX_SHADER ? ST_VS : type == GL_FRAGMENT_SHADER ? ST_FS : ST_ANY;
    bool used[RENAMES] = { false }, used_lookup[LOOKUPS] = { false }, ftransform = false;
    const char *p = src, *insert;
    int insert_line = 1;
    Out body = { 0 }, out = { 0 };

    if (!wantsRewrite(src, &version))
        return NULL;

    // the leading #version and #extension lines stay ahead of everything added
    insert = src;
    for (const char *line = src; *line;)
    {
        const char *s = line;
        const char *eol = strchr(line, '\n');

        while (*s == ' ' || *s == '\t' || *s == '\r')
            s++;

        if (*s == '\n' || *s == 0 || !strncmp(s, "#version", 8) || !strncmp(s, "#extension", 10) ||
            !strncmp(s, "//", 2))
        {
            if (!eol)
            {
                insert = line + strlen(line);
                break;
            }

            line = eol + 1;
            insert = line;
            insert_line++;
            continue;
        }

        break;
    }

    // the body after them, with the old names replaced
    p = insert;

    while (*p)
    {
        if (p[0] == '/' && p[1] == '/')
        {
            const char *e = strchr(p, '\n');
            size_t n = e ? (size_t)(e - p) : strlen(p);
            put(&body, p, n);
            p += n;
            continue;
        }

        if (p[0] == '/' && p[1] == '*')
        {
            const char *e = strstr(p + 2, "*/");
            size_t n = e ? (size_t)(e + 2 - p) : strlen(p);
            put(&body, p, n);
            p += n;
            continue;
        }

        if (identStart(*p) && (p == src || !identChar(p[-1])))
        {
            const char *e = p;
            char word[128];
            size_t n;
            bool done = false;

            while (identChar(*e))
                e++;

            n = (size_t)(e - p);

            if (n < sizeof(word))
            {
                memcpy(word, p, n);
                word[n] = 0;

                if (!strcmp(word, "attribute") && stage == ST_VS)
                {
                    puts_(&body, "in");
                    done = true;
                }
                else if (!strcmp(word, "varying") && stage != ST_ANY)
                {
                    puts_(&body, stage == ST_VS ? "out" : "in");
                    done = true;
                }
                else if (!strcmp(word, "ftransform"))
                {
                    puts_(&body, "mgl_ftransform");
                    ftransform = true;
                    done = true;
                }

                for (int i = 0; !done && i < LOOKUPS; i++)
                {
                    if (!strcmp(word, lookups[i].name))
                    {
                        printf_(&body, "mgl_%s", word);
                        used_lookup[i] = true;
                        done = true;
                    }
                }

                for (int i = 0; !done && i < RENAMES; i++)
                {
                    if ((renames[i].stage == ST_ANY || renames[i].stage == stage) && !strcmp(word, renames[i].name))
                    {
                        puts_(&body, renames[i].to);
                        used[i] = true;
                        done = true;
                    }
                }
            }

            if (!done)
                put(&body, p, n);

            p = e;
            continue;
        }

        put(&body, p, 1);
        p++;
    }

    if (ftransform)
    {
        for (int i = 0; i < RENAMES; i++)
            if (!strcmp(renames[i].name, "gl_ModelViewProjectionMatrix") ||
                (!strcmp(renames[i].name, "gl_Vertex") && renames[i].stage == ST_VS))
                used[i] = true;
    }

    // the new version line, the leading lines that came after the old one,
    // the declarations, and the body back on its own line numbers
    puts_(&out, "#version 410 core\n");

    for (const char *line = src; line < insert;)
    {
        const char *eol = strchr(line, '\n');
        size_t n = eol ? (size_t)(eol + 1 - line) : strlen(line);
        const char *s = line;

        while (*s == ' ' || *s == '\t')
            s++;

        if (strncmp(s, "#version", 8))
            put(&out, line, n);

        line += n;
    }

    for (int i = 0; i < RENAMES; i++)
    {
        if (used[i] && renames[i].decl && strstr(renames[i].decl, "uniform mgl_"))
        {
            puts_(&out, mglCompatBuiltinDecls);
            break;
        }
    }

    for (int i = 0; i < RENAMES; i++)
    {
        if (!used[i] || renames[i].decl == NULL)
            continue;

        // the same name mapped twice declares once
        bool seen = false;
        for (int j = 0; j < i; j++)
            if (used[j] && renames[j].decl && !strcmp(renames[j].decl, renames[i].decl))
                seen = true;

        if (!seen)
            printf_(&out, "%s\n", renames[i].decl);
    }

    if (ftransform)
        puts_(&out, "vec4 mgl_ftransform() { return mgl_ModelViewProjectionMatrix * mgl_Vertex; }\n");

    for (int i = 0; i < LOOKUPS; i++)
    {
        if (!used_lookup[i])
            continue;

        puts_(&out, lookups[i].defs);

        if (stage == ST_FS && lookups[i].fs_bias)
            biasOverloads(&out, lookups[i].defs);
    }

    printf_(&out, "#line %d\n", insert_line);
    put(&out, body.s ? body.s : "", body.len);

    free(body.s);
    return out.s;
}

#endif /* MGL_COMPAT_CONTEXTS */
