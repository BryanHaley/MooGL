/*
 * test_sdl_renderer.c
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
 * SDL_Renderer's opengl backend on MooGL: a compatibility context drawing
 * with client arrays, the matrix stacks and render-target textures.
 */

#include <SDL2/SDL.h>

#include <stdio.h>
#include <string.h>

static int fails;

static void check(int ok, const char *what)
{
    printf("  [%s] %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

static int near(Uint8 a, int b) { return a + 3 >= b && a <= b + 3; }

static int pixelIs(SDL_Renderer *r, int x, int y, int red, int green, int blue)
{
    Uint8 px[4];
    SDL_Rect at = { x, y, 1, 1 };

    if (SDL_RenderReadPixels(r, &at, SDL_PIXELFORMAT_RGBA32, px, 4) != 0)
        return 0;
    return near(px[0], red) && near(px[1], green) && near(px[2], blue);
}

int main(void)
{
    SDL_RendererInfo info;

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 77;

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

    SDL_Window *window = SDL_CreateWindow("MooGL SDL renderer test", SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED, 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer *r = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : NULL;

    check(r != NULL, "SDL makes an opengl renderer");
    if (r == NULL)
    {
        printf("  %s\nFAILED\n", SDL_GetError());
        return 1;
    }

    SDL_GetRendererInfo(r, &info);
    check(strcmp(info.name, "opengl") == 0, "and it is the opengl one, not Metal");
    check((info.flags & SDL_RENDERER_TARGETTEXTURE) != 0, "it can render to a texture");

    // a fill and a rectangle
    SDL_SetRenderDrawColor(r, 255, 0, 0, 255);
    SDL_RenderClear(r);
    SDL_SetRenderDrawColor(r, 0, 255, 0, 255);
    SDL_Rect left = { 0, 0, 32, 64 };
    SDL_RenderFillRect(r, &left);
    check(pixelIs(r, 8, 32, 0, 255, 0) && pixelIs(r, 56, 32, 255, 0, 0), "it clears and fills a rectangle");

    // a streaming texture copied to the right half
    Uint32 blue[16];
    for (int i = 0; i < 16; i++)
        blue[i] = 0xff0000ffu;   // ARGB8888: opaque blue
    SDL_Texture *tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 4, 4);
    SDL_UpdateTexture(tex, NULL, blue, 16);
    SDL_Rect right = { 32, 0, 32, 64 };
    SDL_RenderCopy(r, tex, NULL, &right);
    check(pixelIs(r, 48, 32, 0, 0, 255) && pixelIs(r, 8, 32, 0, 255, 0), "it draws a texture");

    // drawn into a target texture, then onto the window
    SDL_Texture *target = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 16, 16);
    SDL_SetRenderTarget(r, target);
    SDL_SetRenderDrawColor(r, 255, 255, 0, 255);
    SDL_RenderClear(r);
    SDL_SetRenderTarget(r, NULL);
    SDL_Rect corner = { 0, 0, 16, 16 };
    SDL_RenderCopy(r, target, NULL, &corner);
    check(pixelIs(r, 8, 8, 255, 255, 0) && pixelIs(r, 8, 40, 0, 255, 0), "it renders to a texture and draws that");

    SDL_RenderPresent(r);

    SDL_DestroyTexture(target);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("%s\n", fails ? "FAILED" : "passed");
    return fails ? 1 : 0;
}
