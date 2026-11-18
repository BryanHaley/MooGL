/*
 * Copyright (C) The MooGL Project
 *
 * test_es_context.c
 * MGL
 *
 * OpenGL ES 3.1 contexts from libmoogl: the ES strings and queries, the
 * enables ES has, and GLSL ES 1.00, 3.00 and 3.10 shaders, checked under
 * the ES rules.
 */

#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "harness.h"
#include "MGLContext.h"
#include "legacy_gl.h"

#ifdef MGL_OGL_ES

#define W 16
#define H 16

extern void *CppCreateMGLRendererHeadless(void *glm_ctx);

static GLMContext was;

static GLMContext openES(MGLTestTarget *t)
{
    GLMContext c;

    was = MGLgetCurrentContext();
    c = createGLMContextProfile(GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, GL_DEPTH_COMPONENT, GL_FLOAT,
                                GL_STENCIL_INDEX8, GL_UNSIGNED_BYTE, NULL, MGL_CONTEXT_ES_PROFILE_BIT);
    if (c == NULL)
        return NULL;

    MGLsetCurrentContext(c);
    CppCreateMGLRendererHeadless(c);

    if (!mgl_target_create(t, W, H, GL_RGBA8, 0))
    {
        MGLsetCurrentContext(was);
        destroyGLMContext(c);
        return NULL;
    }

    glViewport(0, 0, W, H);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    return c;
}

static void closeES(GLMContext c, MGLTestTarget *t)
{
    mgl_target_destroy(t);
    MGLsetCurrentContext(was);
    destroyGLMContext(c);
}

static GLboolean compiles(GLenum type, const char *src, char *log, int size)
{
    GLuint s = glCreateShader(type);
    GLint ok = 0;

    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    glGetShaderInfoLog(s, size, NULL, log);
    glDeleteShader(s);
    return ok == GL_TRUE;
}

// draws the quad with vs and fs, and returns the centre pixel
static void drawn(MGLTestTarget *t, const char *vs, const char *fs, unsigned char out[4])
{
    char log[1024] = { 0 };
    GLuint p = mgl_build_program(vs, fs, log, sizeof log), vbo = 0, vao;
    unsigned char *px;

    memset(out, 0, 4);
    CHECK_MSG(p != 0, "link: %s", log);
    if (!p)
        return;

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(p);
    vao = mgl_fullscreen_quad(&vbo);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    px = mgl_read_rgba8(t);
    if (px)
        mgl_pixel_at(px, t, W / 2, H / 2, out);
    free(px);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glUseProgram(0);
    glDeleteProgram(p);
}

GPU_TEST(es_context, reports_es_3_1)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    GLint v = -1;

    CHECK(c != NULL);
    if (!c)
        return;

    CHECK(!strncmp((const char *)glGetString(GL_VERSION), "OpenGL ES 3.1 ", 14));
    CHECK(!strcmp((const char *)glGetString(GL_SHADING_LANGUAGE_VERSION), "OpenGL ES GLSL ES 3.10"));

    glGetIntegerv(GL_MAJOR_VERSION, &v);
    CHECK_EQ_INT(v, 3);
    glGetIntegerv(GL_MINOR_VERSION, &v);
    CHECK_EQ_INT(v, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // desktop only
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
    glEnable(GL_PROGRAM_POINT_SIZE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
    CHECK_EQ_UINT(glCreateShader(GL_GEOMETRY_SHADER), 0u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glGetIntegerv(GL_NUM_EXTENSIONS, &v);
    CHECK(v > 0);
    for (GLint i = 0; i < v; i++)
        CHECK(!strncmp((const char *)glGetStringi(GL_EXTENSIONS, (GLuint)i), "GL_", 3));
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    closeES(c, &t);
}

GPU_TEST(es_context, draws_with_glsl_es_3_00_and_1_00)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    unsigned char px[4];

    CHECK(c != NULL);
    if (!c)
        return;

    drawn(&t,
          "#version 300 es\n"
          "layout(location = 0) in vec2 pos;\n"
          "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n",
          "#version 300 es\n"
          "precision mediump float;\n"
          "out vec4 colour;\n"
          "void main() { colour = vec4(1.0, 0.0, 0.0, 1.0); }\n", px);
    CHECK(px[0] > 250 && px[1] < 5 && px[2] < 5);

    drawn(&t,
          "attribute vec2 pos;\n"
          "varying vec2 uv;\n"
          "void main() { uv = pos; gl_Position = vec4(pos, 0.0, 1.0); }\n",
          "#version 100\n"
          "precision mediump float;\n"
          "varying vec2 uv;\n"
          "void main() { gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0) + vec4(uv * 0.0, 0.0, 0.0); }\n", px);
    CHECK(px[0] < 5 && px[1] > 250 && px[2] < 5);

    closeES(c, &t);
}

GPU_TEST(es_context, holds_shaders_to_the_es_rules)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    char log[1024];

    CHECK(c != NULL);
    if (!c)
        return;

    // desktop GLSL, and an ES version past 3.10
    CHECK(!compiles(GL_VERTEX_SHADER, "#version 330 core\nvoid main() { gl_Position = vec4(0.0); }\n",
                    log, sizeof log));
    CHECK(strlen(log) > 0);
    CHECK(!compiles(GL_VERTEX_SHADER, "#version 320 es\nvoid main() { gl_Position = vec4(0.0); }\n",
                    log, sizeof log));

    // a fragment shader needs a float precision in ES; desktop GLSL does not ask
    CHECK(!compiles(GL_FRAGMENT_SHADER,
                    "#version 300 es\nout vec4 c;\nvoid main() { c = vec4(1.0); }\n", log, sizeof log));
    CHECK(compiles(GL_FRAGMENT_SHADER,
                   "#version 310 es\nprecision highp float;\nout vec4 c;\nvoid main() { c = vec4(1.0); }\n",
                   log, sizeof log));

    // glGetShaderSource hands back what was given, not what was compiled
    {
        const char *src = "#version 300 es\nvoid main() { gl_Position = vec4(0.0); }\n";
        char back[256] = { 0 };
        GLuint s = glCreateShader(GL_VERTEX_SHADER);

        glShaderSource(s, 1, &src, NULL);
        glCompileShader(s);
        glGetShaderSource(s, sizeof back, NULL, back);
        CHECK(!strcmp(back, src));
        glDeleteShader(s);
    }

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    closeES(c, &t);
}

