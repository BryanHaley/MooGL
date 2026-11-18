/*
 * test_misc_core.c
 * MGL
 *
 * Remaining uncovered core entry points: glCopyImageSubData, glDispatchCompute,
 * glGetInternalformati64v, glGetObjectLabel, glGetObjectPtrLabel,
 * glGetShaderPrecisionFormat, glObjectPtrLabel, glReleaseShaderCompiler,
 * glTexBuffer, glTexBufferRange.
 */

#include "mgl_test.h"
#include "harness.h"
#include "MGLContext.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ---------- dispatch compute ---------- */

GPU_TEST(misc_core, dispatch_compute_rejects_no_program)
{
    // spec: GL_INVALID_OPERATION if no active program for compute shader stage
    glDispatchCompute(1, 1, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(misc_core, dispatch_compute_rejects_zero_groups)
{
    // zero is not greater than or equal to the max, so it should not be rejected
    // by the value check; the real error is no program.  The spec says
    // GL_INVALID_VALUE only when a count >= MAX_COMPUTE_WORK_GROUP_COUNT.
    glDispatchCompute(0, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(misc_core, dispatch_compute_rejects_huge_groups)
{
    // spec: GL_INVALID_VALUE if any count >= MAX_COMPUTE_WORK_GROUP_COUNT
    glDispatchCompute(0xFFFFFFFFu, 1, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- copy image sub data ---------- */

GPU_TEST(misc_core, copy_image_rejects_invalid_target)
{
    GLuint tex[2] = { 0 };

    glGenTextures(2, tex);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    glBindTexture(GL_TEXTURE_2D, tex[1]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    glBindTexture(GL_TEXTURE_2D, 0);

    // GL_TEXTURE_BUFFER is explicitly excluded
    glCopyImageSubData(tex[0], GL_TEXTURE_BUFFER, 0, 0, 0, 0,
                       tex[1], GL_TEXTURE_2D, 0, 0, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glCopyImageSubData(tex[0], GL_TEXTURE_2D, 0, 0, 0, 0,
                       tex[1], GL_TEXTURE_BUFFER, 0, 0, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteTextures(2, tex);
}

GPU_TEST(misc_core, copy_image_rejects_invalid_name)
{
    GLuint tex = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    glBindTexture(GL_TEXTURE_2D, 0);

    // non-existent source
    glCopyImageSubData(999123, GL_TEXTURE_2D, 0, 0, 0, 0,
                       tex, GL_TEXTURE_2D, 0, 0, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // non-existent destination
    glCopyImageSubData(tex, GL_TEXTURE_2D, 0, 0, 0, 0,
                       999123, GL_TEXTURE_2D, 0, 0, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteTextures(1, &tex);
}

GPU_TEST(misc_core, copy_image_round_trips_pixels)
{
    // create two identical textures, write a distinctive pixel into the source,
    // copy a 1x1 region, and verify it arrives in the destination
    MGLTestTarget t;
    unsigned char *px;
    GLuint src = 0, dst = 0;

    if (!mgl_target_create(&t, 4, 4, GL_RGBA8, 0)) { CHECK(0); return; }
    mgl_target_bind(&t);

    glGenTextures(1, &src);
    glBindTexture(GL_TEXTURE_2D, src);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                    (const unsigned char[]){ 0x80, 0x40, 0x20, 0xFF });

    glGenTextures(1, &dst);
    glBindTexture(GL_TEXTURE_2D, dst);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);

    glBindTexture(GL_TEXTURE_2D, 0);

    glCopyImageSubData(src, GL_TEXTURE_2D, 0, 1, 1, 0,
                       dst, GL_TEXTURE_2D, 0, 0, 0, 0, 1, 1, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // draw the destination texture and read back the pixel at (0,0)
    // which should now contain the copied colour
    {
        char err[1024] = { 0 };
        GLuint p;
        const char *vs =
            "#version 460 core\n"
            "layout(location = 0) in vec2 pos;\n"
            "out vec2 uv;\n"
            "void main() { uv = pos * 0.5 + 0.5; gl_Position = vec4(pos, 0, 1); }\n";
        const char *fs =
            "#version 460 core\n"
            "uniform sampler2D u_tex;\n"
            "in vec2 uv;\n"
            "layout(location = 0) out vec4 frag;\n"
            "void main() { frag = texture(u_tex, uv); }\n";

        p = mgl_build_program(vs, fs, err, sizeof err);
        CHECK(p != 0);

        glUseProgram(p);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, dst);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glUniform1i(glGetUniformLocation(p, "u_tex"), 0);

        mgl_fullscreen_quad(NULL);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        px = mgl_read_rgba8(&t);
        CHECK(px != NULL);
        if (px)
        {
            unsigned char rgba[4];
            mgl_pixel_at(px, &t, 0, 0, rgba);
            CHECK_EQ_UINT(rgba[0], 0x80u);
            CHECK_EQ_UINT(rgba[1], 0x40u);
            CHECK_EQ_UINT(rgba[2], 0x20u);
            CHECK_EQ_UINT(rgba[3], 0xFFu);
            free(px);
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        glDeleteProgram(p);
    }

    glDeleteTextures(1, &src);
    glDeleteTextures(1, &dst);
    mgl_target_destroy(&t);
}

/* ---------- get internal format i64v ---------- */

GPU_TEST(misc_core, get_internalformati64v_matches_32_bit)
{
    GLint  v32[4] = { -1, -1, -1, -1 };
    GLint64 v64[4] = { -1, -1, -1, -1 };

    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_INTERNALFORMAT_SUPPORTED, 4, v32);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetInternalformati64v(GL_TEXTURE_2D, GL_RGBA8, GL_INTERNALFORMAT_SUPPORTED, 4, v64);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_EQ_INT((GLint)v64[0], v32[0]);
}

GPU_TEST(misc_core, get_internalformati64v_rejects_bad_args)
{
    GLint64 v = -1;

    // invalid target
    glGetInternalformati64v(0x9999, GL_RGBA8, GL_INTERNALFORMAT_SUPPORTED, 1, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // negative count
    glGetInternalformati64v(GL_TEXTURE_2D, GL_RGBA8, GL_INTERNALFORMAT_SUPPORTED, -1, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // NULL params with non-zero count
    glGetInternalformati64v(GL_TEXTURE_2D, GL_RGBA8, GL_INTERNALFORMAT_SUPPORTED, 1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- object labels (KHR_debug) ---------- */

GPU_TEST(misc_core, object_label_round_trip)
{
    GLuint b = 0;
    GLsizei len = 0;
    char got[64] = { 0 };

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 16, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glObjectLabel(GL_BUFFER, b, -1, "my_buffer");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetObjectLabel(GL_BUFFER, b, sizeof got, &len, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_STR_EQ(got, "my_buffer");
    CHECK_EQ_INT(len, 9);

    glDeleteBuffers(1, &b);
}

GPU_TEST(misc_core, object_label_rejects_bad_identifier)
{
    glObjectLabel(0x9999, 1, -1, "x");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glGetObjectLabel(0x9999, 1, 64, NULL, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(misc_core, object_label_rejects_nonexistent_name)
{
    // spec: GL_INVALID_OPERATION if name is not an existing object
    glObjectLabel(GL_BUFFER, 999123, -1, "nope");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGetObjectLabel(GL_BUFFER, 999123, 64, NULL, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(misc_core, object_label_rejects_negative_bufsize)
{
    GLuint b = 0;
    char tmp[8] = { 0 };

    glGenBuffers(1, &b);
    glObjectLabel(GL_BUFFER, b, -1, "tag");

    glGetObjectLabel(GL_BUFFER, b, -1, NULL, tmp);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteBuffers(1, &b);
}

GPU_TEST(misc_core, object_ptr_label_round_trip)
{
    GLsync sync;

    sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    CHECK(sync != NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glObjectPtrLabel(sync, -1, "my_sync");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    {
        GLsizei len = 0;
        char got[64] = { 0 };
        glGetObjectPtrLabel(sync, sizeof got, &len, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_STR_EQ(got, "my_sync");
        CHECK_EQ_INT(len, 7);
    }

    glDeleteSync(sync);
}

GPU_TEST(misc_core, object_ptr_label_rejects_null)
{
    // spec: GL_INVALID_VALUE if ptr is NULL
    glObjectPtrLabel(NULL, -1, "x");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetObjectPtrLabel(NULL, 64, NULL, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(misc_core, object_ptr_label_rejects_invalid_ptr)
{
    // spec: GL_INVALID_VALUE if ptr is not a valid sync object
    glObjectPtrLabel((const void *)0x1234, -1, "x");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetObjectPtrLabel((const void *)0x1234, 64, NULL, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(misc_core, object_ptr_label_rejects_negative_bufsize)
{
    GLsync sync;
    char tmp[8] = { 0 };

    sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    CHECK(sync != NULL);
    glObjectPtrLabel(sync, -1, "tag");

    glGetObjectPtrLabel(sync, -1, NULL, tmp);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteSync(sync);
}

/* ---------- shader precision format ---------- */

GPU_TEST(misc_core, shader_precision_returns_reasonable_values)
{
    GLint range[2] = { -1, -1 };
    GLint prec = -1;

    glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_HIGH_FLOAT, range, &prec);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(range[0] > 0);
    CHECK(range[1] > 0);
    CHECK(prec > 0);

    // ints are 32-bit integers: no fraction, and a range of 31 bits
    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_LOW_INT, range, &prec);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(range[0], 31);
    CHECK_EQ_INT(range[1], 30);
    CHECK_EQ_INT(prec, 0);
}

GPU_TEST(misc_core, shader_precision_rejects_bad_enums)
{
    GLint range[2], prec;

    // invalid shader type
    glGetShaderPrecisionFormat(0x9999, GL_HIGH_FLOAT, range, &prec);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // invalid precision type
    glGetShaderPrecisionFormat(GL_VERTEX_SHADER, 0x9999, range, &prec);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

/* ---------- release shader compiler ---------- */

GPU_TEST(misc_core, release_shader_compiler_is_harmless)
{
    // spec: hint, no error conditions defined
    glReleaseShaderCompiler();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

/* ---------- tex buffer ---------- */

GPU_TEST(misc_core, tex_buffer_rejects_bad_target)
{
    GLuint b = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 64, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // target must be GL_TEXTURE_BUFFER
    glTexBuffer(GL_TEXTURE_2D, GL_RGBA8, b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glTexBufferRange(GL_TEXTURE_2D, GL_RGBA8, b, 0, 64);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteBuffers(1, &b);
}

GPU_TEST(misc_core, tex_buffer_works_on_the_default_texture)
{
    GLuint b = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 64, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Texture 0 is a real texture object here, so this is legal: the spec's
    // error list for TexBuffer has no "nothing bound" case.
    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteBuffers(1, &b);
}

GPU_TEST(misc_core, tex_buffer_requires_valid_buffer_name)
{
    GLuint tex = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_BUFFER, tex);

    // non-existent buffer
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, 999123);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glDeleteTextures(1, &tex);
}

GPU_TEST(misc_core, tex_buffer_detach_with_zero)
{
    // buffer = 0 is valid and detaches any existing buffer
    GLuint tex = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_BUFFER, tex);

    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glDeleteTextures(1, &tex);
}

GPU_TEST(misc_core, tex_buffer_range_validates_offset)
{
    GLuint tex = 0, b = 0;
    GLint alignment = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_BUFFER, tex);

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 256, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // offset must be a multiple of GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT
    glGetIntegerv(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT, &alignment);
    if (alignment > 1)
    {
        glTexBufferRange(GL_TEXTURE_BUFFER, GL_RGBA8, b, alignment + 1, 64);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    }

    // offset + size must not exceed BUFFER_SIZE
    glTexBufferRange(GL_TEXTURE_BUFFER, GL_RGBA8, b, 128, 256);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // size must be > 0
    glTexBufferRange(GL_TEXTURE_BUFFER, GL_RGBA8, b, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // negative offset
    glTexBufferRange(GL_TEXTURE_BUFFER, GL_RGBA8, b, -8, 64);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteBuffers(1, &b);
    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glDeleteTextures(1, &tex);
}

GPU_TEST(misc_core, tex_buffer_attach_and_query)
{
    // attach a buffer via glTexBuffer, then read the binding back
    GLuint tex = 0, b = 0;
    GLint binding = -1;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_BUFFER, tex);

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 256, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &binding);
    CHECK_EQ_UINT((GLuint)binding, tex);

    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &b);
}

// glfwSwapInterval reached MGL and stopped there, so an application that asked
// for no vsync still waited for the display on every frame
GPU_TEST(misc_core, swap_interval_is_remembered)
{
    unsigned int before = 1, off = 1, on = 0;

    MGLget(NULL, MGL_SWAP_INTERVAL, &before);
    CHECK_EQ_UINT(before, 1u);      // vsync unless the application says otherwise

    MGLsetSwapInterval(NULL, 0);
    MGLget(NULL, MGL_SWAP_INTERVAL, &off);
    CHECK_EQ_UINT(off, 0u);

    MGLsetSwapInterval(NULL, 1);
    MGLget(NULL, MGL_SWAP_INTERVAL, &on);
    CHECK_EQ_UINT(on, 1u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

// Three alignments were never filled in and answered 0. GL wants a power of
// two no larger than 256, and an application rounding up to 0 divides by it.
GPU_TEST(misc_core, buffer_offset_alignments_are_real)
{
    static const GLenum pnames[] = {
        GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
        GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT,
        GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT,
    };

    for (int i = 0; i < 3; i++)
    {
        GLint a = 0;

        glGetIntegerv(pnames[i], &a);
        CHECK_MSG(a > 0 && a <= 256 && (a & (a - 1)) == 0, "pname 0x%x answers %d", pnames[i], a);
    }
}

// GL_EXT_shader_integer_mix is advertised: a shader that requires it and
// picks between integer vectors with a bool vector has to build and draw.
GPU_TEST(misc_core, integer_mix_extension_builds_and_selects)
{
    static const char *VS =
        "#version 330 core\n"
        "#extension GL_EXT_shader_integer_mix : require\n"
        "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); gl_PointSize = 1.0; }\n";
    static const char *FS =
        "#version 330 core\n"
        "#extension GL_EXT_shader_integer_mix : require\n"
        "out vec4 o;\n"
        "void main() {\n"
        "    ivec4 v = mix(ivec4(0), ivec4(255), bvec4(true, false, true, false));\n"
        "    o = vec4(v) / 255.0;\n"
        "}\n";
    MGLTestTarget t;
    GLuint prog, vao;
    char log[2048] = { 0 };
    unsigned char *px, c[4] = { 0 };

    prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    mgl_target_create(&t, 1, 1, GL_RGBA8, 0);
    mgl_target_bind(&t);
    glViewport(0, 0, 1, 1);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glDrawArrays(GL_POINTS, 0, 1);
    glDisable(GL_PROGRAM_POINT_SIZE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (px)
    {
        mgl_pixel_at(px, &t, 0, 0, c);
        free(px);
    }
    CHECK_MSG(c[0] == 255 && c[1] == 0 && c[2] == 255 && c[3] == 0, "got %d,%d,%d,%d", c[0], c[1], c[2], c[3]);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// glTextureBarrier makes what was drawn visible to a later draw that samples
// the same texture. It did nothing, so the second draw read the old texels.
GPU_TEST(misc_core, texture_barrier_shows_earlier_rendering)
{
    static const char *VS =
        "#version 450 core\n"
        "void main() {\n"
        "    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2)) * 2.0 - 1.0;\n"
        "    gl_Position = vec4(p, 0.0, 1.0);\n"
        "}\n";
    static const char *FS =
        "#version 450 core\n"
        "layout(binding = 0) uniform sampler2D self;\n"
        "uniform int pass_no;\n"
        "out vec4 o;\n"
        "void main() {\n"
        "    vec4 seen = texelFetch(self, ivec2(gl_FragCoord.xy), 0);\n"
        "    o = pass_no == 0 ? vec4(0.25, 0.0, 0.0, 1.0) : vec4(seen.r * 2.0, 0.0, 0.0, 1.0);\n"
        "}\n";
    GLuint prog, vao, tex, fb;
    char log[2048] = { 0 };
    GLubyte px[4 * 4 * 4] = { 0 };

    prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glViewport(0, 0, 4, 4);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glUniform1i(glGetUniformLocation(prog, "pass_no"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glTextureBarrier();
    glUniform1i(glGetUniformLocation(prog, "pass_no"), 1);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(px[0] >= 126 && px[0] <= 130, "second draw wrote %d, want about 128", px[0]);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
    glDeleteFramebuffers(1, &fb);
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &tex);
    glDeleteProgram(prog);
}

// The CTS texture_barrier disjoint case in small: draws that read texels the
// same draw does not write, from an integer texture it renders into
GPU_TEST(misc_core, rendering_into_a_texture_it_reads_disjointly)
{
    enum { W = 16, H = 16 };
    static const char *VS =
        "#version 450 core\n"
        "void main() {\n"
        "    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2)) * 2.0 - 1.0;\n"
        "    gl_Position = vec4(p, 0.0, 1.0);\n"
        "}\n";
    static const char *FS =
        "#version 450 core\n"
        "uniform int blockSize;\n"
        "uniform usampler2D tex[8];\n"
        "out uvec4 o[8];\n"
        "void main() {\n"
        "    ivec2 c = ivec2(gl_FragCoord.xy), b = c / blockSize;\n"
        "    ivec2 dx = ivec2(blockSize, 0), dy = ivec2(0, blockSize);\n"
        "    if ((b.x + b.y) % 2 != 0) discard;\n"
        "    for (int i = 0; i < 8; ++i)\n"
        "        o[i] = texelFetch(tex[i], c + dx, 0) + texelFetch(tex[i], c - dx, 0) +\n"
        "               texelFetch(tex[i], c + dy, 0) + texelFetch(tex[i], c - dy, 0);\n"
        "}\n";
    static GLuint ref[W * H], got[W * H];
    GLuint prog, vao, tex[8], fb;
    GLenum bufs[8];
    char log[2048] = { 0 };

    prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    glGenTextures(8, tex);
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glUseProgram(prog);
    for (int i = 0; i < 8; i++)
    {
        char name[16];

        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, W, H);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, tex[i], 0);
        snprintf(name, sizeof name, "tex[%d]", i);
        glUniform1i(glGetUniformLocation(prog, name), i);
        bufs[i] = GL_COLOR_ATTACHMENT0 + i;
    }
    glDrawBuffers(8, bufs);
    glViewport(0, 0, W, H);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    
    for (int bs = 1; bs <= 3; bs++)
    {
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                ref[x + y * W] = (GLuint)((y << 12) + x);

        for (int i = 0; i < 8; i++)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RED_INTEGER, GL_UNSIGNED_INT, ref);
        }
        glUniform1i(glGetUniformLocation(prog, "blockSize"), bs);

        for (int pass = 0; pass < 3; pass++)
        {
            glDrawArrays(GL_TRIANGLES, 0, 3);

            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++)
                {
                    if (((x / bs) + (y / bs)) % 2 != 0)
                        continue;
#define AT(X, Y) ((X) < 0 || (X) >= W || (Y) < 0 || (Y) >= H ? 0u : ref[(X) + (Y) * W])
                    ref[x + y * W] = AT(x + bs, y) + AT(x - bs, y) + AT(x, y + bs) + AT(x, y - bs);
#undef AT
                }
        }

        for (int t = 0; t < 8; t++)
        {
            int bad = -1;

            glActiveTexture(GL_TEXTURE0 + t);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, got);

            for (int i = 0; i < W * H && bad < 0; i++)
                if (got[i] != ref[i])
                    bad = i;

            CHECK_MSG(bad < 0, "block size %d, texture %d: texel (%d, %d) is %u, want %u", bs, t,
                      bad % W, bad / W, bad >= 0 ? got[bad] : 0, bad >= 0 ? ref[bad] : 0);
        }
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
    glDeleteFramebuffers(1, &fb);
    glDeleteVertexArrays(1, &vao);
    glActiveTexture(GL_TEXTURE0);
    glDeleteTextures(8, tex);
    glDeleteProgram(prog);
}

// The multi-bind calls: a bad entry fails alone, the span is checked against
// the limits GL reports, a missing array empties the span, and each unit's
// texture binding can be read by index.
GPU_TEST(misc_core, multi_bind_entries_stand_alone)
{
    GLuint bufs[3], tex, cube;
    GLint max_atomic = 0, v = -1, units = 0;
    GLuint names[3];
    static const GLubyte texel[4 * 6] = { 0 };

    glGenBuffers(3, bufs);
    for (int i = 0; i < 3; i++)
    {
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, bufs[i]);
        glBufferData(GL_ATOMIC_COUNTER_BUFFER, 16, NULL, GL_DYNAMIC_COPY);
    }

    // the middle name was never made: it alone fails
    names[0] = bufs[0];
    names[1] = 0x7FFFFFF0u;
    names[2] = bufs[2];
    glBindBuffersBase(GL_ATOMIC_COUNTER_BUFFER, 0, 3, names);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glGetIntegeri_v(GL_ATOMIC_COUNTER_BUFFER_BINDING, 2, &v);
    CHECK_EQ_INT(v, (GLint)bufs[2]);

    glGetIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, &max_atomic);
    glBindBuffersBase(GL_ATOMIC_COUNTER_BUFFER, (GLuint)max_atomic - 1, 2, bufs);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindBuffersBase(GL_ATOMIC_COUNTER_BUFFER, 0, 3, NULL);
    glGetIntegeri_v(GL_ATOMIC_COUNTER_BUFFER_BINDING, 2, &v);
    CHECK_EQ_INT(v, 0);

    // a texture bound to unit 3 reads back through that index
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTextures(3, 1, &tex);
    glGetIntegeri_v(GL_TEXTURE_BINDING_2D, 3, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, (GLint)tex);

    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &units);
    glBindTextures((GLuint)units, 1, &tex);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glBindTextures(3, 1, NULL);

    // an image bound by the multi-bind form comes off again with no array
    glBindImageTextures(0, 1, &tex);
    glGetIntegeri_v(GL_IMAGE_BINDING_NAME, 0, &v);
    CHECK_EQ_INT(v, (GLint)tex);
    glBindImageTextures(0, 1, NULL);
    glGetIntegeri_v(GL_IMAGE_BINDING_NAME, 0, &v);
    CHECK_EQ_INT(v, 0);

    // one face of one cube of a cube map array takes a sub-image
    glGenTextures(1, &cube);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, cube);
    glTexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 1, GL_RGBA8, 1, 1, 6);
    glTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, 0, 1, 1, 6, GL_RGBA, GL_UNSIGNED_BYTE, texel);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);

    glDeleteTextures(1, &cube);
    glDeleteTextures(1, &tex);
    glDeleteBuffers(3, bufs);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

// GL_TEXTURE_BINDING_CUBE_MAP_ARRAY reads back like every other binding.
// It was an unknown enum.
GPU_TEST(misc_core, cube_map_array_binding_reads_back)
{
    GLuint tex;
    GLint v = -1;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, tex);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, (GLint)tex);

    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
    glDeleteTextures(1, &tex);
}

static GLint compiles(const char *src)
{
    GLuint sh = glCreateShader(GL_VERTEX_SHADER);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    glDeleteShader(sh);

    return ok;
}

// mix() on integers needs GLSL 4.50 or GL_EXT_shader_integer_mix; glslang
// only warned about it.
GPU_TEST(misc_core, integer_mix_needs_its_extension_before_450)
{
    CHECK_EQ_INT(compiles("#version 330\nvoid main() { ivec2 a = mix(ivec2(1), ivec2(2), bvec2(true)); "
                          "gl_Position = vec4(a, 0, 1); }\n"), GL_FALSE);
    CHECK_EQ_INT(compiles("#version 330\n#extension GL_EXT_shader_integer_mix : enable\n"
                          "void main() { ivec2 a = mix(ivec2(1), ivec2(2), bvec2(true)); "
                          "gl_Position = vec4(a, 0, 1); }\n"), GL_TRUE);
    CHECK_EQ_INT(compiles("#version 450\nvoid main() { ivec2 a = mix(ivec2(1), ivec2(2), bvec2(true)); "
                          "gl_Position = vec4(a, 0, 1); }\n"), GL_TRUE);
}

static GLint fragmentCompiles(const char *src)
{
    GLuint sh = glCreateShader(GL_FRAGMENT_SHADER);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    glDeleteShader(sh);

    return ok;
}

// Keywords are not names. MGL's own rewrites hid two cases from glslang: a
// subroutine type it drops, and "invariant", which it strips from inputs.
GPU_TEST(misc_core, keywords_are_not_names)
{
    CHECK_EQ_INT(fragmentCompiles("#version 460\nsubroutine void uniform(inout vec4 a);\n"
                                  "subroutine (uniform) void f(inout vec4 a) { a = vec4(1); }\n"
                                  "void main() { }\n"), GL_FALSE);
    CHECK_EQ_INT(fragmentCompiles("#version 460\nsubroutine void sampler2D(inout vec4 a);\n"
                                  "void main() { }\n"), GL_FALSE);
    CHECK_EQ_INT(fragmentCompiles("#version 460\nin vec4 invariant;\nvoid main() { }\n"), GL_FALSE);
    CHECK_EQ_INT(fragmentCompiles("#version 460\nvoid test(in vec4 invariant) { }\nvoid main() { }\n"), GL_FALSE);

    // still fine as what they are
    CHECK_EQ_INT(fragmentCompiles("#version 460\ninvariant in vec4 v;\nout vec4 o;\nvoid main() { o = v; }\n"), GL_TRUE);
    CHECK_EQ_INT(fragmentCompiles("#version 460\nsubroutine void samplerFn(inout vec4 a);\n"
                                  "subroutine (samplerFn) void f(inout vec4 a) { a = vec4(1); }\n"
                                  "subroutine uniform samplerFn s;\nout vec4 o;\n"
                                  "void main() { vec4 a = vec4(0); s(a); o = a; }\n"), GL_TRUE);
}

// std430 is for buffer blocks, and imageSize arrived in GLSL 4.30. glslang
// takes both when it writes SPIR-V, so they compiled.
GPU_TEST(misc_core, std430_uniforms_and_early_image_size_do_not_compile)
{
    CHECK_EQ_INT(fragmentCompiles("#version 430 core\nlayout(std430) uniform UBO { int x; };\n"
                                  "buffer SSBO { int y; };\nvoid main() { y = x; }\n"), GL_FALSE);
    CHECK_EQ_INT(fragmentCompiles("#version 430 core\nlayout(std430) buffer SSBO { int y; };\n"
                                  "void main() { y = 1; }\n"), GL_TRUE);
    CHECK_EQ_INT(fragmentCompiles("#version 420 core\nlayout(location = 0) out vec4 c;\n"
                                  "layout(binding = 0, rg16f) uniform image2D img;\n"
                                  "void main() { c = vec4(imageSize(img), 0, 1); }\n"), GL_FALSE);
    CHECK_EQ_INT(fragmentCompiles("#version 430 core\nlayout(location = 0) out vec4 c;\n"
                                  "layout(binding = 0, rg16f) uniform image2D img;\n"
                                  "void main() { c = vec4(imageSize(img), 0, 1); }\n"), GL_TRUE);
}

// Each stage needs a main. A vertex shader without one linked.
GPU_TEST(misc_core, a_stage_without_main_does_not_link)
{
    static const char *VS = "#version 460\nvoid niam() { gl_Position = vec4(0); }\n";
    static const char *FS = "#version 460\nout vec4 o;\nvoid main() { o = vec4(1); }\n";
    char log[1024] = { 0 };
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);

    CHECK_EQ_UINT(prog, 0);
    if (prog)
        glDeleteProgram(prog);

    // nor does a separable program holding just that stage
    static const char *NOMAIN_FS = "#version 310 es\nuniform highp vec4 u_color;\n"
                                   "layout(location = 0) out mediump vec4 o_color;\n"
                                   "void niaM(void) { o_color = u_color; }\n";
    static const char *MAIN_FS = "#version 310 es\nuniform highp vec4 u_color;\n"
                                 "layout(location = 0) out mediump vec4 o_color;\n"
                                 "void main(void) { o_color = u_color; }\n";
    GLuint sep = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &MAIN_FS), fs = glCreateShader(GL_FRAGMENT_SHADER);
    GLint ok = -1;

    // the shader compiles once with a main, then again without one
    glShaderSource(fs, 1, &MAIN_FS, NULL);
    glCompileShader(fs);
    glAttachShader(sep, fs);
    glShaderSource(fs, 1, &NOMAIN_FS, NULL);
    glCompileShader(fs);
    glLinkProgram(sep);
    glGetProgramiv(sep, GL_LINK_STATUS, &ok);
    CHECK_EQ_INT(ok, GL_FALSE);

    glDeleteShader(fs);
    glDeleteProgram(sep);
    mgl_drain_errors();
}

// A shader whose latest compile failed fails the link, whatever it compiled
// to before.
GPU_TEST(misc_core, a_failed_recompile_fails_the_link)
{
    static const char *GOOD = "#version 460\nout vec4 o;\nvoid main() { o = vec4(1); }\n";
    static const char *BAD = "#version 460\nout vec4 o;\nvoid main() { o = vec4(1) }\n";
    static const char *VS = "#version 460\nvoid main() { gl_Position = vec4(0); }\n";
    GLuint prog = glCreateProgram(), vs = glCreateShader(GL_VERTEX_SHADER), fs = glCreateShader(GL_FRAGMENT_SHADER);
    GLint ok = -1;

    glShaderSource(vs, 1, &VS, NULL);
    glCompileShader(vs);
    glShaderSource(fs, 1, &GOOD, NULL);
    glCompileShader(fs);
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glShaderSource(fs, 1, &BAD, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    CHECK_EQ_INT(ok, GL_FALSE);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK_EQ_INT(ok, GL_FALSE);

    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteProgram(prog);
    mgl_drain_errors();
}

/* ---------- glClipControl changes what is drawn ---------- */

static const char *CLIP_VS =
    "#version 450 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "uniform float z;\n"
    "uniform float lower;\n"
    "void main() { gl_Position = vec4(pos.x, lower > 0.5 ? pos.y * 0.5 - 0.5 : pos.y, z, 1.0); }\n";
static const char *CLIP_FS =
    "#version 450 core\n"
    "out vec4 c;\n"
    "void main() { c = vec4(0.0, 1.0, 0.0, 1.0); }\n";

static GLfloat clipDepthAfter(GLuint prog, GLuint vao, GLenum mode, GLfloat z)
{
    GLfloat d = -1.0f;

    glClipControl(GL_LOWER_LEFT, mode);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUniform1f(glGetUniformLocation(prog, "z"), z);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glReadPixels(4, 4, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d);
    return d;
}

GPU_TEST(misc_core, clip_depth_mode_zero_to_one_keeps_z)
{
    MGLTestTarget t;
    GLuint vbo, vao, prog;
    char log[1024];

    if (!mgl_target_create(&t, 8, 8, GL_RGBA8, 1)) { CHECK(0); return; }
    prog = mgl_build_program(CLIP_VS, CLIP_FS, log, sizeof log);
    CHECK_MSG(prog != 0, "clip program did not build: %s", log);
    if (!prog) { mgl_target_destroy(&t); return; }

    vao = mgl_fullscreen_quad(&vbo);
    glUseProgram(prog);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);

    GLfloat d = clipDepthAfter(prog, vao, GL_NEGATIVE_ONE_TO_ONE, 0.5f);
    CHECK_MSG(fabsf(d - 0.75f) < 1e-3f, "-1..1: depth %g, want 0.75", d);

    d = clipDepthAfter(prog, vao, GL_ZERO_TO_ONE, 0.5f);
    CHECK_MSG(fabsf(d - 0.5f) < 1e-3f, "0..1: depth %g, want 0.5", d);

    // below zero is outside the clip volume now, so nothing is drawn
    d = clipDepthAfter(prog, vao, GL_ZERO_TO_ONE, -0.5f);
    CHECK_MSG(fabsf(d - 1.0f) < 1e-3f, "0..1 at z -0.5: depth %g, want 1 (clipped)", d);

    d = clipDepthAfter(prog, vao, GL_NEGATIVE_ONE_TO_ONE, -0.5f);
    CHECK_MSG(fabsf(d - 0.25f) < 1e-3f, "back to -1..1: depth %g, want 0.25", d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

GPU_TEST(misc_core, clip_origin_upper_left_turns_the_picture_over)
{
    MGLTestTarget t;
    GLuint vbo, vao, prog;
    unsigned char *px, top[4], bottom[4];
    char log[1024];

    if (!mgl_target_create(&t, 8, 8, GL_RGBA8, 0)) { CHECK(0); return; }
    prog = mgl_build_program(CLIP_VS, CLIP_FS, log, sizeof log);
    CHECK_MSG(prog != 0, "clip program did not build: %s", log);
    if (!prog) { mgl_target_destroy(&t); return; }

    vao = mgl_fullscreen_quad(&vbo);
    glUseProgram(prog);
    glUniform1f(glGetUniformLocation(prog, "lower"), 1.0f);

    // back faces are culled; flipping the origin must not make the quad one
    glEnable(GL_CULL_FACE);

    for (int upper = 0; upper < 2; upper++)
    {
        glClipControl(upper ? GL_UPPER_LEFT : GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        px = mgl_read_rgba8(&t);
        mgl_pixel_at(px, &t, 4, 6, top);
        mgl_pixel_at(px, &t, 4, 1, bottom);
        free(px);

        CHECK_MSG((upper ? top[1] : bottom[1]) == 255 && (upper ? bottom[1] : top[1]) == 0,
                  "origin %s: top green %d, bottom green %d", upper ? "upper-left" : "lower-left",
                  top[1], bottom[1]);
    }
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

/* ---------- polygon offset and polygon mode reach the draw ---------- */

static GLfloat offsetDepth(GLuint prog, GLboolean on, GLfloat units, GLfloat clamp)
{
    GLfloat d = -1.0f;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (on) glEnable(GL_POLYGON_OFFSET_FILL); else glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffsetClamp(0.0f, units, clamp);
    glUniform1f(glGetUniformLocation(prog, "z"), 0.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glReadPixels(4, 4, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d);
    return d;
}

GPU_TEST(misc_core, polygon_offset_moves_depth)
{
    MGLTestTarget t;
    GLuint vbo, vao, prog;
    char log[1024];

    if (!mgl_target_create(&t, 8, 8, GL_RGBA8, 1)) { CHECK(0); return; }
    prog = mgl_build_program(CLIP_VS, CLIP_FS, log, sizeof log);
    CHECK_MSG(prog != 0, "offset program did not build: %s", log);
    if (!prog) { mgl_target_destroy(&t); return; }

    vao = mgl_fullscreen_quad(&vbo);
    glUseProgram(prog);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);

    GLfloat ref = offsetDepth(prog, GL_FALSE, 0.0f, 0.0f);
    GLfloat up = offsetDepth(prog, GL_TRUE, 5000.0f, 0.0f);
    GLfloat down = offsetDepth(prog, GL_TRUE, -5000.0f, 0.0f);
    GLfloat clamped = offsetDepth(prog, GL_TRUE, 5000.0f, 0.0001f);

    CHECK_MSG(fabsf(ref - 0.5f) < 1e-3f, "no offset: depth %g, want 0.5", ref);
    CHECK_MSG(up > ref + 1e-4f, "offset up: depth %g, reference %g", up, ref);
    CHECK_MSG(down < ref - 1e-4f, "offset down: depth %g, reference %g", down, ref);
    CHECK_MSG(clamped > ref && clamped < up, "clamped: depth %g, between %g and %g", clamped, ref, up);
    glDisable(GL_POLYGON_OFFSET_FILL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

GPU_TEST(misc_core, polygon_mode_line_leaves_the_middle_empty)
{
    MGLTestTarget t;
    GLuint vbo, vao, prog;
    unsigned char *px, mid[4];
    char log[1024];

    if (!mgl_target_create(&t, 16, 16, GL_RGBA8, 0)) { CHECK(0); return; }
    prog = mgl_build_program(CLIP_VS, CLIP_FS, log, sizeof log);
    CHECK_MSG(prog != 0, "line program did not build: %s", log);
    if (!prog) { mgl_target_destroy(&t); return; }

    vao = mgl_fullscreen_quad(&vbo);
    glUseProgram(prog);
    glUniform1f(glGetUniformLocation(prog, "lower"), 0.0f);

    glPolygonMode(GL_FRONT, GL_LINE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // a quad of two triangles shares a diagonal, so look off it
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    px = mgl_read_rgba8(&t);
    mgl_pixel_at(px, &t, 11, 4, mid);
    free(px);
    CHECK_MSG(mid[1] == 0, "line mode filled the inside (green %d)", mid[1]);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    px = mgl_read_rgba8(&t);
    mgl_pixel_at(px, &t, 11, 4, mid);
    free(px);
    CHECK_MSG(mid[1] == 255, "fill mode left the inside empty (green %d)", mid[1]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// GLSL 4.60 section 7.1: stages of one program that both redeclare
// gl_PerVertex have to agree on its members. These are the CTS's shaders;
// the evaluation stage's isolines and the geometry stage's points disagree
// too, but that is a draw-time error and must not be what stops the link.
GPU_TEST(misc_core, mismatched_per_vertex_blocks_do_not_link)
{
    static const char *VS =
        "#version 410\n"
        "out gl_PerVertex { vec4 gl_Position; };\n"
        "void main() { gl_Position = vec4(2.0); }\n";
    static const char *TCS =
        "#version 410\n"
        "layout (vertices = 4) out;\n"
        "in gl_PerVertex { float gl_PointSize; } gl_in[];\n"
        "out gl_PerVertex { float gl_PointSize; } gl_out[];\n"
        "void main() { gl_out[gl_InvocationID].gl_PointSize = gl_in[0].gl_PointSize + 1.0; }\n";
    static const char *TES =
        "#version 410\n"
        "layout (isolines) in;\n"
        "in gl_PerVertex { float gl_PointSize; vec4 gl_Position; } gl_in[gl_MaxPatchVertices];\n"
        "out gl_PerVertex { float gl_PointSize; vec4 gl_Position; };\n"
        "void main() { gl_Position = vec4(gl_in[0].gl_PointSize) + gl_in[1].gl_Position; }\n";
    static const char *GS =
        "#version 410\n"
        "layout (points) in; layout (points, max_vertices = 4) out;\n"
        "in gl_PerVertex { float gl_ClipDistance[]; } gl_in[];\n"
        "out gl_PerVertex { float gl_ClipDistance[]; };\n"
        "void main() { gl_ClipDistance[0] = 0.5; EmitVertex(); }\n";
    const char *src[4] = { VS, TCS, TES, GS };
    GLenum types[4] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_GEOMETRY_SHADER };
    GLuint p = glCreateProgram();
    GLint ok = -1;

    for (int i = 0; i < 4; i++)
    {
        GLuint sh = glCreateShader(types[i]);
        glShaderSource(sh, 1, &src[i], NULL);
        glCompileShader(sh);
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        CHECK_MSG(ok == GL_TRUE, "stage %d did not compile", i);
        glAttachShader(p, sh);
        glDeleteShader(sh);
    }

    glProgramParameteri(p, GL_PROGRAM_SEPARABLE, GL_TRUE);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    CHECK_MSG(ok == GL_FALSE, "mismatched gl_PerVertex blocks linked");
    glDeleteProgram(p);
    mgl_drain_errors();
}

// A failed relink leaves the program's last good executable, and its uniform
// values, in use (GL 4.6 7.3)
GPU_TEST(misc_core, a_failed_relink_keeps_the_old_executable)
{
    static const char *VS =
        "#version 450 core\n"
        "void main() {\n"
        "    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2)) * 2.0 - 1.0;\n"
        "    gl_Position = vec4(p, 0.0, 1.0);\n"
        "}\n";
    static const char *FS = "#version 450 core\nuniform vec4 c;\nout vec4 o;\nvoid main() { o = c; }\n";
    static const char *NO_MAIN = "#version 450 core\nuniform vec4 c;\nout vec4 o;\nvoid niaM() { o = c; }\n";
    MGLTestTarget t;
    GLuint vs, fs, prog, vao;
    GLint ok = -1;
    unsigned char px4[4] = { 0 };

    if (!mgl_target_create(&t, 4, 4, GL_RGBA8, 0)) { CHECK(0); return; }

    vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &VS, NULL);
    glCompileShader(vs);
    fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &FS, NULL);
    glCompileShader(fs);
    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glUseProgram(prog);
    glUniform4f(glGetUniformLocation(prog, "c"), 1.0f, 1.0f, 1.0f, 1.0f);

    glShaderSource(fs, 1, &NO_MAIN, NULL);
    glCompileShader(fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK_EQ_INT(ok, GL_FALSE);
    mgl_drain_errors();

    mgl_target_bind(&t);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    unsigned char *px = mgl_read_rgba8(&t);

    if (px)
    {
        mgl_pixel_at(px, &t, 1, 1, px4);
        free(px);
    }
    CHECK_MSG(px4[0] == 255 && px4[1] == 255 && px4[2] == 255, "drew %d %d %d, want white", px4[0], px4[1], px4[2]);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// The same through a program pipeline: the stage keeps drawing with the
// executable it had before the failed relink
GPU_TEST(misc_core, a_failed_relink_keeps_the_pipeline_stage)
{
    static const char *VS =
        "#version 450 core\n"
        "out gl_PerVertex { vec4 gl_Position; };\n"
        "void main() {\n"
        "    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2)) * 2.0 - 1.0;\n"
        "    gl_Position = vec4(p, 0.0, 1.0);\n"
        "}\n";
    static const char *FS = "#version 450 core\nuniform vec4 c;\nout vec4 o;\nvoid main() { o = c; }\n";
    static const char *NO_MAIN = "#version 450 core\nuniform vec4 c;\nout vec4 o;\nvoid niaM() { o = c; }\n";
    MGLTestTarget t;
    GLuint vp, fp, pipe, vao, sh;
    GLint ok = -1;
    unsigned char px4[4] = { 0 };

    if (!mgl_target_create(&t, 4, 4, GL_RGBA8, 0)) { CHECK(0); return; }

    vp = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &VS);
    fp = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &FS);
    glGenProgramPipelines(1, &pipe);
    glUseProgramStages(pipe, GL_VERTEX_SHADER_BIT, vp);
    glUseProgramStages(pipe, GL_FRAGMENT_SHADER_BIT, fp);
    glBindProgramPipeline(pipe);
    glProgramUniform4f(fp, glGetUniformLocation(fp, "c"), 1.0f, 1.0f, 1.0f, 1.0f);

    sh = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(sh, 1, &NO_MAIN, NULL);
    glCompileShader(sh);
    glAttachShader(fp, sh);
    glLinkProgram(fp);
    glGetProgramiv(fp, GL_LINK_STATUS, &ok);
    CHECK_EQ_INT(ok, GL_FALSE);
    mgl_drain_errors();

    mgl_target_bind(&t);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    unsigned char *px = mgl_read_rgba8(&t);

    if (px)
    {
        mgl_pixel_at(px, &t, 1, 1, px4);
        free(px);
    }
    CHECK_MSG(px4[0] == 255 && px4[1] == 255 && px4[2] == 255, "drew %d %d %d, want white", px4[0], px4[1], px4[2]);

    glBindProgramPipeline(0);
    glDeleteProgramPipelines(1, &pipe);
    glDeleteVertexArrays(1, &vao);
    glDeleteShader(sh);
    glDeleteProgram(vp);
    glDeleteProgram(fp);
    mgl_target_destroy(&t);
}

// Draws one full-target triangle through a vertex and a fragment program and
// returns the pixel in the middle
static void pipelineDraw(GLuint vp, GLuint fp, unsigned char out[4])
{
    MGLTestTarget t;
    GLuint pipe, vao;
    unsigned char *px;

    memset(out, 0, 4);
    if (!mgl_target_create(&t, 4, 4, GL_RGBA8, 0)) { CHECK(0); return; }

    glGenProgramPipelines(1, &pipe);
    glUseProgramStages(pipe, GL_VERTEX_SHADER_BIT, vp);
    glUseProgramStages(pipe, GL_FRAGMENT_SHADER_BIT, fp);
    glBindProgramPipeline(pipe);
    mgl_target_bind(&t);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (px)
        mgl_pixel_at(px, &t, 1, 1, out);
    free(px);

    glBindProgramPipeline(0);
    glDeleteProgramPipelines(1, &pipe);
    glDeleteVertexArrays(1, &vao);
    mgl_target_destroy(&t);
}

#define FULL_TRIANGLE \
    "    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2)) * 2.0 - 1.0;\n" \
    "    gl_Position = vec4(p, 0.0, 1.0);\n"

// Each separate program keeps its own value for a uniform both declare
GPU_TEST(misc_core, pipeline_stages_keep_their_own_same_named_uniform)
{
    static const char *VS =
        "#version 450 core\n"
        "out gl_PerVertex { vec4 gl_Position; };\n"
        "uniform float s;\n"
        "out float v;\n"
        "void main() {\n" FULL_TRIANGLE "    v = s;\n}\n";
    static const char *FS =
        "#version 450 core\nuniform float s;\nin float v;\nout vec4 o;\nvoid main() { o = vec4(v, s, 0.0, 1.0); }\n";
    GLuint vp = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &VS);
    GLuint fp = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &FS);
    unsigned char px[4];

    glProgramUniform1f(vp, glGetUniformLocation(vp, "s"), 0.25f);
    glProgramUniform1f(fp, glGetUniformLocation(fp, "s"), 0.75f);
    pipelineDraw(vp, fp, px);
    CHECK_MSG(abs(px[0] - 64) <= 2 && abs(px[1] - 191) <= 2, "drew %d %d, want 64 191", px[0], px[1]);

    glDeleteProgram(vp);
    glDeleteProgram(fp);
}

// gl_PointCoord's t runs from the point's top under the default upper-left
// origin, and from its bottom under lower-left
GPU_TEST(misc_core, point_coord_follows_the_sprite_origin)
{
    static const char *vs =
        "#version 330 core\nvoid main() { gl_PointSize = 8.0; gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 330 core\nout vec4 o;\nvoid main() { o = vec4(gl_PointCoord.y, 0.0, 0.0, 1.0); }\n";
    static const GLenum origins[2] = { GL_UPPER_LEFT, GL_LOWER_LEFT };
    char log[1024] = "";
    GLuint prog = mgl_build_program(vs, fs, log, sizeof log), vao;
    GLint origin = 0;
    MGLTestTarget target;

    CHECK_MSG(prog != 0, "did not build: %s", log);
    if (!prog || !mgl_target_create(&target, 8, 8, GL_RGBA8, 0))
        return;

    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT);
    glGetIntegerv(GL_POINT_SPRITE_COORD_ORIGIN, &origin);
    CHECK_EQ_UINT((GLuint)origin, GL_LOWER_LEFT);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glEnable(GL_PROGRAM_POINT_SIZE);

    for (int i = 0; i < 2; i++)
    {
        GLubyte top[4], bottom[4];
        unsigned char *img;

        glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, origins[i]);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_POINTS, 0, 1);
        img = mgl_read_rgba8(&target);
        mgl_pixel_at(img, &target, 4, 7, top);
        mgl_pixel_at(img, &target, 4, 0, bottom);
        free(img);

        // t is 1/16 at the top row and 15/16 at the bottom one, or the reverse
        if (i == 0)
            CHECK_MSG(top[0] < 32 && bottom[0] > 224, "upper-left origin read %u at the top, %u at the bottom",
                      top[0], bottom[0]);
        else
            CHECK_MSG(top[0] > 224 && bottom[0] < 32, "lower-left origin read %u at the top, %u at the bottom",
                      top[0], bottom[0]);
    }

    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_UPPER_LEFT);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    mgl_target_destroy(&target);
}

GPU_TEST(misc_core, an_unset_variable_reads_zero)
{
    // z is never written, as some shipped shaders do; drivers give it zero
    static const char *VS =
        "#version 330 core\n"
        "layout(location = 0) in vec2 pos;\n"
        "void main() { vec4 p; p.w = 1.0; p.xy = pos; gl_Position = p; }\n";
    static const char *FS =
        "#version 330 core\n"
        "out vec4 c;\n"
        "void main() { c = vec4(1.0, 0.0, 0.0, 1.0); }\n";
    MGLTestTarget t;
    char log[1024] = { 0 };
    unsigned char *px, c[4] = { 0 };
    GLuint vbo = 0, vao, prog = mgl_build_program(VS, FS, log, sizeof log);

    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    mgl_target_create(&t, 4, 4, GL_RGBA8, 0);
    mgl_target_bind(&t);
    glViewport(0, 0, 4, 4);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    vao = mgl_fullscreen_quad(&vbo);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    px = mgl_read_rgba8(&t);
    if (px)
        mgl_pixel_at(px, &t, 2, 2, c);
    free(px);
    CHECK_MSG(c[0] == 255, "reads %d %d %d", c[0], c[1], c[2]);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

GPU_TEST(misc_core, active_attributes_are_vertex_inputs_only)
{
    static const char *VS =
        "#version 330 core\n"
        "in vec4 a_0;\n"
        "out vec4 v_color;\n"
        "void main() { v_color = a_0; gl_Position = a_0; }\n";
    static const char *FS =
        "#version 330 core\n"
        "in vec4 v_color;\n"
        "out vec4 color;\n"
        "void main() { color = v_color; }\n";
    char log[1024] = "";
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);
    GLint count = -1, longest = -1;

    CHECK_MSG(prog != 0, "program did not build: %s", log);
    if (!prog)
        return;

    glGetProgramiv(prog, GL_ACTIVE_ATTRIBUTES, &count);
    glGetProgramiv(prog, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &longest);
    CHECK_EQ_INT(count, 1);
    CHECK_EQ_INT(longest, 4);

    glDeleteProgram(prog);
}

GPU_TEST(misc_core, get_reads_caps_hints_and_scaled_values)
{
    GLboolean b = GL_FALSE;
    GLint i[4] = { 0 }, hint = 0;
    GLint64 v[4] = { -1, -1, -1, -1 };

    // capabilities read back through glGet too
    glEnable(GL_BLEND);
    glGetBooleanv(GL_BLEND, &b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(b == GL_TRUE);
    glDisable(GL_BLEND);

    // every value of a multi-value query arrives as 64 bits
    glViewport(1, 2, 3, 4);
    glGetInteger64v(GL_VIEWPORT, v);
    CHECK_MSG(v[0] == 1 && v[1] == 2 && v[2] == 3 && v[3] == 4, "viewport %lld %lld %lld %lld",
              (long long)v[0], (long long)v[1], (long long)v[2], (long long)v[3]);

    // colours and depths read as integers span the integer range, and the clear depth is clamped
    glClearDepth(2.0);
    glGetIntegerv(GL_DEPTH_CLEAR_VALUE, i);
    CHECK_EQ_INT(i[0], 0x7fffffff);
    glClearDepth(1.0);

    glHint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, GL_NICEST);
    glGetIntegerv(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, &hint);
    CHECK_EQ_INT(hint, GL_NICEST);
    glHint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, GL_DONT_CARE);
    glHint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, GL_RED);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(misc_core, deep_3d_textures_and_copy_limits)
{
    static GLubyte texels[4 * 4 * 2 * 8];
    GLuint tex;
    GLint most = 0;

    // a 3D texture deeper than it is wide has levels down to 1x1x1
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_3D, tex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    glTexImage3D(GL_TEXTURE_3D, 3, GL_RGBA8, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glDeleteTextures(1, &tex);

    // a copy bigger than any texture
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &most);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, most + 1, 1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glDeleteTextures(1, &tex);
}

GPU_TEST(misc_core, state_queries_read_the_bound_objects)
{
    static const char *src = "#version 330 core\nvoid main() { gl_Position = vec4(0.0); }\n";
    GLuint buf, vao, sh = glCreateShader(GL_VERTEX_SHADER), tex;
    GLint v = -1;
    GLfloat f = 0;
    GLboolean b = GL_FALSE;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glGetBooleanv(GL_ARRAY_BUFFER_BINDING, &b);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &v);
    CHECK(b == GL_TRUE);
    CHECK_EQ_INT(v, (GLint)buf);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &v);
    CHECK_EQ_INT(v, (GLint)vao);
    glBindVertexArray(0);

    glGetFloatv(GL_MIN_PROGRAM_TEXEL_OFFSET, &f);
    CHECK_MSG(f <= -8.0f, "min texel offset %f", f);

    // a shader never compiled has not compiled, and its length counts the null
    glGetShaderiv(sh, GL_COMPILE_STATUS, &v);
    CHECK_EQ_INT(v, GL_FALSE);
    glShaderSource(sh, 1, &src, NULL);
    glGetShaderiv(sh, GL_SHADER_SOURCE_LENGTH, &v);
    CHECK_EQ_INT(v, (GLint)strlen(src) + 1);

    // float parameters round to the nearest integer
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, -74.6f);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, &v);
    CHECK_EQ_INT(v, -75);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 2.7f);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, &v);
    CHECK_EQ_INT(v, 3);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
    glDeleteShader(sh);
    glDeleteTextures(1, &tex);
}

GPU_TEST(misc_core, debug_log_state_queries)
{
    GLint n = -1, len = -1, depth = -1;

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 1, GL_DEBUG_SEVERITY_NOTIFICATION, -1, "hello");
    glGetIntegerv(GL_DEBUG_LOGGED_MESSAGES, &n);
    glGetIntegerv(GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH, &len);
    glGetIntegerv(GL_DEBUG_GROUP_STACK_DEPTH, &depth);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(n >= 1);
    CHECK_EQ_INT(len, 6);
    CHECK_EQ_INT(depth, 1);

    // empty the log again
    while (glGetDebugMessageLog(16, 0, NULL, NULL, NULL, NULL, NULL, NULL) > 0)
        ;
    glDisable(GL_DEBUG_OUTPUT);
}

// with no buffer, glGetObjectLabel gives the label's whole length
GPU_TEST(misc_core, object_label_length_can_be_asked_alone)
{
    GLuint buf;
    GLsizei len = -1;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glObjectLabel(GL_BUFFER, buf, -1, "a label");
    glGetObjectLabel(GL_BUFFER, buf, 0, &len, NULL);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);
    CHECK_EQ_INT(len, 7);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &buf);
}

