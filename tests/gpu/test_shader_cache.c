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
 * test_shader_cache.c
 *
 * A program built from the shader cache has to draw and answer queries the
 * way one built fresh does, and a damaged cache file has to be made again.
 */

#include "mgl_test.h"
#include "harness.h"
#include <fts.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef GL_PATCHES
#define GL_PATCHES 0x000E
#endif

void mglShaderCacheUseDir(const char *dir);
unsigned mglShaderCacheHits(void);

#define N 64

static const char *VS =
    "#version 410\n"
    "in vec2 p;\n"
    "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";

static const char *TCS =
    "#version 410\n"
    "layout(vertices = 3) out;\n"
    "void main() {\n"
    "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
    "    gl_TessLevelOuter[0] = 6.0; gl_TessLevelOuter[1] = 6.0; gl_TessLevelOuter[2] = 6.0;\n"
    "    gl_TessLevelInner[0] = 6.0;\n"
    "}\n";

static const char *TES =
    "#version 410\n"
    "layout(triangles, equal_spacing, ccw) in;\n"
    "out vec3 c;\n"
    "void main() {\n"
    "    vec4 q = gl_TessCoord.x * gl_in[0].gl_Position + gl_TessCoord.y * gl_in[1].gl_Position\n"
    "           + gl_TessCoord.z * gl_in[2].gl_Position;\n"
    "    float s = 0.25 * sin(9.0 * gl_TessCoord.x) * sin(9.0 * gl_TessCoord.y);\n"
    "    c = gl_TessCoord;\n"
    "    gl_Position = vec4(q.xy * 0.8 + vec2(s), 0.0, 1.0);\n"
    "}\n";

// the same with another warp, for a program that shares the other stages
static const char *TES2 =
    "#version 410\n"
    "layout(triangles, equal_spacing, ccw) in;\n"
    "out vec3 c;\n"
    "void main() {\n"
    "    vec4 q = gl_TessCoord.x * gl_in[0].gl_Position + gl_TessCoord.y * gl_in[1].gl_Position\n"
    "           + gl_TessCoord.z * gl_in[2].gl_Position;\n"
    "    float s = 0.25 * sin(7.0 * gl_TessCoord.x) * sin(7.0 * gl_TessCoord.y);\n"
    "    c = gl_TessCoord.zyx;\n"
    "    gl_Position = vec4(q.xy * 0.8 + vec2(s), 0.0, 1.0);\n"
    "}\n";

// no layout(location), so the linker picks where o goes
static const char *FS =
    "#version 410\n"
    "in vec3 c;\n"
    "out vec4 o;\n"
    "void main() { o = vec4(c, 1.0); }\n";

static GLuint build(const char *tes)
{
    const char *src[4] = { VS, TCS, tes, FS };
    GLenum type[4] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_FRAGMENT_SHADER };
    GLuint prog = glCreateProgram();
    GLint ok = 0;

    for (int i = 0; i < 4; i++)
    {
        GLuint sh = glCreateShader(type[i]);

        glShaderSource(sh, 1, &src[i], NULL);
        glCompileShader(sh);
        glAttachShader(prog, sh);
        glDeleteShader(sh);
    }

    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok)
    {
        glDeleteProgram(prog);
        return 0;
    }

    return prog;
}

// Draws a tessellated triangle with a fresh build of the program; the pixels,
// and where the program says o and p went
static unsigned char *drawWith(MGLTestTarget *t, const char *tes, GLint *out_loc, GLint *in_loc)
{
    static const GLfloat tri[6] = { -1, -1, 1, -1, -1, 1 };
    GLuint prog = build(tes), vao, vbo;
    unsigned char *px;

    CHECK_MSG(prog != 0, "the program did not link");
    if (prog == 0)
        return NULL;

    *out_loc = glGetProgramResourceLocation(prog, GL_PROGRAM_OUTPUT, "o");
    *in_loc = glGetAttribLocation(prog, "p");

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glVertexAttribPointer((GLuint)*in_loc, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray((GLuint)*in_loc);

    glUseProgram(prog);
    glViewport(0, 0, N, N);
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_PATCHES, 0, 3);
    px = mgl_read_rgba8(t);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    return px;
}

// Where a separable vertex program says its output went: the linker picks, and
// the answer comes from reflecting the linked stage.
static GLint separableOutput(void)
{
    static const char *src =
        "#version 410\n"
        "in vec4 q;\n"
        "out vec4 v;\n"
        "void main() { v = q; gl_Position = q; }\n";
    GLuint prog = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &src);
    GLint loc = glGetProgramResourceLocation(prog, GL_PROGRAM_OUTPUT, "v");

    glDeleteProgram(prog);
    return loc;
}

static unsigned char *drawOnce(MGLTestTarget *t, GLint *out_loc, GLint *in_loc)
{
    return drawWith(t, TES, out_loc, in_loc);
}

static int differing(const unsigned char *a, const unsigned char *b)
{
    int n = 0;

    for (int i = 0; a && b && i < N * N; i++)
        n += memcmp(a + i * 4, b + i * 4, 4) != 0;

    return a && b ? n : -1;
}