GPU_TEST(es_context, copies_into_luminance_and_alpha)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    GLuint src, fbo, dst[3];
    GLenum formats[3] = { GL_LUMINANCE, GL_ALPHA, GL_LUMINANCE_ALPHA };
    GLubyte texel[16 * 4];
    unsigned char px[4];
    // what (50, 100, 150, 200) samples as from each
    const int want[3][4] = { { 50, 50, 50, 255 }, { 0, 0, 0, 200 }, { 50, 50, 50, 200 } };

    CHECK(c != NULL);
    if (!c)
        return;

    for (int i = 0; i < 16; i++)
    {
        texel[i * 4 + 0] = 50;
        texel[i * 4 + 1] = 100;
        texel[i * 4 + 2] = 150;
        texel[i * 4 + 3] = 200;
    }

    glGenTextures(1, &src);
    glBindTexture(GL_TEXTURE_2D, src);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, src, 0);

    glGenTextures(3, dst);
    for (int i = 0; i < 3; i++)
    {
        glBindTexture(GL_TEXTURE_2D, dst[i]);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, formats[i], 0, 0, 4, 4, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    mgl_target_bind(&t);
    for (int i = 0; i < 3; i++)
    {
        glBindTexture(GL_TEXTURE_2D, dst[i]);
        drawn(&t,
              "#version 300 es\nlayout(location = 0) in vec2 pos;\n"
              "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n",
              "#version 300 es\nprecision mediump float;\nuniform sampler2D s;\nout vec4 c;\n"
              "void main() { c = texture(s, vec2(0.5)); }\n", px);
        CHECK_MSG(abs(px[0] - want[i][0]) <= 2 && abs(px[1] - want[i][1]) <= 2 && abs(px[2] - want[i][2]) <= 2 &&
                  abs(px[3] - want[i][3]) <= 2, "0x%x samples %d %d %d %d", formats[i], px[0], px[1], px[2], px[3]);
    }

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &src);
    glDeleteTextures(3, dst);
    closeES(c, &t);
}

GPU_TEST(es_context, copies_luminance_into_a_cube_face)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    GLuint src, fbo, cube;
    GLubyte texel[16 * 4], zero[16] = { 0 };
    unsigned char px[4];

    CHECK(c != NULL);
    if (!c)
        return;

    for (int i = 0; i < 16 * 4; i++)
        texel[i] = 90;

    glGenTextures(1, &src);
    glBindTexture(GL_TEXTURE_2D, src);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, src, 0);

    // the copy into +X, and the rest of the faces given after it
    glGenTextures(1, &cube);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    for (GLenum f = GL_TEXTURE_CUBE_MAP_POSITIVE_X; f <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; f++)
    {
        if (f == GL_TEXTURE_CUBE_MAP_POSITIVE_X)
            glCopyTexImage2D(f, 0, GL_LUMINANCE, 0, 0, 4, 4, 0);
        else
            glTexImage2D(f, 0, GL_LUMINANCE, 4, 4, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, zero);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    mgl_target_bind(&t);
    drawn(&t,
          "#version 300 es\nlayout(location = 0) in vec2 pos;\n"
          "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n",
          "#version 300 es\nprecision mediump float;\nuniform samplerCube s;\nout vec4 c;\n"
          "void main() { c = texture(s, vec3(1.0, 0.0, 0.0)); }\n", px);
    CHECK_MSG(abs(px[0] - 90) <= 2 && abs(px[1] - 90) <= 2 && px[3] == 255, "+X samples %d %d %d %d",
              px[0], px[1], px[2], px[3]);

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &src);
    glDeleteTextures(1, &cube);
    closeES(c, &t);
}

GPU_TEST(es_context, blit_framebuffer_errors)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    GLuint fbo, color, depth;

    CHECK(c != NULL);
    if (!c)
        return;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenRenderbuffers(1, &color);
    glBindRenderbuffer(GL_RENDERBUFFER, color);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 32, 32);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 32, 32);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth);
    CHECK_EQ_UINT(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    mgl_target_bind(&t);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    mgl_drain_errors();

    glBlitFramebuffer(0, 0, 32, 32, 0, 0, 32, 32, 0, GL_NEAREST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glBlitFramebuffer(0, 0, 32, 32, 0, 0, 32, 32, GL_COLOR_BUFFER_BIT | GL_MAP_INVALIDATE_BUFFER_BIT, GL_NEAREST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glBlitFramebuffer(0, 0, 32, 32, 0, 0, 32, 32, GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_LINEAR);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // ES has no blit from a buffer onto itself
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBlitFramebuffer(0, 0, 32, 32, 0, 0, 32, 32, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    mgl_target_bind(&t);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBlitFramebuffer(0, 0, 32, 32, 0, 0, 32, 32, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &color);
    glDeleteRenderbuffers(1, &depth);
    closeES(c, &t);
}

GPU_TEST(es_context, lists_multisample_sampler_uniforms)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    const char *vs =
        "#version 310 es\n"
        "uniform highp sampler2DMS vs_ms;\n"
        "uniform highp usampler2DMS vs_ms_uint;\n"
        "void main() { gl_Position = texelFetch(vs_ms, ivec2(0), 0) + vec4(texelFetch(vs_ms_uint, ivec2(0), 0)); }\n";
    const char *fs =
        "#version 310 es\n"
        "precision highp float;\n"
        "uniform highp isampler2DMS fs_ms_int;\n"
        "out vec4 result;\n"
        "void main() { result = vec4(texelFetch(fs_ms_int, ivec2(0), 0)); }\n";
    char log[1024] = { 0 }, name[64];
    GLint count = 0, longest = 0, size;
    GLenum type;
    GLuint p;

    CHECK(c != NULL);
    if (!c)
        return;

    p = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(p != 0, "%s", log);

    glGetProgramiv(p, GL_ACTIVE_UNIFORMS, &count);
    glGetProgramiv(p, GL_ACTIVE_UNIFORM_MAX_LENGTH, &longest);
    CHECK_EQ_INT(count, 3);
    CHECK_EQ_INT(longest, (GLint)sizeof "vs_ms_uint");

    for (GLint i = 0; i < count; i++)
    {
        name[0] = 0;
        glGetActiveUniform(p, (GLuint)i, sizeof name, NULL, &size, &type, name);
        CHECK_MSG(!strcmp(name, "vs_ms") || !strcmp(name, "vs_ms_uint") || !strcmp(name, "fs_ms_int"), "'%s'", name);
    }

    glDeleteProgram(p);
    closeES(c, &t);
}

static GLint pipelineValid(const char *vs, const char *fs)
{
    GLuint a = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &vs);
    GLuint b = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &fs);
    GLuint pipe;
    GLint ok = -1;

    glGenProgramPipelines(1, &pipe);
    glUseProgramStages(pipe, GL_VERTEX_SHADER_BIT, a);
    glUseProgramStages(pipe, GL_FRAGMENT_SHADER_BIT, b);
    glValidateProgramPipeline(pipe);
    glGetProgramPipelineiv(pipe, GL_VALIDATE_STATUS, &ok);
    glDeleteProgramPipelines(1, &pipe);
    glDeleteProgram(a);
    glDeleteProgram(b);
    return ok;
}

