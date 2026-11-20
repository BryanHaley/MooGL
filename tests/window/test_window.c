/*
 * test_window.c
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
 * What only shows with a real window, through MooGL's GLFW: the context
 * version a request gets, the default viewport, the window framebuffer's
 * size and stencil, and state kept from one frame to the next.
 */

#include <GL/glcorearb.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "MGLContext.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

// the compatibility profile calls this test makes; libmoogl exports them
#define GL_QUADS 0x0007
void glBegin(GLenum mode);
void glEnd(void);
void glVertex2f(GLfloat x, GLfloat y);
void glColor3f(GLfloat r, GLfloat g, GLfloat b);

static int fails;

static void check(int ok, const char *what)
{
    printf("  [%s] %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

static const char *vs =
    "#version 330 core\n"
    "void main() {\n"
    "    vec2 p = vec2((gl_VertexID & 1) * 4.0 - 1.0, (gl_VertexID & 2) * 2.0 - 1.0);\n"
    "    gl_Position = vec4(p, 0.0, 1.0);\n"
    "}\n";

static const char *fs =
    "#version 330 core\n"
    "out vec4 color;\n"
    "void main() { color = vec4(0.0, 1.0, 0.0, 1.0); }\n";

static const char *es_vs =
    "#version 300 es\n"
    "void main() {\n"
    "    vec2 p = vec2(float(gl_VertexID & 1) * 4.0 - 1.0, float(gl_VertexID & 2) * 2.0 - 1.0);\n"
    "    gl_Position = vec4(p, 0.0, 1.0);\n"
    "}\n";

static const char *es_fs =
    "#version 300 es\n"
    "precision mediump float;\n"
    "out vec4 color;\n"
    "void main() { color = vec4(0.0, 1.0, 0.0, 1.0); }\n";

static GLuint programOf(const char *v, const char *f)
{
    GLuint p = glCreateProgram();
    const char *src[2] = { v, f };
    GLenum stage[2] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };

    for (int i = 0; i < 2; i++) {
        GLuint s = glCreateShader(stage[i]);
        glShaderSource(s, 1, &src[i], NULL);
        glCompileShader(s);
        glAttachShader(p, s);
    }
    glLinkProgram(p);
    return p;
}

static GLuint program(void)
{
    return programOf(vs, fs);
}

static void pixel(int x, int y, unsigned char out[4])
{
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, out);
}

static int green(const unsigned char p[4]) { return p[0] < 30 && p[1] > 220 && p[2] < 30; }
static int red(const unsigned char p[4])   { return p[0] > 220 && p[1] < 30 && p[2] < 30; }

typedef struct {
    GLFWwindow *window;
    GLuint prog;
    unsigned char px[4];
} SharedDraw;

// the shared window's context, current on this thread only
static void *drawShared(void *arg)
{
    SharedDraw *job = (SharedDraw *)arg;
    GLuint vao;
    int w, h;

    glfwMakeContextCurrent(job->window);
    glfwGetFramebufferSize(job->window, &w, &h);
    glViewport(0, 0, w, h);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(job->prog);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glReadPixels(w / 2, h / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, job->px);
    glDeleteVertexArrays(1, &vao);
    glfwMakeContextCurrent(NULL);
    return NULL;
}

