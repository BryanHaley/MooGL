/*
 * test_glut.c
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
 * A GLUT program on MooGL's libglut (freeglut with a MooGL backend): the
 * main loop, reshape and display callbacks, the solid shapes and bitmap text.
 */

#define GL_SILENCE_DEPRECATION
#include <GL/freeglut.h>

#include <stdio.h>
#include <string.h>

static int fails, reshaped, frames, width, height;

static void check(int ok, const char *what)
{
    printf("  [%s] %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

static int near(GLubyte a, int b) { return a + 3 >= b && a <= b + 3; }

static int pixelIs(int x, int y, int r, int g, int b)
{
    GLubyte p[4];

    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
    return near(p[0], r) && near(p[1], g) && near(p[2], b);
}

static void reshape(int w, int h)
{
    // backing pixels: twice the window size on a Retina display
    reshaped = w >= 128 && w == h;
    width = w;
    height = h;
    glViewport(0, 0, w, h);
}

static void display(void)
{
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-2, 2, -2, 2, -2, 2);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // a cube on the left, a teapot on the right
    glEnable(GL_DEPTH_TEST);
    glColor3f(0, 1, 0);
    glPushMatrix();
    glTranslatef(-1, 0, 0);
    glutSolidCube(1.0);
    glPopMatrix();

    glColor3f(0, 0, 1);
    glPushMatrix();
    glTranslatef(1, 0, 0);
    glutSolidTeapot(0.5);
    glPopMatrix();
    glDisable(GL_DEPTH_TEST);

    // text along the bottom
    glColor3f(1, 1, 0);
    glRasterPos2f(-1.9f, -1.8f);
    glutBitmapString(GLUT_BITMAP_HELVETICA_18, (const unsigned char *)"MooGL");

    if (++frames == 3)
    {
        static GLubyte row[512 * 48 * 4];
        int yellow = 0;

        check(reshaped, "the reshape callback got the window size");
        check(pixelIs(width / 4, height / 2, 0, 255, 0), "glutSolidCube draws");
        check(pixelIs(3 * width / 4, height / 2, 0, 0, 255), "glutSolidTeapot draws");
        check(pixelIs(width / 2, height - 4, 255, 0, 0), "the clear colour is around them");

        int rows = height / 5 < 48 ? height / 5 : 48, cols = width < 512 ? width : 512;
        glReadPixels(0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, row);
        for (int i = 0; i < cols * rows; i++)
            yellow += row[i * 4] > 200 && row[i * 4 + 1] > 200 && row[i * 4 + 2] < 60;
        check(yellow > 20, "glutBitmapString draws text");

        check(glGetError() == GL_NO_ERROR, "no GL errors");
        glutLeaveMainLoop();
    }

    glutSwapBuffers();
    glutPostRedisplay();
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(128, 128);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

    int win = glutCreateWindow("MooGL GLUT test");
    check(win > 0, "glutCreateWindow makes a window");

    const char *version = (const char *)glGetString(GL_VERSION);
    printf("  %s\n", version ? version : "(no version)");
    check(version && strstr(version, "Compatibility"), "it is a compatibility context");

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutMainLoop();

    check(frames >= 3, "the main loop ran and returned");
    printf("%s\n", fails ? "FAILED" : "passed");
    return fails ? 1 : 0;
}