GPU_TEST(es_context, pipeline_stages_must_match)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    const char *vs =
        "#version 310 es\n"
        "in highp vec4 a_position;\n"
        "out vec4 val1;\n"
        "out float val0;\n"
        "void main() { gl_Position = a_position; val1 = a_position; val0 = 1.0; }\n";
    const char *fs_both =
        "#version 310 es\n"
        "precision highp float;\n"
        "in vec4 val1;\n"
        "in float val0;\n"
        "layout(location = 0) out vec4 o;\n"
        "void main() { o = val1 * val0; }\n";
    const char *fs_one =
        "#version 310 es\n"
        "precision highp float;\n"
        "in float val0;\n"
        "layout(location = 0) out vec4 o;\n"
        "void main() { o = vec4(val0); }\n";
    const char *fs_medium =
        "#version 310 es\n"
        "precision mediump float;\n"
        "in vec4 val1;\n"
        "in float val0;\n"
        "layout(location = 0) out vec4 o;\n"
        "void main() { o = val1 * val0; }\n";

    CHECK(c != NULL);
    if (!c)
        return;

    CHECK_EQ_INT(pipelineValid(vs, fs_both), GL_TRUE);
    // unused on both sides still match
    CHECK_EQ_INT(pipelineValid(
        "#version 310 es\n"
        "in highp vec4 a_position;\n"
        "layout(location = 3) out vec4 o_val5;\n"
        "flat out uvec4 val4;\n"
        "out vec3 val2[2];\n"
        "void main() { gl_Position = a_position; o_val5 = a_position; }\n",
        "#version 310 es\n"
        "precision highp float;\n"
        "precision highp int;\n"
        "layout(location = 3) in vec4 i_val5;\n"
        "flat in uvec4 val4;\n"
        "in vec3 val2[2];\n"
        "layout(location = 0) out mediump vec4 o;\n"
        "void main() { o = i_val5; }\n"), GL_TRUE);
    CHECK_EQ_INT(pipelineValid(vs, fs_one), GL_FALSE);
    CHECK_EQ_INT(pipelineValid(vs, fs_medium), GL_FALSE);
    // SPIR-V cannot tell these two apart
    CHECK_EQ_INT(pipelineValid(
        "#version 310 es\n"
        "in highp vec4 a_position;\n"
        "precision mediump float;\n"
        "out float var;\n"
        "void main() { gl_Position = a_position; var = a_position.x; }\n",
        "#version 310 es\n"
        "precision lowp float;\n"
        "in float var;\n"
        "layout(location = 0) out mediump vec4 o;\n"
        "void main() { o = vec4(var); }\n"), GL_FALSE);
    CHECK_EQ_INT(pipelineValid(
        "#version 310 es\n"
        "in highp vec4 a_position;\n"
        "out highp vec4 v;\n"
        "void main() { gl_Position = a_position; v = a_position; }\n",
        "#version 310 es\n"
        "flat in highp vec4 v;\n"
        "layout(location = 0) out mediump vec4 o;\n"
        "void main() { o = v; }\n"), GL_FALSE);
    CHECK_EQ_INT(pipelineValid(
        "#version 310 es\n"
        "in highp vec4 a_position;\n"
        "out mediump float v;\n"
        "void main() { gl_Position = a_position; v = 1.0; }\n",
        "#version 310 es\n"
        "layout(location = 3) in mediump float v;\n"
        "layout(location = 0) out mediump vec4 o;\n"
        "void main() { o = vec4(v); }\n"), GL_FALSE);
    closeES(c, &t);
}

// GL's flat values come from a triangle's last vertex, with a pipeline too
GPU_TEST(es_context, pipeline_flat_values_come_from_the_last_vertex)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    const char *vs =
        "#version 310 es\n"
        "flat out highp float v;\n"
        "void main() {\n"
        "    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2)) * 2.0 - 1.0;\n"
        "    gl_Position = vec4(p, 0.0, 1.0);\n"
        "    v = gl_VertexID == 2 ? 1.0 : 0.0;\n"
        "}\n";
    const char *fs =
        "#version 310 es\n"
        "flat in highp float v;\n"
        "layout(location = 0) out mediump vec4 o;\n"
        "void main() { o = vec4(v, v, v, 1.0); }\n";
    GLuint a, b, pipe, vao;
    unsigned char out[4] = { 0 }, *px;

    CHECK(c != NULL);
    if (!c)
        return;

    a = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &vs);
    b = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &fs);
    glGenProgramPipelines(1, &pipe);
    glUseProgramStages(pipe, GL_VERTEX_SHADER_BIT, a);
    glUseProgramStages(pipe, GL_FRAGMENT_SHADER_BIT, b);
    glBindProgramPipeline(pipe);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (px)
        mgl_pixel_at(px, &t, W / 2, H / 2, out);
    free(px);
    CHECK_MSG(out[0] == 255, "drew %d, want 255", out[0]);

    glBindProgramPipeline(0);
    glDeleteProgramPipelines(1, &pipe);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(a);
    glDeleteProgram(b);
    closeES(c, &t);
}

// a struct variable named like its own sampler member
GPU_TEST(es_context, sampler_in_a_struct_of_the_same_name_is_a_resource)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    const char *vs =
        "#version 310 es\n"
        "struct S { highp sampler2D target; };\n"
        "uniform S target;\n"
        "void main() { gl_Position = vec4(float(textureSize(target.target, 0).x)); }\n";
    const char *fs =
        "#version 310 es\n"
        "layout(location = 0) out mediump vec4 o;\n"
        "void main() { o = vec4(1.0); }\n";
    char log[1024] = { 0 };
    GLuint p;

    CHECK(c != NULL);
    if (!c)
        return;

    p = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(p != 0, "%s", log);
    CHECK(glGetProgramResourceIndex(p, GL_UNIFORM, "target.target") != GL_INVALID_INDEX);
    CHECK(glGetUniformLocation(p, "target.target") >= 0);

    glDeleteProgram(p);
    closeES(c, &t);
}

