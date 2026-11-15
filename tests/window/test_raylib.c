/*
 * Copyright (C) The MooGL Project
 *
 * test_raylib.c
 * MGL
 *
 * raylib on MooGL through MooGL's GLFW: its 3.3 core request, shapes, text,
 * a render texture and a GLSL 330 shader.
 */

#include <raylib.h>
#include <rlgl.h>

#include <stdio.h>
#include <string.h>

// from libmoogl; raylib keeps its GL loader to itself
const unsigned char *glGetString(unsigned int name);
#define GL_VERSION 0x1F02

static int fails;

static void check(int ok, const char *what)
{
    printf("  [%s] %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

static int near(unsigned char a, int b) { return a + 3 >= b && a <= b + 3; }

static int pixelIs(Image img, int x, int y, int r, int g, int b)
{
    Color c = GetImageColor(img, x, y);
    return near(c.r, r) && near(c.g, g) && near(c.b, b);
}

// what has been drawn so far this frame
static Image screen(void)
{
    rlDrawRenderBatchActive();
    return LoadImageFromScreen();
}

int main(void)
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "MooGL raylib test");

    check(IsWindowReady(), "a window");
    if (!IsWindowReady())
    {
        printf("FAILED\n");
        return 1;
    }

    check(rlGetVersion() == RL_OPENGL_33, "raylib takes its OpenGL 3.3 path");
    const char *version = (const char *)glGetString(GL_VERSION);
    check(version && strncmp(version, "4.6", 3) == 0, "on MooGL's 4.6");

    BeginDrawing();
    ClearBackground(RED);
    DrawRectangle(0, 0, 32, 64, GREEN);
    Image img = screen();
    check(img.width == 64 && pixelIs(img, 8, 32, 0, 228, 48) && pixelIs(img, 56, 32, 230, 41, 55),
          "it clears and draws a rectangle");
    UnloadImage(img);
    EndDrawing();

    // the default font is a texture raylib makes at start
    BeginDrawing();
    ClearBackground(BLACK);
    DrawRectangle(0, 0, 64, 64, BLACK);
    DrawText("IIII", 2, 2, 60, WHITE);
    img = screen();
    int lit = 0;
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++)
            lit += GetImageColor(img, x, y).r > 200;
    check(lit > 200, "it draws text");
    UnloadImage(img);
    EndDrawing();

    // a render texture, read back directly and drawn to the screen
    RenderTexture2D target = LoadRenderTexture(16, 16);
    check(IsRenderTextureValid(target), "a render texture");
    BeginTextureMode(target);
    ClearBackground(BLUE);
    EndTextureMode();
    img = LoadImageFromTexture(target.texture);
    check(pixelIs(img, 8, 8, 0, 121, 241), "it holds what was drawn into it");
    UnloadImage(img);

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(target.texture, (Rectangle){ 0, 0, 16, -16 }, (Rectangle){ 32, 0, 32, 64 }, (Vector2){ 0, 0 }, 0,
                   WHITE);
    img = screen();
    check(pixelIs(img, 48, 32, 0, 121, 241) && pixelIs(img, 8, 32, 0, 0, 0), "and draws onto the screen");
    UnloadImage(img);
    EndDrawing();

    // a GLSL 330 shader
    const char *fs = "#version 330\n"
                     "uniform vec4 tint;\n"
                     "out vec4 finalColor;\n"
                     "void main() { finalColor = tint; }\n";
    Shader shader = LoadShaderFromMemory(NULL, fs);
    check(IsShaderValid(shader), "a GLSL 330 shader");
    if (IsShaderValid(shader))
    {
        float tint[4] = { 1, 0, 1, 1 };
        SetShaderValue(shader, GetShaderLocation(shader, "tint"), tint, SHADER_UNIFORM_VEC4);
        BeginDrawing();
        ClearBackground(BLACK);
        BeginShaderMode(shader);
        DrawRectangle(0, 0, 64, 64, WHITE);
        EndShaderMode();
        img = screen();
        check(pixelIs(img, 32, 32, 255, 0, 255), "and draws with it");
        UnloadImage(img);
        EndDrawing();
        UnloadShader(shader);
    }

    UnloadRenderTexture(target);
    CloseWindow();

    printf("%s\n", fails ? "FAILED" : "passed");
    return fails ? 1 : 0;
}