// glGetInternalformativ writes only the values the query has
GPU_TEST(misc_core, internalformat_query_leaves_the_rest_of_the_buffer)
{
    GLint v[3] = { -7, -7, -7 };

    glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, 3, v);
    CHECK_EQ_INT(v[0], 1);
    CHECK_EQ_INT(v[1], -7);
    CHECK_EQ_INT(v[2], -7);
}

// a member named like a bool member of another struct keeps its own type
GPU_TEST(misc_core, struct_member_type_comes_from_its_own_struct)
{
    static const char *VS =
        "#version 330 core\n"
        "struct A { uvec2 d; };\n"
        "uniform A a;\n"
        "struct B { bvec3 d; };\n"
        "uniform B b;\n"
        "void main() { gl_Position = vec4(float(a.d.x) + (b.d.x ? 1.0 : 0.0)); }\n";
    static const char *FS = "#version 330 core\nout vec4 o;\nvoid main() { o = vec4(1.0); }\n";
    static const GLuint u[2] = { 1, 2 };
    char log[1024] = { 0 };
    GLuint p = mgl_build_program(VS, FS, log, sizeof log);
    GLint type = 0, size = 0;
    GLenum t = 0;
    char name[32];

    CHECK_MSG(p != 0, "%s", log);
    if (!p)
        return;

    glUseProgram(p);
    mgl_drain_errors();
    glUniform2uiv(glGetUniformLocation(p, "a.d"), 1, u);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    for (GLint i = 0; i < 2; i++)
    {
        glGetActiveUniform(p, (GLuint)i, sizeof name, NULL, &size, &t, name);
        if (!strcmp(name, "a.d"))
            type = (GLint)t;
    }
    CHECK_EQ_INT(type, GL_UNSIGNED_INT_VEC2);

    glUseProgram(0);
    glDeleteProgram(p);
}