// sets a struct's bool and bvec4 through the float or the int calls, draws,
// and returns the centre pixel's green: 255 when the shader saw them
static int structBoolsSeen(MGLTestTarget *t, int by_float)
{
    const char *vs =
        "#version 310 es\n"
        "in highp vec4 a_position;\n"
        "out mediump float v;\n"
        "struct S { bool m0; bvec4 m1; };\n"
        "uniform S u;\n"
        "void main() {\n"
        "    gl_Position = a_position;\n"
        "    v = (u.m0 && u.m1 == bvec4(true, false, false, true)) ? 1.0 : 0.0;\n"
        "}\n";
    const char *fs =
        "#version 310 es\n"
        "in mediump float v;\n"
        "layout(location = 0) out mediump vec4 o;\n"
        "void main() { o = vec4(0.0, v, 0.0, 1.0); }\n";
    static const GLfloat f[4] = { 9.5f, 0.0f, 0.0f, -7.25f };
    static const GLint i[4] = { 3, 0, 0, -1 };
    char log[1024] = { 0 };
    GLuint p = mgl_build_program(vs, fs, log, sizeof log), vbo = 0, vao;
    unsigned char out[4] = { 0 }, *px;

    CHECK_MSG(p != 0, "%s", log);
    if (!p)
        return -1;

    if (by_float)
    {
        glProgramUniform1fv(p, glGetUniformLocation(p, "u.m0"), 1, f);
        glProgramUniform4fv(p, glGetUniformLocation(p, "u.m1"), 1, f);
    }
    else
    {
        glProgramUniform1iv(p, glGetUniformLocation(p, "u.m0"), 1, i);
        glProgramUniform4iv(p, glGetUniformLocation(p, "u.m1"), 1, i);
    }
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(p);
    vao = mgl_fullscreen_quad(&vbo);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    px = mgl_read_rgba8(t);
    if (px)
        mgl_pixel_at(px, t, W / 2, H / 2, out);
    free(px);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glUseProgram(0);
    glDeleteProgram(p);
    return out[1];
}

GPU_TEST(es_context, struct_bools_take_float_and_int_values)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);

    CHECK(c != NULL);
    if (!c)
        return;

    CHECK_EQ_INT(structBoolsSeen(&t, 0), 255);
    CHECK_EQ_INT(structBoolsSeen(&t, 1), 255);
    closeES(c, &t);
}

static GLint linksES(const char *vs, const char *fs)
{
    char log[1024] = { 0 };
    GLuint p = mgl_build_program(vs, fs, log, sizeof log);

    glDeleteProgram(p);
    return p != 0;
}

// a block two stages share agrees on its layout and on which arrays are sized
GPU_TEST(es_context, shared_blocks_must_match_across_stages)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    const char *fs_tail =
        "layout(location = 0) out mediump vec4 o;\n"
        "void main() { o = vec4(v.x); }\n";
    char vs[512], fs[512];

    CHECK(c != NULL);
    if (!c)
        return;

    snprintf(vs, sizeof vs, "#version 310 es\nlayout(shared) uniform B { highp vec4 v; };\n"
             "void main() { gl_Position = v; }\n");
    snprintf(fs, sizeof fs, "#version 310 es\nlayout(shared) uniform B { highp vec4 v; };\n%s", fs_tail);
    CHECK(linksES(vs, fs));
    snprintf(fs, sizeof fs, "#version 310 es\nlayout(packed) uniform B { highp vec4 v; };\n%s", fs_tail);
    CHECK(!linksES(vs, fs));

    snprintf(vs, sizeof vs, "#version 310 es\nlayout(binding = 0) buffer S { highp float v[]; };\n"
             "void main() { gl_Position = vec4(v[0]); }\n");
    snprintf(fs, sizeof fs, "#version 310 es\nlayout(binding = 0) buffer S { highp float v[1]; };\n%s", fs_tail);
    CHECK(!linksES(vs, fs));
    closeES(c, &t);
}

// a long default layout naming shared and packed many times compiles
GPU_TEST(es_context, long_layout_lists_compile)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    char log[1024] = { 0 };
    const char *vs =
        "#version 310 es\n"
        "layout(shared, packed, std140, shared, packed, std140, shared, packed, std140, "
        "shared, packed, std140, shared, packed, std140) uniform;\n"
        "uniform U { vec4 m; } u;\n"
        "void main() { gl_Position = u.m; }\n";

    CHECK(c != NULL);
    if (!c)
        return;

    CHECK_MSG(compiles(GL_VERTEX_SHADER, vs, log, sizeof log), "%s", log);
    closeES(c, &t);
}

// a desktop capability is no query in ES
GPU_TEST(es_context, desktop_capabilities_are_not_queries)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    GLint v = 0;

    CHECK(c != NULL);
    if (!c)
        return;

    glGetIntegerv(GL_FRAMEBUFFER_SRGB, &v);
    CHECK_EQ_UINT(glGetError(), GL_INVALID_ENUM);
    glGetIntegerv(GL_BLEND, &v);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);
    closeES(c, &t);
}

GPU_TEST(es_context, separable_varyings_without_a_location_sit_at_minus_one)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    const char *vs =
        "#version 310 es\n"
        "out highp vec4 a;\n"
        "layout(location = 2) out highp vec4 b;\n"
        "void main() { gl_Position = vec4(0.0); a = vec4(1.0); b = vec4(2.0); }\n";
    const GLenum prop = GL_LOCATION;
    GLint loc = 0;
    GLuint p;

    CHECK(c != NULL);
    if (!c)
        return;

    p = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &vs);
    glGetProgramResourceiv(p, GL_PROGRAM_OUTPUT, glGetProgramResourceIndex(p, GL_PROGRAM_OUTPUT, "a"), 1, &prop, 1,
                           NULL, &loc);
    CHECK_EQ_INT(loc, -1);
    CHECK_EQ_INT(glGetProgramResourceLocation(p, GL_PROGRAM_OUTPUT, "a"), -1);
    CHECK_EQ_INT(glGetProgramResourceLocation(p, GL_PROGRAM_OUTPUT, "b"), 2);

    glDeleteProgram(p);
    closeES(c, &t);
}

#define EXPECT_ERROR(call, want) do { mgl_drain_errors(); call; CHECK_EQ_UINT(glGetError(), (GLenum)(want)); } while (0)

