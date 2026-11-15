/*
 * Copyright (C) The MooGL Project
 *
 * test_compat_profile.c
 * MGL
 *
 * A compatibility profile context: immediate mode, the matrix stacks,
 * client arrays, quads, fixed-function lighting and texturing, display
 * lists, selection and the raster operations. A core context keeps
 * refusing all of it.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "harness.h"
#include "MGLContext.h"
#include "legacy_gl.h"

#define W 32
#define H 32

extern void *CppCreateMGLRendererHeadless(void *glm_ctx);

static GLMContext was;

static GLMContext openCompat(MGLTestTarget *t)
{
    GLMContext c;

    was = MGLgetCurrentContext();
    c = createGLMContextProfile(GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, GL_DEPTH_COMPONENT, GL_FLOAT,
                                GL_STENCIL_INDEX8, GL_UNSIGNED_BYTE, NULL, GL_CONTEXT_COMPATIBILITY_PROFILE_BIT);
    if (c == NULL)
        return NULL;

    MGLsetCurrentContext(c);
    CppCreateMGLRendererHeadless(c);

    if (!mgl_target_create(t, W, H, GL_RGBA8, 1))
    {
        MGLsetCurrentContext(was);
        destroyGLMContext(c);
        return NULL;
    }

    glViewport(0, 0, W, H);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    return c;
}

static void closeCompat(GLMContext c, MGLTestTarget *t)
{
    mgl_target_destroy(t);
    MGLsetCurrentContext(was);
    destroyGLMContext(c);
}

static void pixel(const MGLTestTarget *t, int x, int y, unsigned char out[4])
{
    unsigned char *px = mgl_read_rgba8(t);

    memset(out, 0, 4);
    if (px)
        mgl_pixel_at(px, t, x, y, out);
    free(px);
}

#define NEAR(a, b) (abs((int)(a) - (int)(b)) <= 3)
#define IS(p, r, g, b) (NEAR((p)[0], r) && NEAR((p)[1], g) && NEAR((p)[2], b))

GPU_TEST(compat_profile, core_context_refuses_legacy_calls)
{
    mgl_drain_errors();
    glBegin(GL_TRIANGLES);
    CHECK_EQ_UINT(glGetError(), GL_INVALID_OPERATION);
    glMatrixMode(GL_MODELVIEW);
    CHECK_EQ_UINT(glGetError(), GL_INVALID_OPERATION);
}

GPU_TEST(compat_profile, immediate_mode_draws_a_coloured_triangle)
{
    MGLTestTarget t;
    unsigned char p[4];
    GLMContext c = openCompat(&t);
    GLint mask = 0;

    CHECK(c != NULL);
    if (!c)
        return;

    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &mask);
    CHECK_EQ_INT(mask, GL_CONTEXT_COMPATIBILITY_PROFILE_BIT);

    glBegin(GL_TRIANGLES);
    glColor3f(1, 0, 0);
    glVertex2f(-1, -1);
    glVertex2f(3, -1);
    glVertex2f(-1, 3);
    glEnd();

    pixel(&t, W / 2, H / 2, p);
    CHECK_MSG(IS(p, 255, 0, 0), "centre %d %d %d", p[0], p[1], p[2]);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    closeCompat(c, &t);
}

GPU_TEST(compat_profile, matrix_stack_places_geometry)
{
    MGLTestTarget t;
    unsigned char in[4], out[4], back[4];
    GLMContext c = openCompat(&t);
    GLint depth = 0;

    CHECK(c != NULL);
    if (!c)
        return;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, W, 0, H, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glPushMatrix();
    glTranslatef(16, 0, 0);
    glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, &depth);
    CHECK_EQ_INT(depth, 2);
    glColor3f(0, 1, 0);
    // the right half only, once translated
    glRectf(0, 0, 16, 32);
    glPopMatrix();

    glColor3f(0, 0, 1);
    // back at the origin, a strip along the bottom-left
    glRectf(0, 0, 4, 4);

    pixel(&t, 24, 16, in);
    pixel(&t, 8, 16, out);
    pixel(&t, 1, 1, back);
    CHECK_MSG(IS(in, 0, 255, 0), "right half %d %d %d", in[0], in[1], in[2]);
    CHECK_MSG(IS(out, 0, 0, 0), "left half %d %d %d", out[0], out[1], out[2]);
    CHECK_MSG(IS(back, 0, 0, 255), "corner %d %d %d", back[0], back[1], back[2]);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    closeCompat(c, &t);
}

GPU_TEST(compat_profile, client_arrays_and_quads_draw)
{
    MGLTestTarget t;
    unsigned char a[4], b[4];
    GLMContext c = openCompat(&t);
    static const GLfloat verts[] = { -1, -1,  0, -1,  0, 1,  -1, 1 };
    static const GLubyte colors[] = { 255, 255, 0, 255,  255, 255, 0, 255,  255, 255, 0, 255,  255, 255, 0, 255 };
    static const GLushort idx[] = { 0, 1, 2, 3 };

    CHECK(c != NULL);
    if (!c)
        return;

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, verts);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
    glDrawArrays(GL_QUADS, 0, 4);

    pixel(&t, 8, 16, a);
    CHECK_MSG(IS(a, 255, 255, 0), "left quad %d %d %d", a[0], a[1], a[2]);

    // the same quad again, mirrored, through client indices
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(-1, 1, 1);
    glDisableClientState(GL_COLOR_ARRAY);
    glColor3f(1, 0, 1);
    glDrawElements(GL_QUADS, 4, GL_UNSIGNED_SHORT, idx);

    pixel(&t, 24, 16, b);
    CHECK_MSG(IS(b, 255, 0, 255), "right quad %d %d %d", b[0], b[1], b[2]);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    closeCompat(c, &t);
}


// a 2x2 texture: red, green / blue, white
static GLuint checker(void)
{
    static const GLubyte px[16] = { 255,0,0,255,  0,255,0,255,  0,0,255,255,  255,255,255,255 };
    GLuint tex;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return tex;
}

static void texturedQuad(void)
{
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1, -1);
    glTexCoord2f(1, 0); glVertex2f(1, -1);
    glTexCoord2f(1, 1); glVertex2f(1, 1);
    glTexCoord2f(0, 1); glVertex2f(-1, 1);
    glEnd();
}

GPU_TEST(compat_profile, lighting_shades_by_the_normal)
{
    MGLTestTarget t;
    unsigned char lit[4], dark[4];
    GLMContext c = openCompat(&t);
    static const GLfloat dir[4] = { 0, 0, 1, 0 };

    CHECK(c != NULL);
    if (!c)
        return;

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, dir);

    // facing the light: diffuse 0.8 plus ambient 0.2 x 0.2
    glNormal3f(0, 0, 1);
    glRectf(-1, -1, 0, 1);
    // facing away: the ambient term alone
    glNormal3f(0, 0, -1);
    glRectf(0, -1, 1, 1);

    pixel(&t, 8, 16, lit);
    pixel(&t, 24, 16, dark);
    CHECK_MSG(NEAR(lit[0], 214) && NEAR(lit[1], 214) && NEAR(lit[2], 214), "lit %d %d %d", lit[0], lit[1], lit[2]);
    CHECK_MSG(NEAR(dark[0], 10) && NEAR(dark[2], 10), "unlit %d %d %d", dark[0], dark[1], dark[2]);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    closeCompat(c, &t);
}

GPU_TEST(compat_profile, texture_environment_combines)
{
    MGLTestTarget t;
    unsigned char mod[4], rep[4], lum[4];
    GLMContext c = openCompat(&t);
    static const GLubyte grey[1] = { 128 };
    GLuint tex, ltex;

    CHECK(c != NULL);
    if (!c)
        return;

    tex = checker();
    glEnable(GL_TEXTURE_2D);

    // modulate: the white texel times a half-grey vertex colour
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor3f(0.5f, 0.5f, 0.5f);
    texturedQuad();
    pixel(&t, 24, 24, mod);
    CHECK_MSG(NEAR(mod[0], 128) && NEAR(mod[1], 128) && NEAR(mod[2], 128), "modulate %d %d %d", mod[0], mod[1], mod[2]);

    // replace: the texel alone
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    texturedQuad();
    pixel(&t, 8, 8, rep);
    CHECK_MSG(IS(rep, 255, 0, 0), "replace %d %d %d", rep[0], rep[1], rep[2]);

    // a luminance texture reads the same in every colour channel
    glGenTextures(1, &ltex);
    glBindTexture(GL_TEXTURE_2D, ltex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 1, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, grey);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    texturedQuad();
    pixel(&t, 16, 16, lum);
    CHECK_MSG(IS(lum, 128, 128, 128), "luminance %d %d %d", lum[0], lum[1], lum[2]);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    glDeleteTextures(1, &tex);
    glDeleteTextures(1, &ltex);
    closeCompat(c, &t);
}

// SFML leaves GL_TEXTURE_2D on with nothing bound
GPU_TEST(compat_profile, an_incomplete_texture_leaves_the_vertex_colour)
{
    MGLTestTarget t;
    unsigned char none[4], mip[4];
    GLMContext c = openCompat(&t);
    static const GLubyte white[4] = { 255, 255, 255, 255 };
    GLuint tex;

    CHECK(c != NULL);
    if (!c)
        return;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glColor3f(0, 1, 0);
    texturedQuad();
    pixel(&t, 16, 16, none);
    CHECK_MSG(IS(none, 0, 255, 0), "no texture %d %d %d", none[0], none[1], none[2]);

    // a mipmapped filter with only level 0 is incomplete too
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glColor3f(0, 0, 1);
    texturedQuad();
    pixel(&t, 16, 16, mip);
    CHECK_MSG(IS(mip, 0, 0, 255), "incomplete %d %d %d", mip[0], mip[1], mip[2]);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    glDeleteTextures(1, &tex);
    closeCompat(c, &t);
}

GPU_TEST(compat_profile, fog_blends_toward_its_colour)
{
    MGLTestTarget t;
    unsigned char p[4];
    GLMContext c = openCompat(&t);
    static const GLfloat blue[4] = { 0, 0, 1, 1 };

    CHECK(c != NULL);
    if (!c)
        return;

    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 0.0f);
    glFogf(GL_FOG_END, 2.0f);
    glFogfv(GL_FOG_COLOR, blue);

    // eye depth 1 is half way through the fog
    glMatrixMode(GL_MODELVIEW);
    glTranslatef(0, 0, -1);
    glColor3f(1, 0, 0);
    glRectf(-1, -1, 1, 1);

    pixel(&t, 16, 16, p);
    CHECK_MSG(NEAR(p[0], 128) && NEAR(p[1], 0) && NEAR(p[2], 128), "fogged %d %d %d", p[0], p[1], p[2]);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    closeCompat(c, &t);
}

GPU_TEST(compat_profile, push_and_pop_attrib_restore_state)
{
    MGLTestTarget t;
    GLMContext c = openCompat(&t);
    GLfloat color[4];
    GLint mode = 0;

    CHECK(c != NULL);
    if (!c)
        return;

    glColor3f(1, 0, 0);
    glShadeModel(GL_SMOOTH);
    glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT | GL_LIGHTING_BIT);
    glColor3f(0, 1, 0);
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_FLAT);
    glPopAttrib();

    glGetFloatv(GL_CURRENT_COLOR, color);
    glGetIntegerv(GL_SHADE_MODEL, &mode);
    CHECK(color[0] == 1.0f && color[1] == 0.0f);
    CHECK(!glIsEnabled(GL_LIGHTING));
    CHECK(!glIsEnabled(GL_DEPTH_TEST));
    CHECK_EQ_INT(mode, GL_SMOOTH);

    // a group that was not pushed keeps its new value
    glPushAttrib(GL_CURRENT_BIT);
    glEnable(GL_LIGHTING);
    glPopAttrib();
    CHECK(glIsEnabled(GL_LIGHTING));

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPopAttrib();
    glPopAttrib();
    CHECK_EQ_UINT(glGetError(), GL_STACK_UNDERFLOW);

    closeCompat(c, &t);
}

GPU_TEST(compat_profile, front_and_back_rasterise_differently)
{
    MGLTestTarget t;
    unsigned char front_mid[4], back_mid[4], back_edge[4];
    GLMContext c = openCompat(&t);

    CHECK(c != NULL);
    if (!c)
        return;

    glPolygonMode(GL_FRONT, GL_FILL);
    glPolygonMode(GL_BACK, GL_LINE);
    glColor3f(1, 1, 1);

    // counter-clockwise, so front facing: filled
    glBegin(GL_QUADS);
    glVertex2f(-1, -1); glVertex2f(-0.1f, -1); glVertex2f(-0.1f, 1); glVertex2f(-1, 1);
    glEnd();
    // clockwise, so back facing: an outline
    glBegin(GL_QUADS);
    glVertex2f(0.1f, -0.9f); glVertex2f(0.1f, 0.9f); glVertex2f(0.9f, 0.9f); glVertex2f(0.9f, -0.9f);
    glEnd();

    pixel(&t, 8, 16, front_mid);
    pixel(&t, 24, 16, back_mid);
    pixel(&t, 17, 16, back_edge);
    CHECK_MSG(IS(front_mid, 255, 255, 255), "front %d", front_mid[0]);
    CHECK_MSG(IS(back_mid, 0, 0, 0), "back interior %d", back_mid[0]);
    CHECK_MSG(back_edge[0] > 0, "back edge %d", back_edge[0]);

    glEdgeFlag(GL_FALSE);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    closeCompat(c, &t);
}

GPU_TEST(compat_profile, display_lists_record_and_replay)
{
    MGLTestTarget t;
    unsigned char a[4], b[4], none[4];
    GLMContext c = openCompat(&t);
    GLuint base, nested;
    GLubyte order[2] = { 0, 1 };

    CHECK(c != NULL);
    if (!c)
        return;

    glMatrixMode(GL_PROJECTION);
    glOrtho(0, W, 0, H, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    base = glGenLists(2);
    CHECK(base != 0 && glIsList(base) && glIsList(base + 1));

    // a red square, recorded and not drawn
    glNewList(base, GL_COMPILE);
    glColor3f(1, 0, 0);
    glRectf(0, 0, 8, 8);
    glEndList();

    // a translate, then the first list again
    glNewList(base + 1, GL_COMPILE);
    glTranslatef(16, 16, 0);
    glCallList(base);
    glEndList();

    pixel(&t, 4, 4, none);
    CHECK_MSG(IS(none, 0, 0, 0), "compiled only %d", none[0]);

    glColor3f(0, 1, 0);
    glListBase(base);
    glCallLists(2, GL_UNSIGNED_BYTE, order);

    pixel(&t, 4, 4, a);
    pixel(&t, 20, 20, b);
    CHECK_MSG(IS(a, 255, 0, 0), "first %d %d %d", a[0], a[1], a[2]);
    CHECK_MSG(IS(b, 255, 0, 0), "nested %d %d %d", b[0], b[1], b[2]);

    // compile and execute draws as it records
    nested = glGenLists(1);
    glLoadIdentity();
    glNewList(nested, GL_COMPILE_AND_EXECUTE);
    glColor3f(0, 0, 1);
    glRectf(24, 0, 32, 8);
    glEndList();
    pixel(&t, 28, 4, a);
    CHECK_MSG(IS(a, 0, 0, 255), "compile and execute %d %d %d", a[0], a[1], a[2]);

    glDeleteLists(base, 2);
    CHECK(!glIsList(base));
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    closeCompat(c, &t);
}

GPU_TEST(compat_profile, selection_reports_what_is_hit)
{
    MGLTestTarget t;
    GLMContext c = openCompat(&t);
    GLuint buf[64];
    GLint hits;

    CHECK(c != NULL);
    if (!c)
        return;

    glSelectBuffer(64, buf);
    glRenderMode(GL_SELECT);
    glInitNames();
    glPushName(0);

    // the view volume covers x in [-1, 0] only
    glMatrixMode(GL_PROJECTION);
    glOrtho(-1, 0, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    glLoadName(7);
    glRectf(-0.9f, -0.5f, -0.5f, 0.5f);
    glLoadName(9);
    glRectf(0.5f, -0.5f, 0.9f, 0.5f);

    hits = glRenderMode(GL_RENDER);
    CHECK_EQ_INT(hits, 1);
    if (hits == 1)
    {
        CHECK_EQ_UINT(buf[0], 1);
        CHECK_EQ_UINT(buf[3], 7);
    }

    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);
    closeCompat(c, &t);
}

GPU_TEST(compat_profile, raster_operations_reach_the_framebuffer)
{
    MGLTestTarget t;
    unsigned char drawn[4], bit_on[4], bit_off[4], copied[4];
    GLMContext c = openCompat(&t);
    GLubyte red[4 * 4 * 4];
    // an 8x8 glyph, its left half lit
    static const GLubyte glyph[8] = { 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0 };
    GLfloat pos[4];

    CHECK(c != NULL);
    if (!c)
        return;

    for (int i = 0; i < 16; i++)
    {
        red[i * 4] = 255; red[i * 4 + 1] = 0; red[i * 4 + 2] = 0; red[i * 4 + 3] = 255;
    }

    // an array buffer the application left bound has nothing to do with it
    {
        GLuint vbo;
        static const GLfloat junk[16] = { 0 };

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof junk, junk, GL_STATIC_DRAW);
    }

    glWindowPos2i(2, 2);
    glDrawPixels(4, 4, GL_RGBA, GL_UNSIGNED_BYTE, red);
    pixel(&t, 3, 3, drawn);
    CHECK_MSG(IS(drawn, 255, 0, 0), "drawn %d %d %d", drawn[0], drawn[1], drawn[2]);

    // the bitmap takes the colour current at glRasterPos time, and moves it on
    glColor3f(0, 1, 0);
    glWindowPos2i(16, 16);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBitmap(8, 8, 0, 0, 10, 0, glyph);
    pixel(&t, 17, 17, bit_on);
    pixel(&t, 22, 17, bit_off);
    CHECK_MSG(IS(bit_on, 0, 255, 0), "bitmap on %d %d %d", bit_on[0], bit_on[1], bit_on[2]);
    CHECK_MSG(IS(bit_off, 0, 0, 0), "bitmap off %d %d %d", bit_off[0], bit_off[1], bit_off[2]);
    glGetFloatv(GL_CURRENT_RASTER_POSITION, pos);
    CHECK(pos[0] == 26.0f && pos[1] == 16.0f);

    // the red block copied up and to the right
    glWindowPos2i(24, 2);
    glCopyPixels(2, 2, 4, 4, GL_COLOR);
    pixel(&t, 25, 3, copied);
    CHECK_MSG(IS(copied, 255, 0, 0), "copied %d %d %d", copied[0], copied[1], copied[2]);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    closeCompat(c, &t);
}

GPU_TEST(compat_profile, point_sprites_size_and_texture)
{
    MGLTestTarget t;
    unsigned char low[4], high[4], outside[4];
    GLMContext c = openCompat(&t);
    GLuint tex;

    CHECK(c != NULL);
    if (!c)
        return;

    tex = checker();
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glEnable(GL_POINT_SPRITE);
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
    glPointSize(16.0f);

    glBegin(GL_POINTS);
    glVertex2f(0, 0);
    glEnd();

    // the sprite's coordinates run top to bottom, so its top row is texel row 0
    pixel(&t, 12, 20, high);
    pixel(&t, 12, 12, low);
    pixel(&t, 2, 2, outside);
    CHECK_MSG(IS(high, 255, 0, 0), "top left %d %d %d", high[0], high[1], high[2]);
    CHECK_MSG(IS(low, 0, 0, 255), "bottom left %d %d %d", low[0], low[1], low[2]);
    CHECK_MSG(IS(outside, 0, 0, 0), "outside %d %d %d", outside[0], outside[1], outside[2]);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    glDeleteTextures(1, &tex);
    closeCompat(c, &t);
}

GPU_TEST(compat_profile, multitexture_is_advertised)
{
    MGLTestTarget t;
    GLMContext c = openCompat(&t);
    const char *ext;
    GLint units = 0;

    CHECK(c != NULL);
    if (!c)
        return;

    ext = (const char *)glGetString(GL_EXTENSIONS);
    CHECK(ext && strstr(ext, "GL_ARB_multitexture "));
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &units);
    CHECK(units >= 2);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    closeCompat(c, &t);
}

// what SFML checks before it uses shaders, and the calls it then makes
GPU_TEST(compat_profile, arb_shader_objects_build_and_draw)
{
    MGLTestTarget t;
    unsigned char px[4];
    GLMContext c = openCompat(&t);
    const char *vs = "void main() { gl_Position = gl_Vertex; }";
    const char *fs = "uniform vec4 colour; void main() { gl_FragColor = colour; }";
    GLint ok = 0, type = 0;
    GLuint buf = 0;

    CHECK(c != NULL);
    if (!c)
        return;

    const char *ext = (const char *)glGetString(GL_EXTENSIONS);
    CHECK(ext && strstr(ext, "GL_ARB_shader_objects ") && strstr(ext, "GL_ARB_vertex_shader ") &&
          strstr(ext, "GL_ARB_fragment_shader ") && strstr(ext, "GL_ARB_shading_language_100 "));

    GLhandleARB v = glCreateShaderObjectARB(GL_VERTEX_SHADER);
    GLhandleARB f = glCreateShaderObjectARB(GL_FRAGMENT_SHADER);
    glShaderSourceARB(v, 1, &vs, NULL);
    glShaderSourceARB(f, 1, &fs, NULL);
    glCompileShaderARB(v);
    glCompileShaderARB(f);
    glGetObjectParameterivARB(f, 0x8B81 /* GL_OBJECT_COMPILE_STATUS_ARB */, &ok);
    CHECK(ok == GL_TRUE);
    glGetObjectParameterivARB(f, 0x8B4E /* GL_OBJECT_TYPE_ARB */, &type);
    CHECK_EQ_UINT((GLuint)type, 0x8B48 /* GL_SHADER_OBJECT_ARB */);

    GLhandleARB p = glCreateProgramObjectARB();
    glAttachObjectARB(p, v);
    glAttachObjectARB(p, f);
    glLinkProgramARB(p);
    glGetObjectParameterivARB(p, 0x8B82 /* GL_OBJECT_LINK_STATUS_ARB */, &ok);
    CHECK(ok == GL_TRUE);

    glUseProgramObjectARB(p);
    CHECK(glGetHandleARB(0x8B40 /* GL_PROGRAM_OBJECT_ARB */) == p);
    glUniform4fARB(glGetUniformLocationARB(p, "colour"), 1, 0, 1, 1);
    glRectf(-1, -1, 1, 1);
    pixel(&t, 16, 16, px);
    CHECK_MSG(IS(px, 255, 0, 255), "drawn %d %d %d", px[0], px[1], px[2]);

    glGenBuffersARB(1, &buf);
    glBindBufferARB(GL_ARRAY_BUFFER, buf);
    CHECK(glIsBuffer(buf));

    glUseProgramObjectARB(NULL);
    glDeleteObjectARB(p);
    glDeleteObjectARB(v);
    glDeleteObjectARB(f);
    glDeleteBuffers(1, &buf);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    closeCompat(c, &t);
}