// a viewport larger than GL_MAX_VIEWPORT_DIMS is clamped to it
GPU_TEST(misc_core, viewport_size_is_clamped_to_the_maximum)
{
    GLint most[2] = { 0, 0 }, vp[4] = { 0, 0, 0, 0 }, was[4];

    glGetIntegerv(GL_VIEWPORT, was);
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, most);
    glViewport(0, 0, most[0] * 2, most[1] * 2);
    glGetIntegerv(GL_VIEWPORT, vp);
    CHECK_EQ_INT(vp[2], most[0]);
    CHECK_EQ_INT(vp[3], most[1]);
    glViewport(was[0], was[1], was[2], was[3]);
}

// a compute program over its block, shared memory or work group limits does not link
GPU_TEST(misc_core, compute_programs_over_their_limits_do_not_link)
{
    GLint blocks = 0, shared = 0;
    char src[1024], log[1024];

    glGetIntegerv(GL_MAX_COMPUTE_UNIFORM_BLOCKS, &blocks);
    glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &shared);

    snprintf(src, sizeof src,
             "#version 450 core\nlayout(local_size_x = 1) in;\n"
             "layout(std140, binding = 0) uniform B { vec4 v; } b[%d];\n"
             "layout(std430, binding = 0) buffer O { vec4 o; };\n"
             "void main() { o = b[0].v; }\n", blocks + 1);
    CHECK(mgl_build_compute_program(src, log, sizeof log) == 0);

    snprintf(src, sizeof src,
             "#version 450 core\nlayout(local_size_x = 1) in;\n"
             "shared uint s[%d];\n"
             "layout(std430, binding = 0) buffer O { uint o; };\n"
             "void main() { s[0] = 1u; barrier(); o = s[%d]; }\n", shared / 4 + 1, shared / 4);
    CHECK(mgl_build_compute_program(src, log, sizeof log) == 0);

    snprintf(src, sizeof src,
             "#version 450 core\nlayout(local_size_x = 1024, local_size_y = 1024) in;\nvoid main() {}\n");
    CHECK(mgl_build_compute_program(src, log, sizeof log) == 0);

    // right at the limits still links
    snprintf(src, sizeof src,
             "#version 450 core\nlayout(local_size_x = 1) in;\n"
             "shared uint s[%d];\n"
             "layout(std430, binding = 0) buffer O { uint o; };\n"
             "void main() { s[0] = 1u; barrier(); o = s[%d]; }\n", shared / 4, shared / 4 - 1);
    GLuint ok = mgl_build_compute_program(src, log, sizeof log);
    CHECK_MSG(ok != 0, "%s", log);
    glDeleteProgram(ok);
    mgl_drain_errors();
}