// the errors dEQP's negative coverage checks one call at a time
GPU_TEST(es_context, calls_raise_the_errors_es_names)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    GLuint tex, prog, fbo, pbo, vao;
    static const GLubyte idx[1] = { 0 };

    CHECK(c != NULL);
    if (!c)
        return;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    EXPECT_ERROR(glVertexAttribPointer(0, 0, GL_BYTE, GL_TRUE, 0, 0), GL_INVALID_VALUE);
    EXPECT_ERROR(glVertexAttribPointer(0, 2, GL_INT_2_10_10_10_REV, GL_TRUE, 0, 0), GL_INVALID_OPERATION);
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    EXPECT_ERROR(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 16385, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0), GL_INVALID_VALUE);
    EXPECT_ERROR(glTexImage2D(GL_TEXTURE_2D, 15, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0), GL_INVALID_VALUE);
    EXPECT_ERROR(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, 0),
                 GL_INVALID_VALUE);
    EXPECT_ERROR(glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, 0, 0), GL_NO_ERROR);
    EXPECT_ERROR(glGenerateMipmap(GL_TEXTURE_2D), GL_INVALID_OPERATION);
    EXPECT_ERROR(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB9_E5, 16, 16, 0, GL_RGB, GL_FLOAT, 0), GL_NO_ERROR);
    EXPECT_ERROR(glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 4, 4), GL_INVALID_OPERATION);

    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, 256, NULL, GL_STATIC_DRAW);
    EXPECT_ERROR(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 4, 4, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1,
                              (const void *)3), GL_INVALID_OPERATION);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glDeleteBuffers(1, &pbo);

    glBindTexture(GL_TEXTURE_2D, 0);
    EXPECT_ERROR(glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16), GL_INVALID_OPERATION);
    glDeleteTextures(1, &tex);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_3D, tex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    EXPECT_ERROR(glTexSubImage3D(GL_TEXTURE_3D, 12, 0, 0, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0),
                 GL_INVALID_VALUE);
    glBindTexture(GL_TEXTURE_3D, 0);
    glDeleteTextures(1, &tex);

    prog = glCreateProgram();
    EXPECT_ERROR(glShaderSource(prog, 0, NULL, NULL), GL_INVALID_OPERATION);
    glDeleteProgram(prog);
    EXPECT_ERROR(glGetAttribLocation(prog, "x"), GL_INVALID_VALUE);
    EXPECT_ERROR(glDispatchComputeIndirect(-1), GL_INVALID_VALUE);

    // with no program a draw draws nothing, but still checks what it can
    glUseProgram(0);
    EXPECT_ERROR(glDrawElements(GL_POINTS, 1, 0xFFFFFFFFu, idx), GL_INVALID_ENUM);
    EXPECT_ERROR(glDrawArraysInstanced(GL_POINTS, 0, 1, -1), GL_INVALID_VALUE);
    EXPECT_ERROR(glDrawRangeElements(GL_POINTS, 1, 0, 1, GL_UNSIGNED_BYTE, idx), GL_INVALID_VALUE);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    EXPECT_ERROR(glDrawArrays(GL_POINTS, 0, 1), GL_INVALID_FRAMEBUFFER_OPERATION);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);

    closeES(c, &t);
}

// ES framebuffer rules: a new renderbuffer is GL_RGBA4, an attachment with
// no storage is linear, and a blit onto the same image is an error
GPU_TEST(es_context, framebuffer_queries_and_blits_follow_es)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    GLuint rb, tex, read, draw;
    GLint v = 0;

    CHECK(c != NULL);
    if (!c)
        return;

    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &v);
    CHECK_EQ_INT(v, GL_RGBA4);

    glGenFramebuffers(1, &read);
    glBindFramebuffer(GL_FRAMEBUFFER, read);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &v);
    CHECK_EQ_INT(v, GL_LINEAR);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
    glDeleteRenderbuffers(1, &rb);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glGenFramebuffers(1, &draw);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
    mgl_drain_errors();
    glBlitFramebuffer(0, 0, 8, 8, 0, 0, 8, 8, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    CHECK_EQ_UINT(glGetError(), GL_INVALID_OPERATION);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &read);
    glDeleteFramebuffers(1, &draw);
    glDeleteTextures(1, &tex);
    closeES(c, &t);
}

// a byte array that starts at an odd offset, drawn from a later vertex
GPU_TEST(es_context, byte_array_at_an_odd_offset)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    const char *vs =
        "#version 300 es\n"
        "layout(location = 0) in vec2 pos;\n"
        "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";
    const char *fs =
        "#version 300 es\n"
        "layout(location = 0) out mediump vec4 o;\n"
        "void main() { o = vec4(1.0, 0.0, 0.0, 1.0); }\n";
    static const GLbyte corners[6][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, -1 }, { 1, 1 }, { -1, 1 } };
    GLbyte data[30 * 32 + 32];
    char log[1024] = { 0 };
    GLuint p;
    unsigned char out[4] = { 0 }, *px;

    CHECK(c != NULL);
    if (!c)
        return;

    memset(data, 0, sizeof data);
    // the quad sits at vertices 24 to 29
    for (int i = 0; i < 6; i++)
    {
        data[17 + (24 + i) * 32] = corners[i][0];
        data[18 + (24 + i) * 32] = corners[i][1];
    }

    p = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(p != 0, "%s", log);
    glUseProgram(p);

    GLuint vbo, vao;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof data, data, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_BYTE, GL_FALSE, 32, (const void *)17);
    glEnableVertexAttribArray(0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 24, 6);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (px)
        mgl_pixel_at(px, &t, W / 2, H / 2, out);
    free(px);
    CHECK_MSG(out[0] == 255, "drew %d, want 255", out[0]);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glUseProgram(0);
    glDeleteProgram(p);
    closeES(c, &t);
}

// transform feedback records vertices in the order given, flat outputs or not
GPU_TEST(es_context, capture_keeps_vertex_order_with_flat_outputs)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    const char *vs =
        "#version 300 es\n"
        "flat out highp float v;\n"
        "void main() { v = float(gl_VertexID); gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";
    const char *fs =
        "#version 300 es\n"
        "flat in highp float v;\n"
        "layout(location = 0) out mediump vec4 o;\n"
        "void main() { o = vec4(v); }\n";
    const char *names[1] = { "v" };
    GLuint p, vs_s, fs_s, buf, vao;
    GLint ok = 0;
    GLfloat got[3] = { -1, -1, -1 };

    CHECK(c != NULL);
    if (!c)
        return;

    p = glCreateProgram();
    vs_s = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs_s, 1, &vs, NULL);
    glCompileShader(vs_s);
    fs_s = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs_s, 1, &fs, NULL);
    glCompileShader(fs_s);
    glAttachShader(p, vs_s);
    glAttachShader(p, fs_s);
    glTransformFeedbackVaryings(p, 1, names, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    CHECK_EQ_INT(ok, GL_TRUE);

    glGenBuffers(1, &buf);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof got, NULL, GL_STREAM_COPY);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(p);
    glBeginTransformFeedback(GL_TRIANGLES);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glEndTransformFeedback();

    const GLfloat *m = glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, GL_MAP_READ_BIT);
    if (m)
        memcpy(got, m, sizeof got);
    glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
    CHECK_MSG(got[0] == 0.0f && got[1] == 1.0f && got[2] == 2.0f, "captured %g %g %g", got[0], got[1], got[2]);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &buf);
    glDeleteShader(vs_s);
    glDeleteShader(fs_s);
    glDeleteProgram(p);
    closeES(c, &t);
}