static void removeTree(const char *dir)
{
    char *roots[] = { (char *)dir, NULL };
    FTS *fts = fts_open(roots, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
    FTSENT *e;

    if (fts == NULL)
        return;

    while ((e = fts_read(fts)) != NULL)
    {
        if (e->fts_info == FTS_F)
            unlink(e->fts_path);
        else if (e->fts_info == FTS_DP)
            rmdir(e->fts_path);
    }

    fts_close(fts);
}

// Flips a byte in the middle of every cache file, where the Metal source or
// SPIR-V is, so a damaged entry that was used would break the draw
static int damageTree(const char *dir)
{
    char *roots[] = { (char *)dir, NULL };
    FTS *fts = fts_open(roots, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
    FTSENT *e;
    int n = 0;

    if (fts == NULL)
        return 0;

    while ((e = fts_read(fts)) != NULL)
    {
        FILE *f;
        int c;

        if (e->fts_info != FTS_F || e->fts_statp->st_size < 2 || (f = fopen(e->fts_path, "r+b")) == NULL)
            continue;

        fseek(f, e->fts_statp->st_size / 2, SEEK_SET);
        c = fgetc(f);
        fseek(f, e->fts_statp->st_size / 2, SEEK_SET);
        fputc(c ^ 0x5a, f);
        fclose(f);
        n++;
    }

    fts_close(fts);
    return n;
}

static bool tempDir(char *dir, size_t n)
{
    snprintf(dir, n, "%s/mgl_shader_cache_XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
    return mkdtemp(dir) != NULL;
}

GPU_TEST(shader_cache, a_program_from_the_cache_draws_like_a_fresh_one)
{
    MGLTestTarget t;
    char dir[1024];
    GLint fresh_out, fresh_in, cached_out, cached_in;

    if (!tempDir(dir, sizeof dir) || !mgl_target_create(&t, N, N, GL_RGBA8, 0))
        SKIP("no target or temporary folder");

    mgl_target_bind(&t);
    mglShaderCacheUseDir(dir);

    unsigned char *fresh = drawOnce(&t, &fresh_out, &fresh_in);
    GLint fresh_v = separableOutput();
    unsigned hits = mglShaderCacheHits();
    unsigned char *cached = drawOnce(&t, &cached_out, &cached_in);
    GLint cached_v = separableOutput();

    CHECK_MSG(mglShaderCacheHits() > hits, "the second build read nothing from the cache");
    CHECK(fresh != NULL && cached != NULL);

    if (fresh && cached)
    {
        int lit = 0, differ = 0;

        for (int i = 0; i < N * N; i++)
        {
            lit += fresh[i * 4] + fresh[i * 4 + 1] + fresh[i * 4 + 2] > 0;
            differ += memcmp(fresh + i * 4, cached + i * 4, 4) != 0;
        }

        CHECK_MSG(lit > N * N / 8, "the fresh build drew %d pixels", lit);
        CHECK_MSG(differ == 0, "%d pixels differ between the fresh and cached builds", differ);
    }

    CHECK_EQ_INT(fresh_out, 0);
    CHECK_EQ_INT(cached_out, fresh_out);
    CHECK_EQ_INT(cached_in, fresh_in);
    CHECK_EQ_INT(cached_v, fresh_v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // A stage from the cache next to one built now: the built one still sees
    // what the cached one would have left, here the control stage's patch size.
    char empty[1024];
    GLint o, i;

    if (tempDir(empty, sizeof empty))
    {
        unsigned char *mixed = drawWith(&t, TES2, &o, &i);

        mglShaderCacheUseDir(empty);

        unsigned char *alone = drawWith(&t, TES2, &o, &i);

        CHECK_MSG(differing(mixed, alone) == 0, "%d pixels differ with only some stages cached",
                  differing(mixed, alone));
        free(mixed);
        free(alone);
        removeTree(empty);
    }

    free(fresh);
    free(cached);
    mglShaderCacheUseDir(NULL);
    removeTree(dir);
    mgl_target_destroy(&t);
}

GPU_TEST(shader_cache, a_damaged_entry_is_made_again)
{
    MGLTestTarget t;
    char dir[1024];
    GLint out_loc, in_loc;

    if (!tempDir(dir, sizeof dir) || !mgl_target_create(&t, N, N, GL_RGBA8, 0))
        SKIP("no target or temporary folder");

    mgl_target_bind(&t);
    mglShaderCacheUseDir(dir);

    unsigned char *first = drawOnce(&t, &out_loc, &in_loc);
    int damaged = damageTree(dir);
    unsigned char *again = drawOnce(&t, &out_loc, &in_loc);

    CHECK_MSG(damaged > 0, "the first build wrote nothing to the cache");

    unsigned hits = mglShaderCacheHits();
    unsigned char *third = drawOnce(&t, &out_loc, &in_loc);

    CHECK_MSG(mglShaderCacheHits() > hits, "the damaged entries were not written again");
    CHECK(first && again && third && !memcmp(first, again, N * N * 4) && !memcmp(first, third, N * N * 4));
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    free(first);
    free(again);
    free(third);
    mglShaderCacheUseDir(NULL);
    removeTree(dir);
    mgl_target_destroy(&t);
}