GPU_TEST(compat_profile, old_glsl_reads_the_fixed_function_state)
{
    MGLTestTarget t;
    unsigned char left[4], right[4];
    GLMContext c = openCompat(&t);
    // the shape of SFML's and many GLUT programs' shaders
    static const char *vs =
        "void main() {\n"
        "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
        "    gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;\n"
        "    gl_FrontColor = gl_Color;\n"
        "}\n";
    static const char *fs =
        "#version 120\n"
        "uniform sampler2D texture;\n"
        "varying vec4 unused;\n"
        "void main() {\n"
        "    gl_FragColor = gl_Color * texture2D(texture, gl_TexCoord[0].xy);\n"
        "}\n";
    char log[1024] = "";
    GLuint tex, prog;

    CHECK(c != NULL);
    if (!c)
        return;

    prog = mgl_build_program(vs, fs, log, sizeof(log));
    CHECK_MSG(prog != 0, "build: %s", log);
    if (!prog)
    {
        closeCompat(c, &t);
        return;
    }

    tex = checker();
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "texture"), 0);

    // only the left column of texels, through the texture matrix
    glMatrixMode(GL_TEXTURE);
    glScalef(0.5f, 1.0f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glOrtho(0, 2, 0, 2, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(0, 0);
    glTexCoord2f(1, 0); glVertex2f(2, 0);
    glTexCoord2f(1, 1); glVertex2f(2, 2);
    glTexCoord2f(0, 1); glVertex2f(0, 2);
    glEnd();

    // texel 0 of the bottom row is red; the scaled coordinate never reaches texel 1
    pixel(&t, 4, 4, left);
    pixel(&t, 28, 4, right);
    CHECK_MSG(IS(left, 255, 0, 0), "left %d %d %d", left[0], left[1], left[2]);
    CHECK_MSG(IS(right, 255, 0, 0), "right %d %d %d", right[0], right[1], right[2]);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteTextures(1, &tex);
    closeCompat(c, &t);
}

static void twoQuads(void)
{
    // green on the left, the same value given for every vertex
    glBegin(GL_QUADS);
    glColor3f(0, 1, 0); glVertex2f(-1, -1);
    glColor3f(0, 1, 0); glVertex2f(0, -1);
    glColor3f(0, 1, 0); glVertex2f(0, 1);
    glColor3f(0, 1, 0); glVertex2f(-1, 1);
    glEnd();

    // blue on the right, its alpha different on each vertex
    glBegin(GL_QUADS);
    glColor4f(0, 0, 1, 1.0f);  glVertex2f(0, -1);
    glColor4f(0, 0, 1, 0.9f);  glVertex2f(1, -1);
    glColor4f(0, 0, 1, 0.8f);  glVertex2f(1, 1);
    glColor4f(0, 0, 1, 0.7f);  glVertex2f(0, 1);
    glEnd();
}

// Each glEnd swapped in its own program and vertex array and back again, and
// every glColor marked all state dirty, so every draw rebuilt the Metal
// pipeline descriptor to look its pipeline up.
GPU_TEST(compat_profile, glbegin_draws_reuse_the_pipeline)
{
    MGLTestTarget t;
    unsigned char left[4], right[4];
    GLMContext c = openCompat(&t);
    GLuint before = 0, after = 0;

    CHECK(c != NULL);
    if (!c)
        return;

    twoQuads();

    MGLget(NULL, MGL_STAT_PIPELINE_LOOKUPS, &before);
    for (int i = 0; i < 10; i++)
        twoQuads();
    MGLget(NULL, MGL_STAT_PIPELINE_LOOKUPS, &after);

    CHECK_EQ_UINT(after - before, 0);

    pixel(&t, 8, 16, left);
    pixel(&t, 24, 16, right);
    CHECK_MSG(IS(left, 0, 255, 0), "left %d %d %d", left[0], left[1], left[2]);
    CHECK_MSG(IS(right, 0, 0, 255), "right %d %d %d", right[0], right[1], right[2]);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    closeCompat(c, &t);
}

// Half-Life's masked textures: texels with alpha 0 are dropped by the alpha
// test, depth and all, so what is behind shows through and a later
// GL_EQUAL pass does not land there.
GPU_TEST(compat_profile, alpha_test_drops_clear_texels)
{
    static const GLubyte px[8] = { 0, 255, 0, 0,  0, 255, 0, 255 };
    MGLTestTarget t;
    unsigned char left[4], right[4];
    GLMContext c = openCompat(&t);
    GLuint tex;

    CHECK(c != NULL);
    if (!c)
        return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.25f);
    texturedQuad();

    // the lightmap pass after it: only where the first pass left its depth,
    // multiplying by black
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
    glDepthFunc(GL_EQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
    glColor3f(0, 0, 0);
    texturedQuad();
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_DEPTH_TEST);

    pixel(&t, 8, 16, left);
    pixel(&t, 24, 16, right);
    CHECK_MSG(IS(left, 255, 0, 0), "clear half %d %d %d", left[0], left[1], left[2]);
    CHECK_MSG(IS(right, 0, 0, 0), "solid half, after the second pass, %d %d %d", right[0], right[1], right[2]);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &tex);
    closeCompat(c, &t);
}