GPU_TEST(es_context, drawing_with_mismatched_pipeline_stages_fails)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    const char *vs =
        "#version 310 es\n"
        "in highp vec4 a_position;\n"
        "out highp vec4 v;\n"
        "void main() { gl_Position = a_position; v = a_position; }\n";
    const char *fs =
        "#version 310 es\n"
        "in mediump vec4 v;\n"
        "layout(location = 0) out mediump vec4 o;\n"
        "void main() { o = v; }\n";
    GLuint a, b, pipe, vbo = 0, vao;

    CHECK(c != NULL);
    if (!c)
        return;

    a = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &vs);
    b = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &fs);
    glGenProgramPipelines(1, &pipe);
    glUseProgramStages(pipe, GL_VERTEX_SHADER_BIT, a);
    glUseProgramStages(pipe, GL_FRAGMENT_SHADER_BIT, b);
    glBindProgramPipeline(pipe);
    vao = mgl_fullscreen_quad(&vbo);
    mgl_drain_errors();
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(glGetError(), GL_INVALID_OPERATION);

    glBindProgramPipeline(0);
    glDeleteProgramPipelines(1, &pipe);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(a);
    glDeleteProgram(b);
    closeES(c, &t);
}

GPU_TEST(es_context, es_3_1_api_rules)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    const char *cs =
        "#version 310 es\n"
        "layout(local_size_x = 1) in;\n"
        "layout(rgba8, binding = 0) readonly uniform highp image2D img;\n"
        "layout(std430, binding = 0) buffer B { ivec2 size; };\n"
        "void main() { size = imageSize(img); }\n";
    static const GLuint cmd[4] = { 3, 1, 0, 0 };
    char log[1024] = { 0 };
    GLuint vbo, ibuf, vao, tex[2], fbo, sh[2];
    GLint v = 0;

    CHECK(c != NULL);
    if (!c)
        return;

    // imageSize is GLSL ES 3.10
    CHECK_MSG(compiles(GL_COMPUTE_SHADER, cs, log, sizeof log), "%s", log);

    // one shader of each type per program
    {
        GLuint p = glCreateProgram();

        sh[0] = glCreateShader(GL_COMPUTE_SHADER);
        sh[1] = glCreateShader(GL_COMPUTE_SHADER);
        glAttachShader(p, sh[0]);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glAttachShader(p, sh[1]);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
        glDeleteShader(sh[0]);
        glDeleteShader(sh[1]);
        glDeleteProgram(p);
    }

    // the format and binding calls need a vertex array object of the application's
    glBindVertexArray(0);
    glVertexAttribBinding(0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glVertexAttribFormat(0, 4, GL_FLOAT, GL_FALSE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // and so does an indirect draw, with buffers behind every enabled array
    glGenBuffers(1, &ibuf);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, ibuf);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof cmd, cmd, GL_STATIC_DRAW);
    glDrawArraysIndirect(GL_TRIANGLES, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 64, NULL, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, cmd);
    glEnableVertexAttribArray(1);
    glDrawArraysIndirect(GL_TRIANGLES, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glDisableVertexAttribArray(1);

    // image units start as GL_R32UI, and take only immutable textures
    glGetIntegeri_v(GL_IMAGE_BINDING_FORMAT, 0, &v);
    CHECK_EQ_INT(v, GL_R32UI);

    glGenTextures(2, tex);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    mgl_drain_errors();
    glBindImageTexture(0, tex[0], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // a compressed update in a format that is not compressed
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGB, 8, cmd);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // multisample storage needs a renderable format, and blits cannot go into it
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex[1]);
    glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGB8_SNORM, 4, 4, GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
    glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, 4, 4, GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, tex[1], 0);
    mgl_target_bind(&t);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 4, 4, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // no layers for a framebuffer with nothing attached
    glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_LAYERS, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(2, tex);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ibuf);
    closeES(c, &t);
}

GPU_TEST(es_context, es_3_0_api_rules)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    GLuint fbo[2], rbo[2], q[2], empty;
    GLint v[4];
    GLubyte px[4];

    CHECK(c != NULL);
    if (!c)
        return;

    CHECK(strstr((const char *)glGetString(GL_EXTENSIONS), "GL_KHR_texture_compression_astc_ldr") != NULL);

    // the window's buffer is GL_BACK, and has no object name
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glReadBuffer(GL_FRONT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // sample counts are for renderable formats on multisample targets
    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, 1, v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
    glGetInternalformativ(GL_RENDERBUFFER, GL_RG8_SNORM, GL_NUM_SAMPLE_COUNTS, 1, v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // ES binds a buffer name nobody generated
    glBindBuffer(GL_ARRAY_BUFFER, 5000000);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // an empty framebuffer cannot be read; an unknown texture cannot be attached
    glGenFramebuffers(1, &empty);
    glBindFramebuffer(GL_FRAMEBUFFER, empty);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_FRAMEBUFFER_OPERATION);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 5000000, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // the two any-samples queries share one slot
    glGenQueries(2, q);
    glBeginQuery(GL_ANY_SAMPLES_PASSED, q[0]);
    glBeginQuery(GL_ANY_SAMPLES_PASSED_CONSERVATIVE, q[1]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glEndQuery(GL_ANY_SAMPLES_PASSED);
    glDeleteQueries(2, q);

    // a blit out of a multisampled framebuffer keeps its rectangle
    glGenRenderbuffers(2, rbo);
    glGenFramebuffers(2, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo[0]);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, 8, 8);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo[0]);
    glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo[0]);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo[1]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 8, 8);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo[1]);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo[1]);
    glBlitFramebuffer(0, 0, 4, 4, 1, 1, 5, 5, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(2, fbo);
    glDeleteFramebuffers(1, &empty);
    glDeleteRenderbuffers(2, rbo);
    closeES(c, &t);
}

GPU_TEST(es_context, restart_index_with_client_arrays)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    static const GLfloat pos[] = { -1, -1,  1, -1,  -1, 1,  1, 1 };
    static const GLuint idx[] = { 0, 1, 2, 0xFFFFFFFFu, 1, 3, 2 };
    const char *vs = "#version 300 es\nin vec2 p;\nvoid main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    const char *fs = "#version 300 es\nout mediump vec4 o;\nvoid main() { o = vec4(0.0, 1.0, 0.0, 1.0); }\n";
    char log[512] = "";
    GLuint prog;
    unsigned char *px, p[4];

    CHECK(c != NULL);
    if (!c)
        return;

    prog = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "%s", log);

    glUseProgram(prog);
    glBindVertexArray(0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos);
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    glDrawElements(GL_TRIANGLES, 7, GL_UNSIGNED_INT, idx);
    glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    mgl_pixel_at(px, &t, 2, 2, p);
    CHECK_MSG(p[1] == 255, "drew %d %d %d", p[0], p[1], p[2]);
    free(px);

    glDisableVertexAttribArray(0);
    glUseProgram(0);
    glDeleteProgram(prog);
    closeES(c, &t);
}