static const char *QUAD_VS =
    "#version 330 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(location = 1) in float shade;\n"
    "out float v;\n"
    "void main() { gl_Position = vec4(pos, 0.0, 1.0); v = shade; }\n";
static const char *SHADE_FS = "#version 330 core\nin float v;\nout vec4 o;\nvoid main() { o = vec4(v, 0.0, 0.0, 1.0); }\n";

static void centrePixel(MGLTestTarget *t, unsigned char out[4])
{
    unsigned char *px = mgl_read_rgba8(t);

    memset(out, 0, 4);
    if (px)
        mgl_pixel_at(px, t, t->width / 2, t->height / 2, out);
    free(px);
}

// GL clamps the stencil reference to what the stencil bits hold
GPU_TEST(misc_core, stencil_reference_is_clamped)
{
    MGLTestTarget t;
    char log[1024] = { 0 };
    GLuint prog = mgl_build_program(QUAD_VS, SHADE_FS, log, sizeof log), vbo = 0, vao;
    unsigned char c[4];

    if (!prog || !mgl_target_create(&t, 8, 8, GL_RGBA8, 1)) { CHECK_MSG(0, "%s", log); return; }

    mgl_target_bind(&t);
    glClearColor(0, 0, 0, 1);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, -1, 0xFF);
    glUseProgram(prog);
    vao = mgl_fullscreen_quad(&vbo);
    glVertexAttrib1f(1, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_STENCIL_TEST);
    centrePixel(&t, c);
    CHECK_MSG(c[0] == 255, "drew %d, want 255", c[0]);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glUseProgram(0);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// a texture deleted while attached to a framebuffer that is not bound stays attached