// An untextured draw, then an alpha-tested textured one, through the same
// immediate-mode vertex array: the second program's uniforms, which hold the
// alpha test, have to be bound for it. Done twice, as the first time the
// vertex layout is still settling.
GPU_TEST(compat_profile, alpha_test_after_another_program)
{
    static const GLubyte px[8] = { 0, 255, 0, 0,  0, 255, 0, 255 };
    MGLTestTarget t;
    unsigned char left[4], right[4];
    GLMContext c = openCompat(&t);
    GLuint tex;

    CHECK(c != NULL);
    if (!c)
        return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glAlphaFunc(GL_GREATER, 0.25f);

    for (int pass = 0; pass < 2; pass++)
    {
        glClearColor(1, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        glDisable(GL_TEXTURE_2D);
        glDisable(GL_ALPHA_TEST);
        glColor3f(0, 0, 1);
        texturedQuad();

        glEnable(GL_TEXTURE_2D);
        glEnable(GL_ALPHA_TEST);
        glColor3f(1, 1, 1);
        texturedQuad();
    }

    pixel(&t, 8, 16, left);
    pixel(&t, 24, 16, right);
    CHECK_MSG(IS(left, 0, 0, 255), "clear half %d %d %d", left[0], left[1], left[2]);
    CHECK_MSG(IS(right, 0, 255, 0), "solid half %d %d %d", right[0], right[1], right[2]);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &tex);
    closeCompat(c, &t);
}