GPU_TEST(es_context, flat_inputs_and_point_restarts)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    static const GLfloat pos[] = { -1, -1,  1, -1,  -1, 1,  0, 0 };
    static const GLfloat col[] = { 1, 0, 0,  1, 0, 0,  0, 0, 1,  0, 1, 0 };
    static const GLushort idx[] = { 0xFFFF };
    const char *vs = "#version 300 es\nlayout(location = 0) in vec2 p;\nlayout(location = 1) in vec3 c;\n"
                     "flat out vec3 v;\n"
                     "void main() { v = c; gl_Position = vec4(p, 0.0, 1.0); gl_PointSize = 1.0; }\n";
    const char *fs = "#version 300 es\nflat in mediump vec3 v;\nout mediump vec4 o;\nvoid main() { o = vec4(v, 1.0); }\n";
    char log[512] = "";
    GLuint prog, tex, fbo;
    unsigned char *px, p[4];

    CHECK(c != NULL);
    if (!c)
        return;

    prog = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "%s", log);

    glUseProgram(prog);
    glBindVertexArray(0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, col);

    // a flat input takes the triangle's last vertex
    glDrawArrays(GL_TRIANGLES, 0, 3);
    px = mgl_read_rgba8(&t);
    mgl_pixel_at(px, &t, 2, 2, p);
    CHECK_MSG(p[2] == 255 && p[0] == 0, "drew %d %d %d", p[0], p[1], p[2]);
    free(px);

    // a restart at the end of a point list draws nothing
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    glDrawElements(GL_POINTS, 1, GL_UNSIGNED_SHORT, idx);
    glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    px = mgl_read_rgba8(&t);
    mgl_pixel_at(px, &t, W / 2, H / 2, p);
    CHECK_MSG(p[0] == 0 && p[1] == 0 && p[2] == 0, "centre %d %d %d", p[0], p[1], p[2]);
    free(px);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glUseProgram(0);
    glDeleteProgram(prog);

    // an unsized depth format is not depth renderable
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 4, 4, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    closeES(c, &t);
}

GPU_TEST(es_context, flat_has_to_match_across_stages)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    const char *vs = "#version 300 es\nflat out mediump float v;\nvoid main() { v = 1.0; gl_Position = vec4(0.0); }\n";
    const char *fs = "#version 300 es\nin mediump float v;\nout mediump vec4 o;\nvoid main() { o = vec4(v); }\n";
    char log[512] = "";

    CHECK(c != NULL);
    if (!c)
        return;

    CHECK(mgl_build_program(vs, fs, log, sizeof log) == 0);
    closeES(c, &t);
}

GPU_TEST(es_context, glsl_100_attributes_may_alias)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    const char *vs = "attribute float a; attribute vec4 b; uniform bool u;\n"
                     "void main() { gl_Position = u ? vec4(a) : b; }\n";
    const char *fs = "void main() { gl_FragColor = vec4(1.0); }\n";
    GLuint sh[2], prog;
    GLint ok = 0;

    CHECK(c != NULL);
    if (!c)
        return;

    sh[0] = glCreateShader(GL_VERTEX_SHADER);
    sh[1] = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(sh[0], 1, &vs, NULL);
    glShaderSource(sh[1], 1, &fs, NULL);
    glCompileShader(sh[0]);
    glCompileShader(sh[1]);
    prog = glCreateProgram();
    glAttachShader(prog, sh[0]);
    glAttachShader(prog, sh[1]);
    glBindAttribLocation(prog, 1, "a");
    glBindAttribLocation(prog, 1, "b");
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    CHECK(ok);
    CHECK_EQ_INT(glGetAttribLocation(prog, "a"), 1);
    CHECK_EQ_INT(glGetAttribLocation(prog, "b"), 1);

    glDeleteShader(sh[0]);
    glDeleteShader(sh[1]);
    glDeleteProgram(prog);
    closeES(c, &t);
}

GPU_TEST(es_context, short_circuits_and_recursion)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    const char *vs =
        "#version 300 es\n"
        "out float v_g;\n"
        "int g;\n"
        "int[2] plus() { ++g; return int[2](g, g); }\n"
        "bool minus() { --g; return false; }\n"
        "void main() {\n"
        "  g = 0;\n"
        "  int a[2] = int[2](0, 0);\n"
        "  minus() && (a == plus());\n"
        "  gl_Position = vec4(0.0);\n"
        "  v_g = float(g);\n"
        "}\n";
    const char *rec =
        "#version 300 es\n"
        "float rec(float x) { return x <= 0.0 ? 0.0 : rec(x - 1.0); }\n"
        "void main() { gl_Position = vec4(rec(5.0)); }\n";
    const char *fs = "#version 300 es\nin mediump float v_g;\nout mediump vec4 o;\nvoid main() { o = vec4(v_g); }\n";
    const char *fs0 = "#version 300 es\nout mediump vec4 o;\nvoid main() { o = vec4(1.0); }\n";
    char log[2048] = "";
    GLuint prog;

    CHECK(c != NULL);
    if (!c)
        return;

    prog = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "%s", log);
    if (prog)
        glDeleteProgram(prog);

    // GLSL ES has no recursion
    prog = mgl_build_program(rec, fs0, log, sizeof log);
    CHECK(prog == 0);
    if (prog)
        glDeleteProgram(prog);

    closeES(c, &t);
}

GPU_TEST(es_context, alpha_textures_sample_their_alpha)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    static const GLubyte a[4] = { 200, 200, 200, 200 };
    const char *vs = "#version 300 es\nlayout(location = 0) in vec2 p;\nvoid main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    const char *fs = "#version 300 es\nuniform mediump sampler2D s;\nout mediump vec4 o;\n"
                     "void main() { o = texture(s, vec2(0.5)) + vec4(0.0, 0.0, 0.0, 0.0); }\n";
    GLuint tex;
    unsigned char p[4];

    CHECK(c != NULL);
    if (!c)
        return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 2, 2, 0, GL_ALPHA, GL_UNSIGNED_BYTE, a);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    drawn(&t, vs, fs, p);
    CHECK_MSG(p[0] == 0 && abs(p[3] - 200) <= 2, "sampled %d %d %d %d", p[0], p[1], p[2], p[3]);

    glDeleteTextures(1, &tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    closeES(c, &t);
}

extern void glDebugMessageInsertKHR(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar *);
extern GLuint glGetDebugMessageLogKHR(GLuint, GLsizei, GLenum *, GLenum *, GLuint *, GLenum *, GLsizei *, GLchar *);

GPU_TEST(es_context, khr_debug_answers_to_its_khr_names)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    GLuint id = 0;

    CHECK(c != NULL);
    if (!c)
        return;

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageInsertKHR(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 42, GL_DEBUG_SEVERITY_NOTIFICATION, -1, "x");
    CHECK_EQ_UINT(glGetDebugMessageLogKHR(1, 0, NULL, NULL, &id, NULL, NULL, NULL), 1);
    CHECK_EQ_UINT(id, 42);
    glDisable(GL_DEBUG_OUTPUT);
    closeES(c, &t);
}


