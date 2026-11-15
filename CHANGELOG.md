## MooGL 0.2.0

### Added
- Shared contexts. Contexts created to share with another one now have the same
  textures, buffers, renderbuffers, samplers, programs, shaders and fences. Each
  context still has its own vertex arrays, framebuffers and transform feedback
  objects, as OpenGL requires. Before, asking for a shared context failed.
  - Works with GLFW's `share` window argument and SDL2's
    `SDL_GL_SHARE_WITH_CURRENT_CONTEXT`, and from C with `createGLMContextShared`.
  - Each thread has its own current context, so contexts on different threads can
    load and draw at the same time.
  - An object deleted in one context stays alive while another context still uses
    it, and is freed once none does.
  - Fences work across contexts: `glWaitSync` makes one context's GPU work wait for
    another's, and `glClientWaitSync` honours its timeout.
- Dear ImGui in the SDK: `libimgui.a` (docking branch, with its GLFW, SDL2 and
  OpenGL 3 backends) and a `MooGL::imgui` CMake target. Stock ImGui loads Apple's
  OpenGL on a Mac; this build draws with MooGL. Two examples, one for GLFW and one
  for SDL2, open ImGui's demo with multi-viewports, so windows dragged out of the
  main one get their own shared context.
- Compatibility profile contexts. Asking GLFW or SDL2 for the compatibility profile, or for
  no profile at all as old code does, gets a 4.6 context that also has what the core profile
  removed. From C, `createGLMContextProfile` takes the profile. A core context is unchanged.
  - `glBegin` and `glEnd`, client-side vertex arrays, `glInterleavedArrays`, and quads, quad
    strips and polygons.
  - The modelview, projection and texture matrix stacks.
  - Fixed-function lighting (eight lights, spot lights, two-sided lighting, colour material),
    `glTexEnv` with every mode including `GL_COMBINE`, texture coordinate generation, fog and
    point sprites, through a generated shader for each combination of state.
  - `glPushAttrib` and `glPopAttrib`, and a different polygon mode for front and back faces.
  - Evaluators: `glMap1`, `glMap2`, `glMapGrid`, `glEvalCoord`, `glEvalPoint` and `glEvalMesh`, with
    `GL_AUTO_NORMAL`. GLU's NURBS draws with them.
  - Display lists, selection with `glRenderMode(GL_SELECT)`, and `glRasterPos`, `glWindowPos`,
    `glDrawPixels`, `glBitmap` and `glCopyPixels`.
  - Old GLSL, 1.10 to 1.30: `attribute`, `varying`, `texture2D`, `gl_FragColor` and the
    built-in matrices, lights and other state.
  - The luminance, alpha and intensity texture formats read the way they did, and
    `GL_ARB_multitexture` is listed, with its `ARB` entry points.
- GLUT in the SDK: `libglut`, which is freeglut with a MooGL backend in place of NSOpenGL, with
  `<GLUT/glut.h>`, `<GL/freeglut.h>`, a `MooGL::glut` CMake target and a teapot example.
- GLU in the SDK: `libGLU`, which is Mesa's GLU linked to MooGL, with `<GL/glu.h>` and a `MooGL::GLU`
  CMake target. Mac programs keep including `<OpenGL/glu.h>`.
- CGL and AGL. A program linked to `libmoogl` instead of `OpenGL.framework` makes MooGL contexts
  with `CGLChoosePixelFormat`, `CGLCreateContext` and the rest of CGL's context calls, or with
  `aglCreateContext` and AGL, whose header MooGL now ships. These contexts draw offscreen.
- Apple's extensions from early Mac OS X, in compatibility contexts: `GL_APPLE_client_storage`,
  `GL_APPLE_texture_range`, `GL_APPLE_ycbcr_422` (4:2:2 video frames, read as RGB),
  `GL_APPLE_packed_pixels` and `GL_EXT_texture_rectangle`, plus `GL_EXT_framebuffer_object`'s names.
- `SDL_Renderer`'s `opengl` backend is built again. Metal stays SDL's default.
- Any texture border colour is exact. Metal's samplers hold only three, so the shader adds any other
  by how much of the filter fell outside the texture; before, linear filtering blended the edge with
  the nearest of the three.

### Faster
- Old-style drawing is much faster. GLQuake's `timedemo demo1` went from 48 to about 225 fps,
  and Half-Life under Xash3D went from about 105 to about 130 fps.
  - A draw that switches the program or vertex array away and back to the ones the last draw
    used no longer counts as a change.
  - `glVertexAttrib` and `glColor` no longer rebuild the render pipeline; only the new value
    is sent.
  - The pipeline cache key is built from plain data, and the Metal vertex layout is only made
    when a new pipeline is needed.
  - `glBegin`/`glEnd` only sends the attributes that change between vertices, and large
    client-side arrays and indices go into one shared buffer instead of a new Metal buffer
    each draw.

### Fixed
- Programs using MooGL never became the active app, so their windows got no keyboard input and
  could not capture the mouse. Loading the library made a context with an `NSView`, which set up
  AppKit's application object before SDL or GLFW could. That context is now made on first use.
- After `glDrawBuffer(GL_BACK)`, which double-buffered programs such as Half-Life call every
  frame, a swap showed nothing and never sent the frame's work to the GPU. The window stayed
  blank and the program slowed down more and more.
- Two driver-made vertex buffers could be mistaken for one, since both had no name; the
  second array read the first one's data.
- `GL_LUMINANCE16` had the value of `GL_LUMINANCE16_ALPHA16`.
- Proxy textures (`GL_PROXY_TEXTURE_2D` and the rest) gave `GL_INVALID_ENUM`. They now report the size a
  texture would have, or zero when it would not fit.

## MooGL 0.1.1

### Faster
- SuperTuxKart is about 3× faster (52 → ~160 fps on its built-in benchmark).
  Normalized 10:10:10:2 vertex attributes are now read by Metal directly instead
  of being converted on the CPU for every draw.
- IDKEngine is about 14% faster (183 → 209 fps at 1600×800).
- Buffer updates no longer stall waiting for the GPU. A partial update of a buffer
  the GPU is still using is copied on the GPU in order, and read-backs only wait
  when the GPU wrote the buffer.
- Deleting buffers, textures and other objects no longer interrupts rendering.
- One render pass per frame instead of three: `glClear` becomes the next pass's
  clear, and swap no longer starts an empty pass or waits for the next drawable early.
- Setting state to the value it already has (`glEnable`, depth, cull and so on)
  costs nothing, and viewport, scissor, depth and cull changes no longer rebuild
  the render pipeline.
- Setting a sampler or uniform-block binding no longer rebuilds the program's
  Metal shaders.
- With primitive restart on, triangle and line lists are only reworked on the CPU
  when they actually contain the restart index.
- Vertex formats Metal can't read are converted once per buffer change instead of
  on every draw.

### Fixed
- Tightly packed 10:10:10:2 vertex attributes (stride 0) were read 16 bytes apart
  instead of 4.
- Drawing with no program bound could crash.

### Added
- `MGLget` can read driver counters (`MGL_STAT_*` in `MGLContext.h`): pipelines
  built, GPU waits, render passes, buffer rebinds, CPU index expansions, program
  rebuilds and attribute conversions.