GPU_TEST(misc_core, deleted_texture_stays_attached_to_an_unbound_framebuffer)
{
    GLuint tex, fbo;
    GLubyte px[4] = { 0 };

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteTextures(1, &tex);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    CHECK_EQ_UINT(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
    glViewport(0, 0, 4, 4);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);
    CHECK_MSG(px[0] == 255, "read %d, want 255", px[0]);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
}

// the scissor test limits what a blit writes
GPU_TEST(misc_core, blit_honours_the_scissor)
{
    MGLTestTarget a, b;
    unsigned char *px;
    unsigned char in[4] = { 0 }, out[4] = { 0 };

    if (!mgl_target_create(&a, 8, 8, GL_RGBA8, 0) || !mgl_target_create(&b, 8, 8, GL_RGBA8, 0)) { CHECK(0); return; }

    mgl_target_bind(&a);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    mgl_target_bind(&b);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, a.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, b.fbo);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, 4, 4);
    // a scaled blit, which is drawn rather than copied
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 8, 8, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glDisable(GL_SCISSOR_TEST);

    px = mgl_read_rgba8(&b);
    if (px)
    {
        mgl_pixel_at(px, &b, 1, 1, in);
        mgl_pixel_at(px, &b, 6, 6, out);
    }
    free(px);
    CHECK_MSG(in[0] == 255 && out[0] == 0, "inside %d, outside %d", in[0], out[0]);

    mgl_target_destroy(&a);
    mgl_target_destroy(&b);
}