// Client vertices and client indices from one draw both go into the driver's
// stream buffer. When the indices did not fit, the buffer took new storage and
// the vertices copied just before were left in the old one. Each draw here
// streams about 1.1 MB, so the fourth one runs out of room part way.
GPU_TEST(compat_profile, a_full_stream_keeps_the_whole_draw)
{
    enum { VERTS = 65536, INDICES = 150000 };
    static const GLfloat colours[6][3] = { {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0}, {0, 1, 1}, {1, 0, 1} };
    MGLTestTarget t;
    GLMContext c = openCompat(&t);
    GLfloat *verts = (GLfloat *)calloc(VERTS * 2, sizeof(GLfloat));
    GLuint *idx = (GLuint *)calloc(INDICES, sizeof(GLuint));

    CHECK(c != NULL && verts && idx);
    if (!c || !verts || !idx)
    {
        free(verts);
        free(idx);
        if (c)
            closeCompat(c, &t);
        return;
    }

    // a quad over the whole target; the last triangle only reaches the last
    // vertex, so the whole array is read
    static const GLfloat quad[8] = { -1, -1,  1, -1,  1, 1,  -1, 1 };
    static const GLuint tris[6] = { 0, 1, 2,  0, 2, 3 };
    memcpy(verts, quad, sizeof quad);
    memcpy(idx, tris, sizeof tris);
    idx[6] = idx[7] = idx[8] = VERTS - 1;

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, verts);

    for (int d = 0; d < 6; d++)
    {
        unsigned char px[4];

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glColor3f(colours[d][0], colours[d][1], colours[d][2]);
        glDrawElements(GL_TRIANGLES, INDICES, GL_UNSIGNED_INT, idx);

        pixel(&t, 16, 16, px);
        CHECK_MSG(IS(px, (int)(colours[d][0] * 255), (int)(colours[d][1] * 255), (int)(colours[d][2] * 255)),
                  "draw %d: %d %d %d", d, px[0], px[1], px[2]);
    }
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    glDisableClientState(GL_VERTEX_ARRAY);
    free(verts);
    free(idx);
    closeCompat(c, &t);
}

