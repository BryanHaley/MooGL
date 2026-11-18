/*
 * test_uniform_query.c
 * MGL
 *
 * Uniform introspection: glGetActiveUniform, glGetUniform*iv,
 * glGetnUniform*iv, uniform block queries and the subroutine uniform family.
 */

#include "mgl_test.h"
#include "harness.h"
#include <string.h>

static const char *VS_INT =
    "#version 460 core\n"
    "layout(location = 0) uniform int u_count;\n"
    "layout(location = 1) uniform uint u_mask;\n"
    "layout(location = 0) in vec2 pos;\n"
    "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *VS_BLOCK =
    "#version 460 core\n"
    "layout(std140, binding = 0) uniform TestBlock {\n"
    "    vec4 color;\n"
    "    float intensity;\n"
    "};\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(location = 0) out vec4 v_col;\n"
    "void main() { v_col = color * intensity; gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *FS_FLAT =
    "#version 460 core\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() { frag = vec4(1.0); }\n";

/* ---------- glGetActiveUniform ---------- */

GPU_TEST(uniform_query, get_active_uniform_reports_name_type_size)
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_INT, FS_FLAT, err, sizeof err);
    char name[64] = { 0 };
    GLsizei len = 0;
    GLint size = 0;
    GLenum type = 0;

    CHECK_MSG(prog != 0, "link failed: %s", err);
    if (!prog) return;

    // Per GL 4.6 § 7.3.1: glGetActiveUniform writes name, length, size and
    // type for the uniform at the given index. MGL's implementation is a stub
    // that only sets GL_INVALID_OPERATION instead.
    glGetActiveUniform(prog, 0, sizeof name, &len, &size, &type, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(size, 1);
    CHECK_MSG(len > 0, "uniform name length must be > 0");

    // Out of range index
    glGetActiveUniform(prog, 9999, sizeof name, &len, &size, &type, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // Negative bufSize
    glGetActiveUniform(prog, 0, -1, &len, &size, &type, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(prog);
}

/* ---------- glGetUniformiv / glGetUniformuiv ---------- */

GPU_TEST(uniform_query, get_uniformiv_reads_back_integers)
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_INT, FS_FLAT, err, sizeof err);
    GLint loc_count, loc_mask;
    GLint iv[4] = { -1, -1, -1, -1 };
    GLuint uv[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };

    CHECK_MSG(prog != 0, "link failed: %s", err);
    if (!prog) return;

    glUseProgram(prog);
    loc_count = glGetUniformLocation(prog, "u_count");
    loc_mask = glGetUniformLocation(prog, "u_mask");

    if (loc_count < 0 || loc_mask < 0)
    {
        glDeleteProgram(prog);
        SKIP("uniforms have no locations");
    }

    // Set and read back a signed int uniform
    glUniform1i(loc_count, 42);
    glGetUniformiv(prog, loc_count, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(iv[0], 42);

    // Set and read back an unsigned int uniform
    glUniform1ui(loc_mask, 0xDEADu);
    glGetUniformuiv(prog, loc_mask, uv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_UINT(uv[0], 0xDEADu);

    // The int readback on a uint location still copies the stored bits
    memset(iv, 0, sizeof iv);
    glGetUniformiv(prog, loc_mask, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_UINT((GLuint)iv[0], 0xDEADu);

    glUseProgram(0);
    glDeleteProgram(prog);
}

GPU_TEST(uniform_query, get_uniformiv_rejects_bad_args)
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_INT, FS_FLAT, err, sizeof err);
    GLint iv[4] = { 0 };

    CHECK_MSG(prog != 0, "link failed: %s", err);
    if (!prog) return;

    // NULL params
    glGetUniformiv(prog, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetUniformuiv(prog, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // Negative location
    glGetUniformiv(prog, -1, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // Non-existent program
    glGetUniformiv(999123, 0, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(prog);
}

/* ---------- glGetnUniformiv / glGetnUniformuiv ---------- */

GPU_TEST(uniform_query, getn_uniform_checks_buffer_size)
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_INT, FS_FLAT, err, sizeof err);
    GLint loc;
    GLint iv[4] = { 0 };

    CHECK_MSG(prog != 0, "link failed: %s", err);
    if (!prog) return;

    glUseProgram(prog);
    loc = glGetUniformLocation(prog, "u_count");
    if (loc < 0) { glDeleteProgram(prog); SKIP("u_count has no location"); }

    glUniform1i(loc, 7);

    // Enough space: one GLint fits
    glGetnUniformiv(prog, loc, sizeof iv, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(iv[0], 7);

    // Not enough: 2 bytes < 4 bytes for one int
    glGetnUniformiv(prog, loc, 2, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // Negative bufSize
    glGetnUniformiv(prog, loc, -1, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // NULL params
    glGetnUniformiv(prog, loc, sizeof iv, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUseProgram(0);
    glDeleteProgram(prog);
}

GPU_TEST(uniform_query, getn_uniformuiv_rejects_short_buffer)
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_INT, FS_FLAT, err, sizeof err);
    GLint loc;
    GLuint uv[4] = { 0 };

    CHECK_MSG(prog != 0, "link failed: %s", err);
    if (!prog) return;

    glUseProgram(prog);
    loc = glGetUniformLocation(prog, "u_mask");
    if (loc < 0) { glDeleteProgram(prog); SKIP("u_mask has no location"); }

    glUniform1ui(loc, 0xFFu);

    // Enough space
    glGetnUniformuiv(prog, loc, sizeof uv, uv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_UINT(uv[0], 0xFFu);

    // Not enough: 1 byte < 4 bytes for one uint
    glGetnUniformuiv(prog, loc, 1, uv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUseProgram(0);
    glDeleteProgram(prog);
}

/* ---------- uniform block queries ---------- */

GPU_TEST(uniform_query, active_uniform_block_name_and_properties)
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_BLOCK, FS_FLAT, err, sizeof err);
    GLint nblocks = 0;
    GLint val = -1;
    char name[64] = { 0 };
    GLsizei len = 0;

    if (!prog) { SKIP("UBO shader did not compile"); return; }

    glGetProgramiv(prog, GL_ACTIVE_UNIFORM_BLOCKS, &nblocks);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    if (nblocks == 0) { glDeleteProgram(prog); SKIP("no uniform blocks reflected"); return; }

    // Query the block name
    glGetActiveUniformBlockName(prog, 0, sizeof name, &len, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(len > 0, "block name length must be > 0");

    // Query the binding point (set in the shader to 0)
    glGetActiveUniformBlockiv(prog, 0, GL_UNIFORM_BLOCK_BINDING, &val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(val, 0);

    // Query the name length
    glGetActiveUniformBlockiv(prog, 0, GL_UNIFORM_BLOCK_NAME_LENGTH, &val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(val > 0, "block name length reported as %d", val);

    // Out of range index
    glGetActiveUniformBlockiv(prog, 9999, GL_UNIFORM_BLOCK_BINDING, &val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetActiveUniformBlockName(prog, 9999, sizeof name, &len, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // Bad pname
    glGetActiveUniformBlockiv(prog, 0, 0x9999, &val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteProgram(prog);
}

GPU_TEST(uniform_query, active_uniform_block_errors)
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_INT, FS_FLAT, err, sizeof err);
    GLint val = -1;
    char name[64] = { 0 };
    GLsizei len = 0;

    CHECK_MSG(prog != 0, "link failed: %s", err);
    if (!prog) return;

    // No uniform blocks in this program, so any index is out of range
    glGetActiveUniformBlockiv(prog, 0, GL_UNIFORM_BLOCK_BINDING, &val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetActiveUniformBlockName(prog, 0, sizeof name, &len, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // Invalid program
    glGetActiveUniformBlockiv(999123, 0, GL_UNIFORM_BLOCK_BINDING, &val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // NULL params
    glGetActiveUniformBlockiv(prog, 0, GL_UNIFORM_BLOCK_BINDING, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // Negative bufSize
    glGetActiveUniformBlockName(prog, 0, -1, &len, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(prog);
}

/* ---------- subroutine uniform family ---------- */

GPU_TEST(uniform_query, subroutine_uniforms_return_no_active)
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_INT, FS_FLAT, err, sizeof err);
    GLuint uv = 0;

    CHECK_MSG(prog != 0, "link failed: %s", err);
    if (!prog) return;

    glUseProgram(prog);

    // MGL captures no subroutines, so any index is out of range.
    // Per GL 4.6 § 7.3.1.1: GL_INVALID_VALUE when index >= count.
    glGetActiveSubroutineUniformiv(prog, GL_VERTEX_SHADER, 0,
                                   GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS,
                                   &(GLint){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetActiveSubroutineUniformName(prog, GL_VERTEX_SHADER, 0, 64, NULL,
                                     (char[64]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // glGetUniformSubroutineuiv: any location is out of range
    glGetUniformSubroutineuiv(GL_VERTEX_SHADER, 0, &uv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUseProgram(0);

    // No program bound → GL_INVALID_OPERATION
    glGetUniformSubroutineuiv(GL_VERTEX_SHADER, 0, &uv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // NULL params → GL_INVALID_VALUE (checked before the program check)
    glGetUniformSubroutineuiv(GL_VERTEX_SHADER, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // Bad shader type for the subroutine queries
    glGetActiveSubroutineUniformiv(prog, 0x9999, 0,
                                   GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS,
                                   &(GLint){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteProgram(prog);
}

// A shader's name where a program's belongs is the wrong kind of object, an
// INVALID_OPERATION. It read as an unknown name.
GPU_TEST(uniform_query, a_shader_name_is_not_a_program)
{
    GLuint sh = glCreateShader(GL_VERTEX_SHADER);
    GLfloat f;
    GLint i;

    glGetUniformfv(sh, 0, &f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glGetUniformiv(sh, 0, &i);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glGetUniformfv(987654, 0, &f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteShader(sh);
}

// Shaders and programs share one set of names. A shader could get the same
// number as a live program, so asking about one found the other.
GPU_TEST(uniform_query, shaders_and_programs_never_share_a_name)
{
    GLuint p[4], s[4];

    for (int i = 0; i < 4; i++)
    {
        p[i] = glCreateProgram();
        s[i] = glCreateShader(GL_FRAGMENT_SHADER);
    }

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            CHECK_MSG(p[i] != s[j], "program %d and shader %d are both %u", i, j, p[i]);

    CHECK(glIsShader(p[0]) == GL_FALSE);
    CHECK(glIsProgram(s[0]) == GL_FALSE);

    for (int i = 0; i < 4; i++)
    {
        glDeleteProgram(p[i]);
        glDeleteShader(s[i]);
    }
    mgl_drain_errors();
}

GPU_TEST(uniform_query, struct_samplers_keep_their_gl_names)
{
    static const char *VS =
        "#version 330 core\n"
        "layout(location = 0) in vec2 pos;\n"
        "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";
    static const char *FS =
        "#version 330 core\n"
        "struct S { sampler2D my_tex; float x; };\n"
        "uniform S u_var;\n"
        "uniform sampler2D many[2];\n"
        "out vec4 color;\n"
        "void main() { color = texture(u_var.my_tex, vec2(0.5)) * u_var.x + texture(many[1], vec2(0.5)); }\n";
    const GLchar *names[1] = { "u_var.my_tex" };
    char log[1024] = "", name[64];
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log), index = GL_INVALID_INDEX;
    GLint count = 0, found = 0;

    CHECK_MSG(prog != 0, "%s", log);
    if (!prog)
        return;

    glGetUniformIndices(prog, 1, names, &index);
    CHECK(index != GL_INVALID_INDEX);
    CHECK(glGetUniformLocation(prog, "u_var.my_tex") >= 0);

    glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &count);
    for (GLint i = 0; i < count; i++)
    {
        GLint size;
        GLenum type;

        glGetActiveUniform(prog, (GLuint)i, sizeof name, NULL, &size, &type, name);
        found += !strcmp(name, "u_var.my_tex");
        // an array is named with "[0]"
        found += !strcmp(name, "many[0]") * 10;
    }
    CHECK_EQ_INT(found, 11);

    glDeleteProgram(prog);
}

GPU_TEST(uniform_query, a_sampler_struct_in_both_stages_is_one_uniform)
{
    static const char *VS =
        "#version 330 core\n"
        "struct S { sampler2D a; samplerCube b; };\n"
        "uniform S u_var;\n"
        "layout(location = 0) in vec2 pos;\n"
        "out float v;\n"
        "void main() { v = texture(u_var.a, vec2(0.0)).x + texture(u_var.b, vec3(0.0)).x;\n"
        "              gl_Position = vec4(pos, 0.0, 1.0); }\n";
    static const char *FS =
        "#version 330 core\n"
        "struct S { sampler2D a; samplerCube b; };\n"
        "uniform S u_var;\n"
        "in float v;\n"
        "out vec4 color;\n"
        "void main() { color = vec4(v) + texture(u_var.a, vec2(0.0)) + texture(u_var.b, vec3(0.0)); }\n";
    char log[1024] = "";
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);
    GLint count = 0;

    CHECK_MSG(prog != 0, "%s", log);
    if (!prog)
        return;

    glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &count);
    CHECK_EQ_INT(count, 2);
    glDeleteProgram(prog);
}

GPU_TEST(uniform_query, bool_arrays_read_back_every_element)
{
    static const char *VS =
        "#version 330 core\n"
        "layout(location = 0) in vec2 pos;\n"
        "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";
    static const char *FS =
        "#version 330 core\n"
        "struct S { float f; bvec2 m[3]; };\n"
        "uniform S s;\n"
        "uniform bool b[3];\n"
        "out vec4 color;\n"
        "void main() { color = vec4(s.f + float(s.m[2].y) + float(b[2])); }\n";
    char log[1024] = "";
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);
    GLint set[2] = { 0, 1 }, got[2] = { 9, 9 };

    CHECK_MSG(prog != 0, "%s", log);
    if (!prog)
        return;

    glUseProgram(prog);
    glUniform2iv(glGetUniformLocation(prog, "s.m[2]"), 1, set);
    glGetUniformiv(prog, glGetUniformLocation(prog, "s.m[2]"), got);
    CHECK_MSG(got[0] == 0 && got[1] == 1, "s.m[2] read %d %d", got[0], got[1]);

    glUniform1i(glGetUniformLocation(prog, "b[2]"), 1);
    glGetUniformiv(prog, glGetUniformLocation(prog, "b[2]"), got);
    CHECK_EQ_INT(got[0], 1);

    glUseProgram(0);
    glDeleteProgram(prog);
}

GPU_TEST(uniform_query, samplers_in_an_array_of_structs_are_listed_per_element)
{
    static const char *VS =
        "#version 330 core\n"
        "layout(location = 0) in vec2 pos;\n"
        "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";
    static const char *FS =
        "#version 330 core\n"
        "struct S { sampler2D a; sampler2D b[2]; float f; };\n"
        "uniform S u_var[2];\n"
        "out vec4 color;\n"
        "void main() { color = texture(u_var[0].a, vec2(0.0)) + texture(u_var[1].b[1], vec2(0.0)) * u_var[1].f; }\n";
    const GLchar *names[2] = { "u_var[1].a", "u_var[1].b[0]" };
    char log[1024] = "", name[64];
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log), idx[2] = { GL_INVALID_INDEX, GL_INVALID_INDEX };
    GLint count = 0, found = 0;

    CHECK_MSG(prog != 0, "%s", log);
    if (!prog)
        return;

    glGetUniformIndices(prog, 2, names, idx);
    CHECK(idx[0] != GL_INVALID_INDEX && idx[1] != GL_INVALID_INDEX);

    glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &count);
    for (GLint i = 0; i < count; i++)
    {
        GLint size;
        GLenum type;

        glGetActiveUniform(prog, (GLuint)i, sizeof name, NULL, &size, &type, name);
        found += !strcmp(name, "u_var[0].a") + !strcmp(name, "u_var[1].a") +
                 (!strcmp(name, "u_var[1].b[0]") && size == 2);
    }
    CHECK_EQ_INT(found, 3);

    glDeleteProgram(prog);
}

// a struct uniform nobody set reads as zero, however big it is
GPU_TEST(uniform_query, a_large_unset_struct_reads_zero)
{
    static const char *VS =
        "#version 330 core\n"
        "layout(location = 0) in vec2 pos;\n"
        "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";
    static const char *FS =
        "#version 330 core\n"
        "struct S { mat4 a; mat2 b; mat4 c[3]; mat2 d[3]; };\n"
        "uniform S u;\n"
        "out vec4 o;\n"
        "void main() { o = vec4(u.d[2][1][1] == 0.0 && u.c[2][3][3] == 0.0 ? 1.0 : 0.0, 0.0, 0.0, 1.0); }\n";
    MGLTestTarget t;
    char log[1024] = { 0 };
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log), vbo = 0, vao;
    unsigned char *px, c[4] = { 0 };

    if (!prog || !mgl_target_create(&t, 4, 4, GL_RGBA8, 0)) { CHECK_MSG(0, "%s", log); return; }

    mgl_target_bind(&t);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    vao = mgl_fullscreen_quad(&vbo);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    px = mgl_read_rgba8(&t);
    if (px)
        mgl_pixel_at(px, &t, 2, 2, c);
    free(px);
    CHECK_MSG(c[0] == 255, "drew %d, want 255", c[0]);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glUseProgram(0);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// a three-row matrix beside a sampler in a struct, set and drawn
GPU_TEST(uniform_query, three_row_matrices_in_a_struct_with_a_sampler)
{
    static const char *VS =
        "#version 330 core\n"
        "layout(location = 0) in vec2 pos;\n"
        "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";
    static const char *FS =
        "#version 330 core\n"
        "struct S { mat2x3 a; sampler2D s; mat3 b; };\n"
        "uniform S u;\n"
        "out vec4 o;\n"
        "void main() { o = vec4(u.a[1][2] == 6.0 && u.b[2][1] == 8.0 ? 1.0 : 0.0, 0.0, 0.0, 1.0)\n"
        "                  + texture(u.s, vec2(0.5)) * 0.0; }\n";
    static const GLfloat a[6] = { 1, 2, 3, 4, 5, 6 }, b[9] = { 0, 0, 0, 0, 0, 0, 0, 8, 0 };
    MGLTestTarget t;
    char log[1024] = { 0 };
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log), vbo = 0, vao;
    GLfloat back[9] = { 0 };
    unsigned char *px, c[4] = { 0 };

    if (!prog || !mgl_target_create(&t, 4, 4, GL_RGBA8, 0)) { CHECK_MSG(0, "%s", log); return; }

    mgl_target_bind(&t);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    glUniformMatrix2x3fv(glGetUniformLocation(prog, "u.a"), 1, GL_FALSE, a);
    glUniformMatrix3fv(glGetUniformLocation(prog, "u.b"), 1, GL_FALSE, b);
    glGetUniformfv(prog, glGetUniformLocation(prog, "u.a"), back);
    CHECK(back[5] == 6.0f);
    vao = mgl_fullscreen_quad(&vbo);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    px = mgl_read_rgba8(&t);
    if (px)
        mgl_pixel_at(px, &t, 2, 2, c);
    free(px);
    CHECK_MSG(c[0] == 255, "drew %d, want 255", c[0]);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glUseProgram(0);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}
