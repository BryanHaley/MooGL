/*
 * Copyright (C) The MooGL Project
 *
 * test_sdl3.c
 * MGL
 *
 * MooGL's SDL3: core and compatibility contexts, sharing, and SDL's opengl
 * renderer drawing through a compatibility context.
 */

#include <GL/glcorearb.h>
#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

static int fails;

static void check(int ok, const char *what)
{
    printf("  [%s] %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

static int near(Uint8 a, int b) { return a + 3 >= b && a <= b + 3; }

static int glPixelIs(int x, int y, int r, int g, int b)
{
    GLubyte p[4];

    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
    return near(p[0], r) && near(p[1], g) && near(p[2], b);
}

static void contexts(void)
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window *window = SDL_CreateWindow("MooGL SDL3 test", 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    SDL_GLContext core = window ? SDL_GL_CreateContext(window) : NULL;

    check(core != NULL, "a core context");
    if (!core)
    {
        printf("  %s\n", SDL_GetError());
        return;
    }

    int major = 0, minor = 0, profile = 0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile);
    check(major == 4 && minor == 6 && profile == SDL_GL_CONTEXT_PROFILE_CORE, "a 4.1 core request gets 4.6 core");

    const char *version = (const char *)glGetString(GL_VERSION);
    check(version && strncmp(version, "4.6", 3) == 0, "GL_VERSION says 4.6");

    PFNGLCLEARCOLORPROC clearColor = (PFNGLCLEARCOLORPROC)SDL_GL_GetProcAddress("glClearColor");
    check(clearColor == (PFNGLCLEARCOLORPROC)glClearColor, "SDL_GL_GetProcAddress finds MooGL's functions");

    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    check(glPixelIs(32, 32, 0, 255, 0), "it clears");

    int interval = -2;
    check(SDL_GL_SetSwapInterval(1) && SDL_GL_GetSwapInterval(&interval) && interval == 1, "swap interval");
    check(SDL_GL_SwapWindow(window), "it swaps");

    GLuint tex, vao;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glGenVertexArrays(1, &vao);

    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    SDL_GLContext second = SDL_GL_CreateContext(window);
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);

    check(second != NULL, "a second context sharing with the first");
    if (second)
    {
        check(glIsTexture(tex) && !glIsVertexArray(vao), "it shares the texture and not the vertex array");
        SDL_GL_MakeCurrent(window, core);
        SDL_GL_DestroyContext(second);
    }
    check(glIsTexture(tex), "the texture outlives the second context");
    check(glGetError() == GL_NO_ERROR, "no GL errors in the core context");

    SDL_GL_DestroyContext(core);

    SDL_GL_ResetAttributes();
    SDL_GLContext compat = SDL_GL_CreateContext(window);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile);
    check(compat != NULL && profile == SDL_GL_CONTEXT_PROFILE_COMPATIBILITY,
          "with no profile asked for, a compatibility context");
    if (compat)
    {
        GLint mask = 0;
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &mask);
        check(mask == GL_CONTEXT_COMPATIBILITY_PROFILE_BIT, "GL agrees it is compatibility");
        SDL_GL_DestroyContext(compat);
    }

    SDL_DestroyWindow(window);
}

static int rendererPixelIs(SDL_Renderer *r, int x, int y, int red, int green, int blue)
{
    SDL_Rect at = { x, y, 1, 1 };
    SDL_Surface *s = SDL_RenderReadPixels(r, &at);
    Uint8 cr = 0, cg = 0, cb = 0, ca = 0;
    int ok;

    if (!s)
        return 0;
    ok = SDL_ReadSurfacePixel(s, 0, 0, &cr, &cg, &cb, &ca);
    SDL_DestroySurface(s);
    return ok && near(cr, red) && near(cg, green) && near(cb, blue);
}

static void renderer(void)
{
    SDL_Window *window = SDL_CreateWindow("MooGL SDL3 renderer test", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer *r = window ? SDL_CreateRenderer(window, "opengl") : NULL;

    check(r != NULL && strcmp(SDL_GetRendererName(r), "opengl") == 0, "SDL makes an opengl renderer");
    if (!r)
    {
        printf("  %s\n", SDL_GetError());
        return;
    }

    SDL_SetRenderDrawColor(r, 255, 0, 0, 255);
    SDL_RenderClear(r);
    SDL_SetRenderDrawColor(r, 0, 255, 0, 255);
    SDL_FRect left = { 0, 0, 32, 64 };
    SDL_RenderFillRect(r, &left);
    check(rendererPixelIs(r, 8, 32, 0, 255, 0) && rendererPixelIs(r, 56, 32, 255, 0, 0),
          "it clears and fills a rectangle");

    // a streaming texture into a render target, then the target onto the screen
    Uint32 blue[16];
    for (int i = 0; i < 16; i++)
        blue[i] = 0xFF0000FF;
    SDL_Texture *t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 4, 4);
    SDL_Texture *target = SDL_CreateTexture(r, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, 16, 16);
    SDL_UpdateTexture(t, NULL, blue, 16);

    SDL_SetRenderTarget(r, target);
    SDL_RenderTexture(r, t, NULL, NULL);
    SDL_SetRenderTarget(r, NULL);
    SDL_FRect right = { 32, 0, 32, 64 };
    SDL_RenderTexture(r, target, NULL, &right);
    check(rendererPixelIs(r, 48, 32, 0, 0, 255) && rendererPixelIs(r, 8, 32, 0, 255, 0),
          "a texture through a render target");
    check(SDL_RenderPresent(r), "it presents");

    SDL_DestroyTexture(target);
    SDL_DestroyTexture(t);
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(window);
}

int main(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
        return 77;

    contexts();
    renderer();

    SDL_Quit();
    printf("%s\n", fails ? "FAILED" : "passed");
    return fails ? 1 : 0;
}