GPU_TEST(es_context, generated_names_are_not_in_use)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    GLuint fbo[2] = { 0, 0 };

    CHECK(c != NULL);
    if (!c)
        return;

    // ES lets an app bind a name it never generated
    glGenFramebuffers(1, &fbo[0]);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[0] + 1);
    glGenFramebuffers(1, &fbo[1]);
    CHECK_MSG(fbo[1] != fbo[0] + 1, "got %u twice", fbo[1]);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(2, fbo);
    fbo[0]++;
    glDeleteFramebuffers(1, fbo);

    closeES(c, &t);
}

GPU_TEST(es_context, aliased_attributes_share_a_location)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    const char *fs =
        "#version 100\n"
        "varying mediump vec4 v;\n"
        "void main() { gl_FragColor = v; }\n";
    char vs[4096], log[1024] = { 0 }, name[16];
    int n = 0;
    GLint ok = 0, max = 0;
    GLuint p, sh[2], vbo = 0, vao;
    unsigned char out[4] = { 0 }, *px;

    CHECK(c != NULL);
    if (!c)
        return;

    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max);
    n += snprintf(vs + n, sizeof vs - n,
                  "#version 100\nattribute vec2 a_pos;\nattribute vec2 c_pos;\nuniform mediump float u_A;\n"
                  "varying mediump vec4 v;\n");
    for (int i = 1; i < max; i++)
        n += snprintf(vs + n, sizeof vs - n, "attribute vec4 a_%d;\nattribute vec4 b_%d;\n", i, i);
    n += snprintf(vs + n, sizeof vs - n, "void main() {\n v = vec4(0.0);\n");
    for (int i = 1; i < max; i++)
        n += snprintf(vs + n, sizeof vs - n, " if (u_A != 0.0) v += a_%d;\n if (u_A == 0.0) v += b_%d;\n", i, i);
    snprintf(vs + n, sizeof vs - n, " gl_Position = vec4(u_A != 0.0 ? a_pos : c_pos, 0.0, 1.0);\n}\n");

    p = glCreateProgram();
    sh[0] = glCreateShader(GL_VERTEX_SHADER);
    sh[1] = glCreateShader(GL_FRAGMENT_SHADER);
    {
        const char *src = vs;
        glShaderSource(sh[0], 1, &src, NULL);
    }
    glShaderSource(sh[1], 1, &fs, NULL);
    glCompileShader(sh[0]);
    glCompileShader(sh[1]);
    glAttachShader(p, sh[0]);
    glAttachShader(p, sh[1]);
    glBindAttribLocation(p, 0, "a_pos");
    glBindAttribLocation(p, 0, "c_pos");
    for (int i = 1; i < max; i++)
    {
        snprintf(name, sizeof name, "a_%d", i);
        glBindAttribLocation(p, i, name);
        snprintf(name, sizeof name, "b_%d", i);
        glBindAttribLocation(p, i, name);
    }
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    glGetProgramInfoLog(p, sizeof log, NULL, log);
    CHECK_MSG(ok == GL_TRUE, "%s", log);

    CHECK_EQ_INT(glGetAttribLocation(p, "c_pos"), 0);

    if (ok)
    {
        glUseProgram(p);
        glUniform1f(glGetUniformLocation(p, "u_A"), 0.0f);
        for (int i = 1; i < max; i++)
            glVertexAttrib4f(i, i == 1 ? 0.5f : 0.0f, 0.0f, 0.0f, i == 1 ? 1.0f : 0.0f);
        vao = mgl_fullscreen_quad(&vbo);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        px = mgl_read_rgba8(&t);
        if (px)
            mgl_pixel_at(px, &t, W / 2, H / 2, out);
        free(px);
        CHECK_MSG(out[0] >= 126 && out[0] <= 129, "drew %d, want 128", out[0]);
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glUseProgram(0);
    }

    glDeleteShader(sh[0]);
    glDeleteShader(sh[1]);
    glDeleteProgram(p);
    closeES(c, &t);
}

GPU_TEST(es_context, luminance_textures_generate_mipmaps)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    static const GLenum formats[3] = { GL_LUMINANCE, GL_ALPHA, GL_LUMINANCE_ALPHA };
    unsigned char texels[4 * 4 * 2] = { 0 };
    GLuint tex;

    CHECK(c != NULL);
    if (!c)
        return;

    for (int i = 0; i < 3; i++)
    {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, formats[i], 4, 4, 0, formats[i], GL_UNSIGNED_BYTE, texels);
        mgl_drain_errors();
        glGenerateMipmap(GL_TEXTURE_2D);
        CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);
        glDeleteTextures(1, &tex);
    }

    // integer formats still cannot
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8I, 4, 4, 0, GL_RED_INTEGER, GL_BYTE, texels);
    mgl_drain_errors();
    glGenerateMipmap(GL_TEXTURE_2D);
    CHECK_EQ_UINT(glGetError(), GL_INVALID_OPERATION);
    glDeleteTextures(1, &tex);
    closeES(c, &t);
}

GPU_TEST(es_context, integer_attribute_with_a_two_byte_stride)
{
    MGLTestTarget t;
    GLMContext c = openES(&t);
    const char *vs =
        "#version 300 es\n"
        "in highp ivec2 a_0;\n"
        "void main() { gl_Position = vec4(vec2(a_0.xy) / 32768.0, 0.0, 1.0); }\n";
    const char *fs =
        "#version 300 es\n"
        "layout(location = 0) out mediump vec4 o;\n"
        "void main() { o = vec4(1.0); }\n";
    static const GLshort pos[2] = { -30000, 30000 };
    static const GLushort idx[2] = { 0, 1 };
    unsigned char raw[16];
    char log[1024] = { 0 };
    GLuint p;
    unsigned char *px, out[4] = { 0 };

    CHECK(c != NULL);
    if (!c)
        return;

    p = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(p != 0, "%s", log);
    memcpy(raw + 7, idx, sizeof idx);
    // client arrays need the default vertex array object
    glBindVertexArray(0);
    glUseProgram(p);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribIPointer(0, 1, GL_SHORT, 0, pos);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawElementsInstanced(GL_LINE_LOOP, 2, GL_UNSIGNED_SHORT, raw + 7, 1);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (px)
    {
        for (int y = 0; y < H && !out[0]; y++)
            mgl_pixel_at(px, &t, W / 2, y, out);
    }
    free(px);
    CHECK_MSG(out[0] == 255, "no line drawn");

    glDisableVertexAttribArray(0);
    glUseProgram(0);
    glDeleteProgram(p);
    closeES(c, &t);
}

#endif /* MGL_OGL_ES */
