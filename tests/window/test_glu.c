/*
 * test_glu.c
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
 * GLU from libGLU, drawing into a CGL context: matrices, quadrics, the
 * tessellator, NURBS and mipmaps. Built with Apple's headers, the way Mac
 * code includes GLU, and linked to libGLU and libmoogl alone.
 */

#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

static int fails;
static GLint vp[4];

static void check(int ok, const char *what)
{
    printf("  [%s] %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

static int near(GLubyte a, int b) { return a + 3 >= b && a <= b + 3; }

// at a fraction of the viewport's width and height
static int pixelIs(double fx, double fy, int r, int g, int b)
{
    GLubyte p[4];

    glReadPixels((GLint)(fx * vp[2]), (GLint)(fy * vp[3]), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
    return near(p[0], r) && near(p[1], g) && near(p[2], b);
}

static void clearRed(void)
{
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(0, 1, 0);
}

static void matrices(void)
{
    GLdouble m[16], ident[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    GLdouble x, y, z;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(90, 1, 1, 10);
    glGetDoublev(GL_PROJECTION_MATRIX, m);
    check(fabs(m[0] - 1) < 1e-6 && fabs(m[10] + 11.0 / 9) < 1e-6 && m[11] == -1 && fabs(m[14] + 20.0 / 9) < 1e-6,
          "gluPerspective");
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0, 0, 5, 0, 0, 0, 0, 1, 0);
    glGetDoublev(GL_MODELVIEW_MATRIX, m);
    check(m[0] == 1 && m[5] == 1 && m[10] == 1 && m[14] == -5, "gluLookAt");
    glLoadIdentity();

    gluProject(0, 0, 0, ident, ident, vp, &x, &y, &z);
    check(x == vp[0] + vp[2] / 2.0 && y == vp[1] + vp[3] / 2.0, "gluProject");
    check(strcmp((const char *)gluErrorString(GLU_INVALID_ENUM), "invalid enumerant") == 0, "gluErrorString");
}

static void quadrics(void)
{
    GLUquadric *q = gluNewQuadric();

    clearRed();
    gluSphere(q, 0.5, 16, 16);
    check(pixelIs(0.5, 0.5, 0, 255, 0) && pixelIs(0.05, 0.05, 255, 0, 0), "gluSphere");

    clearRed();
    gluQuadricDrawStyle(q, GLU_FILL);
    gluDisk(q, 0.25, 0.5, 32, 1);
    check(pixelIs(0.5, 0.5, 255, 0, 0) && pixelIs(0.5, 0.6875, 0, 255, 0), "gluDisk leaves its hole");

    gluDeleteQuadric(q);
}

static void tessellator(void)
{
    // an L, concave: its top right corner stays empty
    GLdouble l[6][3] = { { -0.8, -0.8, 0 }, { 0.8, -0.8, 0 }, { 0.8, 0, 0 }, { 0, 0, 0 }, { 0, 0.8, 0 }, { -0.8, 0.8, 0 } };
    GLUtesselator *t = gluNewTess();

    gluTessCallback(t, GLU_TESS_BEGIN, (GLvoid (*)())glBegin);
    gluTessCallback(t, GLU_TESS_VERTEX, (GLvoid (*)())glVertex3dv);
    gluTessCallback(t, GLU_TESS_END, (GLvoid (*)())glEnd);

    clearRed();
    gluTessBeginPolygon(t, NULL);
    gluTessBeginContour(t);
    for (int i = 0; i < 6; i++)
        gluTessVertex(t, l[i], l[i]);
    gluTessEndContour(t);
    gluTessEndPolygon(t);

    check(pixelIs(0.25, 0.25, 0, 255, 0) && pixelIs(0.25, 0.75, 0, 255, 0) && pixelIs(0.75, 0.25, 0, 255, 0) &&
          pixelIs(0.75, 0.75, 255, 0, 0), "the tessellator fills a concave polygon");

    gluDeleteTess(t);
}

static void nurbs(void)
{
    // a flat bicubic surface over the left half
    GLfloat knots[8] = { 0, 0, 0, 0, 1, 1, 1, 1 };
    GLfloat pts[4][4][3];
    GLUnurbs *n = gluNewNurbsRenderer();

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
        {
            pts[i][j][0] = -1 + i / 3.0f;
            pts[i][j][1] = -1 + 2 * j / 3.0f;
            pts[i][j][2] = 0;
        }

    clearRed();
    gluNurbsProperty(n, GLU_SAMPLING_TOLERANCE, 25);
    gluBeginSurface(n);
    gluNurbsSurface(n, 8, knots, 8, knots, 4 * 3, 3, &pts[0][0][0], 4, 4, GL_MAP2_VERTEX_3);
    gluEndSurface(n);

    check(pixelIs(0.25, 0.5, 0, 255, 0) && pixelIs(0.75, 0.5, 255, 0, 0) && glGetError() == GL_NO_ERROR,
          "gluNurbsSurface");

    gluDeleteNurbsRenderer(n);
}

static void mipmaps(void)
{
    GLubyte img[8 * 8 * 4];
    GLint w = 0;
    GLuint tex;

    memset(img, 200, sizeof img);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    check(gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, img) == 0,
          "gluBuild2DMipmaps");
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 3, GL_TEXTURE_WIDTH, &w);
    check(w == 1, "down to a 1x1 level");
    glDeleteTextures(1, &tex);
}

int main(void)
{
    CGLPixelFormatAttribute attribs[] = {
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_Legacy, (CGLPixelFormatAttribute)0
    };
    CGLPixelFormatObj pix = NULL;
    CGLContextObj ctx = NULL;
    GLint npix = 0;

    if (CGLChoosePixelFormat(attribs, &pix, &npix) != kCGLNoError || CGLCreateContext(pix, NULL, &ctx) != kCGLNoError)
    {
        printf("no context\nFAILED\n");
        return 1;
    }

    CGLSetCurrentContext(ctx);
    glGetIntegerv(GL_VIEWPORT, vp);
    printf("GLU %s\n", (const char *)gluGetString(GLU_VERSION));

    matrices();
    quadrics();
    tessellator();
    nurbs();
    mipmaps();

    CGLSetCurrentContext(NULL);
    CGLDestroyContext(ctx);
    CGLDestroyPixelFormat(pix);

    printf("%s\n", fails ? "FAILED" : "passed");
    return fails ? 1 : 0;
}