// QuickTime's 4:2:2 frames: a red pair of pixels and a blue pair, in both
// byte orders, come out as RGB.
GPU_TEST(compat_profile, ycbcr_422_textures_read_as_rgb)
{
    // Y, Cb, Cr: red is (81, 90, 240), blue (41, 240, 110)
    static const GLubyte vuy2[8] = { 90, 81, 240, 81,  240, 41, 110, 41 };   // Cb Y0 Cr Y1
    static const GLubyte yuvs[8] = { 81, 90, 81, 240,  41, 240, 41, 110 };   // Y0 Cb Y1 Cr
    MGLTestTarget t;
    GLMContext c = openCompat(&t);
    GLuint tex;

    CHECK(c != NULL);
    if (!c)
        return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    for (int order = 0; order < 2; order++)
    {
        unsigned char left[4], right[4];

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 4, 1, 0, GL_YCBCR_422_APPLE,
                     order ? GL_UNSIGNED_SHORT_8_8_APPLE : GL_UNSIGNED_SHORT_8_8_REV_APPLE,
                     order ? yuvs : vuy2);
        texturedQuad();

        pixel(&t, 4, 16, left);
        pixel(&t, 28, 16, right);
        CHECK_MSG(IS(left, 255, 0, 0), "order %d, red pair: %d %d %d", order, left[0], left[1], left[2]);
        CHECK_MSG(IS(right, 0, 0, 255), "order %d, blue pair: %d %d %d", order, right[0], right[1], right[2]);
    }
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &tex);
    closeCompat(c, &t);
}

