/*
 * Copyright (C) The MooGL Project
 *
 * A lit, spinning teapot through GLUT and the fixed-function pipeline, the way
 * programs were written before shaders, drawing through MooGL.
 */

#include <GLUT/glut.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float angle;
static int frames, max_frames;

static void reshape(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-0.5 * w / h, 0.5 * w / h, -0.5, 0.5, 1.0, 10.0);
    glMatrixMode(GL_MODELVIEW);
}

static void display(void)
{
    static const GLfloat light[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
    static const GLfloat copper[4] = { 0.85f, 0.45f, 0.2f, 1.0f };

    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glLoadIdentity();
    glLightfv(GL_LIGHT0, GL_POSITION, light);
    glTranslatef(0.0f, 0.0f, -3.0f);
    glRotatef(angle, 0.3f, 1.0f, 0.0f);

    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, copper);
    glutSolidTeapot(0.8);

    glutSwapBuffers();

    if (max_frames && ++frames >= max_frames)
        glutLeaveMainLoop();
}

static void tick(int value)
{
    (void)value;
    angle += 1.0f;
    glutPostRedisplay();
    glutTimerFunc(16, tick, 0);
}

static void keyboard(unsigned char key, int x, int y)
{
    (void)x;
    (void)y;
    if (key == 27 || key == 'q')
        glutLeaveMainLoop();
}

int main(int argc, char **argv)
{
    // --frames N quits after N frames, for a quick check
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--frames"))
            max_frames = atoi(argv[i + 1]);

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(640, 480);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glutCreateWindow("GLUT teapot on MooGL");
    printf("%s\n", (const char *)glGetString(GL_VERSION));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(16, tick, 0);
    glutMainLoop();
    return 0;
}