// float depth is clamped to [0, 1] as it is uploaded
GPU_TEST(misc_core, float_depth_uploads_are_clamped)
{
    static const GLfloat in[4] = { -0.5f, 0.25f, 1.5f, 1.0f };
    GLfloat got[4] = { 9, 9, 9, 9 };
    GLuint tex;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 2, 2, 0, GL_DEPTH_COMPONENT, GL_FLOAT, in);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, got);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);
    CHECK_MSG(got[0] == 0.0f && got[1] == 0.25f && got[2] == 1.0f, "got %g %g %g", got[0], got[1], got[2]);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &tex);
}

// GL takes floor(source position of the pixel centre) for a nearest blit.
// At 54 texels onto 81 pixels some centres land exactly on texel edges.
GPU_TEST(misc_core, nearest_blit_picks_the_texel_gl_names)
{
    GLuint tex[2], fbo[2];
    GLubyte in[80 * 4], out[100 * 4] = { 0 };
    int wrong = 0, first = -1;

    for (int i = 0; i < 80; i++)
    {
        in[i * 4 + 0] = (GLubyte)(i * 3);
        in[i * 4 + 1] = 0;
        in[i * 4 + 2] = 0;
        in[i * 4 + 3] = 255;
    }

    glGenTextures(2, tex);
    glGenFramebuffers(2, fbo);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 80, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, in);
    glBindTexture(GL_TEXTURE_2D, tex[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 100, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo[0]);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[0], 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo[1]);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[1], 0);

    glBlitFramebuffer(20, 0, 74, 1, 10, 0, 91, 1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo[1]);
    glReadPixels(0, 0, 100, 1, GL_RGBA, GL_UNSIGNED_BYTE, out);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    for (int x = 10; x < 91; x++)
    {
        int want = 20 + (int)((x - 10 + 0.5) * 54.0 / 81.0);

        if (out[x * 4] != want * 3)
        {
            wrong++;
            if (first < 0)
                first = x;
        }
    }
    CHECK_MSG(wrong == 0, "%d pixels wrong, first at %d", wrong, first);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(2, fbo);
    glDeleteTextures(2, tex);
}

// a vertex shader whose inputs need more locations than GL_MAX_VERTEX_ATTRIBS does not link
GPU_TEST(misc_core, too_many_vertex_inputs_do_not_link)
{
    char vs[4096], log[1024];
    GLint most = 0;
    int at;

    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &most);
    at = snprintf(vs, sizeof vs, "#version 330 core\n");
    for (int i = 0; i <= most; i++)
        at += snprintf(vs + at, sizeof vs - (size_t)at, "in vec4 a%d;\n", i);
    at += snprintf(vs + at, sizeof vs - (size_t)at, "void main() { gl_Position = vec4(0.0)");
    for (int i = 0; i <= most; i++)
        at += snprintf(vs + at, sizeof vs - (size_t)at, " + a%d", i);
    snprintf(vs + at, sizeof vs - (size_t)at, "; }\n");

    GLuint p = mgl_build_program(vs, "#version 330 core\nout vec4 o;\nvoid main() { o = vec4(1.0); }\n", log, sizeof log);
    CHECK_EQ_UINT(p, 0u);
    glDeleteProgram(p);
    mgl_drain_errors();
}