// The client-storage switch and the texture range are kept and answered;
// the extensions that name them are listed.
GPU_TEST(compat_profile, apple_texture_storage_hints)
{
    static GLubyte memory[64];
    MGLTestTarget t;
    GLMContext c = openCompat(&t);
    GLboolean on = GL_FALSE;
    GLint length = 0, hint = 0;
    void *range = NULL;
    GLuint tex;

    CHECK(c != NULL);
    if (!c)
        return;

    const char *ext = (const char *)glGetString(GL_EXTENSIONS);
    CHECK(ext && strstr(ext, "GL_APPLE_client_storage") && strstr(ext, "GL_APPLE_texture_range") &&
          strstr(ext, "GL_APPLE_ycbcr_422") && strstr(ext, "GL_APPLE_packed_pixels") &&
          strstr(ext, "GL_EXT_texture_rectangle"));

    glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
    glGetBooleanv(GL_UNPACK_CLIENT_STORAGE_APPLE, &on);
    CHECK(on == GL_TRUE);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, tex);
    glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_EXT, sizeof memory, memory);
    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_SHARED_APPLE);
    glTexImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA8, 4, 4, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, memory);

    glGetTexParameteriv(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_RANGE_LENGTH_APPLE, &length);
    glGetTexParameteriv(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE, &hint);
    glGetTexParameterPointervAPPLE(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_RANGE_POINTER_APPLE, &range);
    CHECK_EQ_INT(length, (GLint)sizeof memory);
    CHECK_EQ_INT(hint, GL_STORAGE_SHARED_APPLE);
    CHECK(range == (void *)memory);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE, GL_RGBA);
    CHECK_EQ_UINT(glGetError(), GL_INVALID_ENUM);

    glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
    glDeleteTextures(1, &tex);
    closeCompat(c, &t);
}