int main(void)
{
    unsigned char a[4], b[4];
    int w, h;

    // AppKit's application object; the toolkit has to be the one to make it
    void **app = (void **)dlsym(RTLD_DEFAULT, "NSApp");
    check(app == NULL || *app == NULL, "loading MooGL leaves the application object to the toolkit");

    if (!glfwInit())
        return 77;

    // what most Mac code asks for; MooGL answers with 4.6
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    GLFWwindow *window = glfwCreateWindow(320, 240, "MooGL window test", NULL, NULL);
    check(window != NULL, "a 3.3 core request makes a context");
    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwGetFramebufferSize(window, &w, &h);
    printf("  framebuffer %dx%d, %s\n", w, h, glGetString(GL_VERSION));
    check(!strncmp((const char *)glGetString(GL_VERSION), "4.6", 3), "and gets 4.6");

    GLuint prog = program();
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);

    // no glViewport call: the viewport starts as the whole framebuffer, in pixels
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    pixel(w - 1, h - 1, a);
    pixel(0, 0, b);
    check(green(a) && green(b), "a draw with the default viewport covers the whole framebuffer");
    glfwSwapBuffers(window);

    // GLFW asks for 8 stencil bits by default; mark the left half and draw where it is set
    glViewport(0, 0, w, h);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, w / 2, h);
    glClearStencil(1);
    glClear(GL_STENCIL_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 1, 0xff);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisable(GL_STENCIL_TEST);
    pixel(w / 4, h / 2, a);
    pixel(3 * w / 4, h / 2, b);
    check(green(a) && red(b), "the window framebuffer has a stencil buffer");
    glfwSwapBuffers(window);

    // a scissor box set in one frame still holds in the next
    glScissor(0, 0, w / 2, h);
    glfwSwapBuffers(window);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    pixel(w / 4, h / 2, a);
    pixel(3 * w / 4, h / 2, b);
    check(green(a) && red(b), "the scissor box carries over to the next frame");
    glfwSwapBuffers(window);

    // a frame of clear, draw and swap is one render pass
    {
        GLuint before = 0, after = 0;

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glfwSwapBuffers(window);

        MGLget(NULL, MGL_STAT_RENDER_PASSES, &before);
        for (int frame = 0; frame < 3; frame++)
        {
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glfwSwapBuffers(window);
        }
        MGLget(NULL, MGL_STAT_RENDER_PASSES, &after);
        check(after - before == 3, "a clear, a draw and a swap make one render pass");

        // GL_BACK is what double-buffered code draws to; a swap still presents
        glDrawBuffer(GL_BACK);
        MGLget(NULL, MGL_STAT_FRAMES_PRESENTED, &before);
        for (int frame = 0; frame < 3; frame++)
        {
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glfwSwapBuffers(window);
        }
        MGLget(NULL, MGL_STAT_FRAMES_PRESENTED, &after);
        check(after - before == 3, "with glDrawBuffer(GL_BACK) every swap shows the frame");
        glDrawBuffer(GL_FRONT);
    }

    // with vsync off a swap does not wait for the screen: 240 frames at 120 Hz
    // would take 2 s
    {
        double start;

        glfwSwapInterval(0);
        start = glfwGetTime();
        for (int i = 0; i < 240; i++)
        {
            glClear(GL_COLOR_BUFFER_BIT);
            glfwSwapBuffers(window);
        }
        glFinish();
        check(glfwGetTime() - start < 1.0, "with vsync off, swaps are not paced to the display");
        glfwSwapInterval(1);
    }

    check(glGetError() == GL_NO_ERROR, "no GL errors");

    // a window made to share sees the first one's program, not its vertex array
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow *shared = glfwCreateWindow(64, 64, "shared", NULL, window);
    check(shared != NULL, "a window can share the first one's objects");
    if (shared)
    {
        SharedDraw job = { shared, prog, { 0 } };
        pthread_t th;

        glfwMakeContextCurrent(shared);
        check(glIsProgram(prog) && !glIsVertexArray(vao), "programs are shared and vertex arrays are not");
        glfwMakeContextCurrent(NULL);

        pthread_create(&th, NULL, drawShared, &job);
        pthread_join(th, NULL);
        check(green(job.px), "it draws with that program from another thread");

        glfwMakeContextCurrent(window);
        glfwDestroyWindow(shared);
    }

    // a window that asks for no profile, the way GLUT-era programs do, gets
    // the old calls
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow *legacy = glfwCreateWindow(64, 64, "legacy", NULL, NULL);
    check(legacy != NULL, "a window with the default hints makes a context");
    if (legacy)
    {
        GLint mask = 0;
        unsigned char p[4];

        glfwMakeContextCurrent(legacy);
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &mask);
        check(mask == GL_CONTEXT_COMPATIBILITY_PROFILE_BIT, "it is a compatibility context");

        glClearColor(1, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glColor3f(0, 1, 0);
        glBegin(GL_QUADS);
        glVertex2f(-1, -1); glVertex2f(1, -1); glVertex2f(1, 1); glVertex2f(-1, 1);
        glEnd();
        pixel(32, 32, p);
        check(green(p), "glBegin and glEnd draw in it");
        check(glGetError() == GL_NO_ERROR, "no GL errors in it");

        glfwMakeContextCurrent(window);
        glfwDestroyWindow(legacy);
    }

#ifdef MGL_OGL_ES
    // an OpenGL ES window
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow *es = glfwCreateWindow(64, 64, "es", NULL, NULL);
    check(es != NULL, "an OpenGL ES 3.0 request makes a context");
    if (es)
    {
        GLuint vao, prog;
        unsigned char p[4];

        glfwMakeContextCurrent(es);
        check(!strncmp((const char *)glGetString(GL_VERSION), "OpenGL ES 3.1", 13), "and gets ES 3.1");
        check(glfwGetWindowAttrib(es, GLFW_CLIENT_API) == GLFW_OPENGL_ES_API &&
              glfwGetWindowAttrib(es, GLFW_CONTEXT_VERSION_MINOR) == 1, "GLFW agrees it is ES 3.1");

        prog = programOf(es_vs, es_fs);
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glUseProgram(prog);
        glClearColor(1, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        pixel(32, 32, p);
        check(green(p), "a GLSL ES 3.00 program draws in it");
        check(glGetError() == GL_NO_ERROR, "no GL errors in it");
        glfwSwapBuffers(es);

        glfwMakeContextCurrent(window);
        glfwDestroyWindow(es);
    }
#endif

    glfwTerminate();
    printf("%s\n", fails ? "FAILED" : "passed");
    return fails ? 1 : 0;
}
