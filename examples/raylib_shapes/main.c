/*
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
 * raylib on MooGL, through MooGL's GLFW: shapes, text and a spinning cube.
 * --frames N closes after N frames.
 */

#include <raylib.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    const int frames = argc > 2 && !strcmp(argv[1], "--frames") ? atoi(argv[2]) : 0;

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(640, 480, "MooGL raylib shapes");

    Camera3D camera = { { 3, 3, 3 }, { 0, 0, 0 }, { 0, 1, 0 }, 45, CAMERA_PERSPECTIVE };

    for (int frame = 0; !WindowShouldClose() && (!frames || frame < frames); frame++)
    {
        const float t = (float)GetTime();

        camera.position = (Vector3){ 4 * cosf(t), 3, 4 * sinf(t) };

        BeginDrawing();
        ClearBackground((Color){ 20, 22, 30, 255 });

        BeginMode3D(camera);
        DrawCube((Vector3){ 0, 0, 0 }, 1.5f, 1.5f, 1.5f, ORANGE);
        DrawCubeWires((Vector3){ 0, 0, 0 }, 1.5f, 1.5f, 1.5f, BLACK);
        DrawGrid(10, 0.5f);
        EndMode3D();

        DrawCircle(560, 80, 40, SKYBLUE);
        DrawText("raylib on MooGL", 20, 20, 24, RAYWHITE);
        DrawFPS(20, 440);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