// a flat patch over the whole viewport, z = 0, facing +z
static void fullScreenPatch(void)
{
    static const GLfloat corners[2][2][3] = {
        { { -1, -1, 0 }, { -1, 1, 0 } },
        { {  1, -1, 0 }, {  1, 1, 0 } },
    };

    glMap2f(GL_MAP2_VERTEX_3, 0, 1, 6, 2, 0, 1, 3, 2, &corners[0][0][0]);
}

GPU_TEST(compat_profile, evaluators_draw_a_bezier_patch)
{
    // red rises and falls across u: 0, 1, 0 as a quadratic, 0.5 in the middle
    static const GLfloat reds[3][4] = { { 0, 0, 0, 1 }, { 1, 0, 0, 1 }, { 0, 0, 0, 1 } };
    MGLTestTarget t;
    unsigned char mid[4], edge[4];
    GLfloat current[4];
    GLint order[2] = { 0, 0 };
    GLMContext c = openCompat(&t);

    CHECK(c != NULL);
    if (!c)
        return;

    glColor3f(0, 0, 1);
    fullScreenPatch();
    glMap2f(GL_MAP2_COLOR_4, 0, 1, 4, 3, 0, 1, 4, 1, &reds[0][0]);
    glEnable(GL_MAP2_VERTEX_3);
    glEnable(GL_MAP2_COLOR_4);
    glMapGrid2f(16, 0, 1, 16, 0, 1);
    glEvalMesh2(GL_FILL, 0, 16, 0, 16);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    pixel(&t, 16, 16, mid);
    pixel(&t, 1, 16, edge);
    CHECK_MSG(abs(mid[0] - 127) <= 6 && mid[1] == 0 && mid[2] == 0, "middle %d %d %d", mid[0], mid[1], mid[2]);
    CHECK_MSG(abs(edge[0] - 22) <= 6, "edge red %d", edge[0]);

    // the evaluated colour went with the vertices only
    glGetFloatv(GL_CURRENT_COLOR, current);
    CHECK(current[0] == 0 && current[2] == 1);

    glGetMapiv(GL_MAP2_COLOR_4, GL_ORDER, order);
    CHECK(order[0] == 3 && order[1] == 1);

    glPushAttrib(GL_EVAL_BIT);
    glDisable(GL_MAP2_VERTEX_3);
    CHECK(!glIsEnabled(GL_MAP2_VERTEX_3));
    glPopAttrib();
    CHECK(glIsEnabled(GL_MAP2_VERTEX_3));
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    glDisable(GL_MAP2_VERTEX_3);
    glDisable(GL_MAP2_COLOR_4);
    closeCompat(c, &t);
}

GPU_TEST(compat_profile, evaluators_in_a_list_use_the_map_at_call_time)
{
    static const GLfloat blue[4] = { 0, 0, 1, 1 };
    MGLTestTarget t;
    unsigned char before[4], after[4];
    GLMContext c = openCompat(&t);
    GLuint list;

    CHECK(c != NULL);
    if (!c)
        return;

    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    fullScreenPatch();

    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glEnable(GL_MAP2_VERTEX_3);
    glEnable(GL_MAP2_COLOR_4);
    glMapGrid2f(2, 0, 1, 2, 0, 1);
    glEvalMesh2(GL_FILL, 0, 2, 0, 2);
    glEndList();
    pixel(&t, 16, 16, before);

    glMap2f(GL_MAP2_COLOR_4, 0, 1, 4, 1, 0, 1, 4, 1, blue);
    glCallList(list);
    pixel(&t, 16, 16, after);

    CHECK_MSG(IS(before, 255, 0, 0), "compiling drew %d %d %d", before[0], before[1], before[2]);
    CHECK_MSG(IS(after, 0, 0, 255), "after glCallList %d %d %d", after[0], after[1], after[2]);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    glDisable(GL_MAP2_VERTEX_3);
    glDisable(GL_MAP2_COLOR_4);
    glDeleteLists(list, 1);
    closeCompat(c, &t);
}

GPU_TEST(compat_profile, auto_normal_lights_an_evaluated_patch)
{
    MGLTestTarget t;
    unsigned char lit[4];
    GLMContext c = openCompat(&t);

    CHECK(c != NULL);
    if (!c)
        return;

    // facing away, so only GL_AUTO_NORMAL's normal can light it
    glNormal3f(0, 0, -1);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_AUTO_NORMAL);
    fullScreenPatch();
    glEnable(GL_MAP2_VERTEX_3);
    glMapGrid2f(1, 0, 1, 1, 0, 1);
    glEvalMesh2(GL_FILL, 0, 1, 0, 1);

    pixel(&t, 16, 16, lit);
    CHECK_MSG(lit[0] > 180, "lit %d %d %d", lit[0], lit[1], lit[2]);

    glDisable(GL_MAP2_VERTEX_3);
    glDisable(GL_AUTO_NORMAL);
    glDisable(GL_LIGHTING);
    closeCompat(c, &t);
}
