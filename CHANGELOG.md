## MooGL 0.2.0

### Added
- Minecraft runs on MooGL without being rebuilt: two JVM arguments in the launcher point
  LWJGL at MooGL's SDL3 and `libmoogl`. Minecraft 26.3 with Fabric, Sodium and Iris runs the
  BetterVoxels shaderpack, which needs OpenGL 4.3 and does not run on Apple's 4.1.
- Up to 32 textures in each shader stage, up from 16. A stage that samples more than 16 reads
  them through a Metal argument buffer, since Metal has 16 sampler slots a stage.
- `MGLsetContextView`, which moves a context's drawing to another `NSView`, as
  `NSOpenGLContext`'s `setView` does. MooGL's SDL3 uses it, so a context made with one window
  can be made current with another.
- OpenGL ES 3.1 contexts, in a build made with `make OGL_ES=1`; a plain build leaves them out,
  and the SDK has them. GLFW's `GLFW_OPENGL_ES_API` and SDL's `SDL_GL_CONTEXT_PROFILE_ES` make
  one (any request from ES 2.0 to 3.1 gets 3.1), and so does `createGLMContextProfile` with
  `MGL_CONTEXT_ES_PROFILE_BIT`. SDL's `opengles2` renderer runs on one. An ES context has the
  ES strings, extensions, enables, texture formats and errors, and takes GLSL ES 1.00, 3.00
  and 3.10 shaders under the ES rules. KHR-GLES3 and KHR-GLES31 pass with no failures.
  ES contexts also list `GL_EXT_buffer_storage`, `GL_OES_texture_storage_multisample_2d_array`
  and `GL_KHR_texture_compression_astc_ldr`. GLSL ES 1.00 attributes may share a location.
  The separate `libmoogles` build is gone.
- SDL3, SFML 3, SFML 2.6 and raylib on MooGL, in the SDK with their own CMake packages and
  an example each. SDL3 and SFML make MooGL contexts in place of Apple's; raylib is stock,
  built on MooGL's GLFW. SDL3's `opengl` renderer works, and so does SFML's graphics module.
- A `glfw3` CMake package for MooGL's GLFW.
- Khronos's `<GL/glext.h>` in the SDK, which `<GL/gl.h>` now includes in place of Apple's
  older `glext.h`, so code written for Linux or Windows builds unchanged.
- In compatibility contexts, `GL_ARB_shader_objects`, `GL_ARB_vertex_shader`,
  `GL_ARB_fragment_shader` and `GL_ARB_shading_language_100` with their `ARB` calls, which
  SFML checks for before it uses shaders, and the `EXT` names for blending and framebuffer
  blits and multisampling.
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
- Geometry shaders cost less. The draws of one multi-draw run their geometry stage in one
  compute pass and draw in one render pass, where each used to break the pass. Each geometry
  kernel marks its own unused output as empty instead of a blit clearing it first, holds less
  in registers when building triangle strips, and gets room for as many triangles as its strip
  can make rather than three times its vertex count. BetterVoxels, which draws its shadows
  through a geometry shader, went from 5 to 8 FPS.
- Buffers use cached CPU memory, not write-combined. Copying into a buffer the GPU is still
  using reads the old contents back on the CPU, and from write-combined memory that is very
  slow. Minecraft does this every frame: it went from 177 to about 1,380 FPS with shaders off,
  and from 55 to 138 with BSL, ahead of Apple's OpenGL (389 and 113).
- With vsync off, a swap no longer waits for the window to hand back a drawable. A window
  takes a new frame only once per screen refresh, so before, everything ran at the display's
  rate. The newest frame is now shown when the screen can take one, and older ones are skipped,
  as Apple's OpenGL does.
- A small texture with data is made CPU visible and filled straight in when it is created,
  before the GPU has seen it, with no copy on the GPU at all.
- Shader builds skip SPIRV-Tools' check of the module before inlining; glslang made it, so it
  only cost time. Old-style drawing builds a program for each new combination of state, so
  this shortens the stalls the first time each one is drawn.
- Texture levels for common formats are written with a small draw of MooGL's own instead of
  the GPU's copy from a buffer, which needs a pipeline the driver builds for each new texture
  size. A texture's levels go up together, and the pipelines for the usual formats are made in
  the background when a context starts.
- Old-style drawing sends the fixed-function matrices and values again only when they changed,
  rather than working them all out and comparing them before every draw.
- Format lookups use an index instead of searching the format table, and draws with programs
  that use no images skip the image bookkeeping.
- MooGL's stream buffer for client-side arrays is handed to Metal as it is, rather than copied
  into a new Metal buffer each time it fills up.
- Texture uploads no longer wait for the GPU. Each level used to wait for everything queued
  before it, so loading a level's textures in Half-Life stalled a frame for a quarter of a
  second. Another context that draws with a texture after only a `glFlush` still waits for it.
- Leaving a framebuffer after real drawing sends that work to the GPU, as a `glFlush` would,
  so the GPU draws one pass while the application builds the next. Before, a frame of many
  passes only started on the GPU at the swap. SuperTuxKart went from about 270 to 336 fps.
- Up to 96 command buffers can be in flight, not 64. A frame now sends one for each
  framebuffer it leaves, and on a busy machine their completions run late, so the next one
  waited; SuperTuxKart's slowest frames came from that.
- With vsync off, the swap hands the back buffer itself to the present queue and draws the
  next frame into another one, rather than copying the frame twice on its way to the window.
- `glFenceSync` sends the work queued before it to the GPU straight away, so a later wait on
  the fence finds it done. SuperTuxKart waits on one every frame.
- Shaders that use no doubles compile with Metal's relaxed maths, which allows faster maths
  but keeps infinities and NaNs. Shaders that use doubles keep exact maths, which the double
  emulation needs.
- Textures get Metal's shader-write usage only once they are bound as images, through a unit
  or a bindless handle, or handed out with `MGLgetMetalTexture`. That usage turns off Apple's
  lossless compression, which render targets like a G-buffer benefit from. A texture made
  before its first image binding is made again, contents and all. `GL_RGB9_E5` and
  `GL_R11F_G11F_B10F` keep the usage always: compressed, some of their bit patterns came back
  changed from a copy.
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
- A draw after new `glBufferData` and a bind of some other buffer uses the new vertices, not
  the last draw's. Sprinting in DawnCraft left long black streaks from the particles.
- `GL_ARB_conservative_depth` is listed, and with it a shader before GLSL 4.20 can redeclare
  `gl_FragDepth` with a depth layout. Create's Flywheel renderer does this, and Minecraft
  1.18.2 with the DawnCraft modpack crashed on entering a world.
- GLSL names that are C++ keywords, such as a variable called `new` or `and`, no longer make
  Metal reject the shader. BSL's deferred pass has one.
- A shared array of `vec3` takes 12 bytes an element, as in GL, instead of Metal's 16, so one
  that fits GL's 32 KB fits Metal's too. Photon's sky pass needed 27,648 bytes and got 36,864.
- A compute pipeline Metal will not build for the whole work group is made without that
  request and the reason is logged, instead of every dispatch failing silently.
- Uniform blocks no longer read each other's buffers when one of them is used only from a
  global variable's initializer. glslang left that block at binding 0, where SPIRV-Cross took
  it for the same buffer as another block. Iris's sky shaders hit this: Minecraft's sky read
  its matrices out of the fog block and drew nothing.
- Translated shaders are compiled as Metal 3.2. Left unset, Metal picked the version the
  program was built against, which in Minecraft's Java was 3.1, and `memoryBarrierShared()`
  did not compile.
- Compute shaders run with the whole work group they declare. A shader using many registers
  got a Metal pipeline that allowed fewer threads, and Metal dropped the dispatch.
- `glGenerateMipmap` keeps what was drawn into level 0 when it makes room for the other levels.
- A vertex shader with more than 16 plain uniforms draws. The draw was refused, and the vertex
  data could land on the uniform table's slot.
- Integer vertex inputs get their values when the array's signedness differs from the
  input's, or when the array was set with `glVertexAttribPointer`. Metal refused the pipeline.
- Geometry shaders: varyings declared several to a statement (`flat out vec3 a, b;`), a
  function parameter named like an output, and draws with a large first or base vertex,
  which used to size their buffer from vertex 0.
- MooGL's SDL3 no longer shows the wrong part of the frame after a context moves to a window
  of another size.
- `glCopyImageSubData` no longer refuses a texture that has only its base level when its
  minification filter is `GL_NEAREST` or `GL_LINEAR`; such a texture is complete, since the
  filter needs no mipmaps.
- Drawing into a `GL_RGB9_E5` renderbuffer no longer aborts in Metal's pipeline check.
- `glGen*` no longer hands out a name an app already brought to life by binding it, and a
  name deleted twice is not handed out twice.
- `glGenerateMipmap` works on luminance and alpha textures in ES contexts.
- 3-row matrices (`mat3`, `mat2x3`, `mat4x3`) in a struct uniform that also holds a sampler
  get the right values.
- An integer attribute with an offset or stride that is not a multiple of 4, such as one
  `GLshort`, reaches the shader; nothing drew.
- GLSL ES 1.00 shaders that bind every attribute location twice now link.
- Program pipelines: two stages from different programs that declare a uniform with the same
  name each keep their own value; both saw one of them. In ES and compatibility contexts flat
  values come from the last vertex with a pipeline too. In ES contexts a pipeline whose stages
  disagree on a varying's flat qualifier, location or struct type fails validation, and drawing
  with any mismatched pipeline is `GL_INVALID_OPERATION`.
- Samplers inside struct uniforms are listed by `glGetProgramResourceIndex` and the other
  program interface queries. In ES contexts a separable program's varyings between stages
  report location -1 unless `layout(location)` placed them.
- Bools inside struct uniforms reach the shader; a struct's bool and bvec members read false.
  A struct member named like a bool member of another struct keeps its own type, so
  `glUniform2uiv` on it no longer fails.
- An array of structs in a storage or uniform block keeps the block's stride when a `vec3`
  member is packed tight in Metal, as in `struct { ivec3 v; int a; int b; }`; every element
  past the first was read from the wrong place. The fix is in MGL's SPIRV-Cross.
- `glDebugMessageControl` filters messages by source, type, severity and id, applied in order,
  and each debug group's own settings end when it is popped. Low severity messages start out
  off, as GL_KHR_debug says. Before, the call turned every message on or off at once.
- A sampler array inside an array of structs, such as `sampler2D b[2]` in `uniform S u[2]`,
  compiles in shaders older than GLSL 4.30 and ES 3.10; MGL keeps it as one flat array.
- Error checks dEQP's negative tests look for: `glVertexAttribPointer` sizes and packed types,
  `glTexImage*` size and level limits and unequal cube faces, a pixel unpack buffer offset
  that is not a whole number of the pixel type, `glTexStorage*` on texture 0, a level past the
  target's limit in the sub-image and copy calls, location queries on a deleted program,
  `glShaderSource` on a program name, and `glDispatchComputeIndirect` offsets. An empty
  compressed image is no longer an error.
- In ES contexts, a draw with no program checks its arguments and the framebuffer, then draws
  nothing; an indexed draw while transform feedback records, a negative instance count and an
  end before the start are errors; `glGenerateMipmap` needs a color-renderable, filterable
  format; and copies into part of a texture follow the copy format rules.
- An array of arrays in a buffer block whose outer size is 1, such as `bool b[1][8]` under
  std140, keeps its element stride; its elements were read 4 bytes apart. The fix is in MGL's
  SPIRV-Cross. The inner array of `a[1][1]` reports a size of 1, not 0.
- A `layout(...)` list that names `shared` or `packed` more than eight times compiles.
- The viewport's size is clamped to `GL_MAX_VIEWPORT_DIMS`; a larger one drew nothing.
- A framebuffer with no attachments and a default sample count Metal lacks, such as 3, rounds
  it up and draws; it drew nothing.
- A program that uses more uniform or storage blocks than a stage or the whole program allows,
  or a compute shader whose work group or shared variables are over the limits, fails to link.
- In ES contexts, a block declared in two stages must have the same layout and the same sized
  and unsized arrays in both, or the program does not link.
- `glBindVertexBuffer` takes a buffer name from `glGenBuffers` that was never bound, and
  deleting such a name frees it.
- The stencil reference is clamped to 0..255 as GL says; -1 and 256 wrapped around instead.
- A vertex attribute at an offset or stride that is not a multiple of 4, such as bytes at
  offset 17, is read correctly; Metal cannot read it in place, so MGL converts a copy.
- A texture deleted while attached to a framebuffer that is not bound stays attached and keeps
  its image until it is detached; the framebuffer became incomplete.
- `glBlitFramebuffer` honours the scissor test when it scales, and a scaled nearest blit picks
  the source texel GL's formula names; some pixels whose centres fell on a texel edge took
  the texel before it.
- Float depth uploads are clamped to [0, 1].
- Transform feedback records a triangle's or line's vertices in the order given when the
  fragment shader has flat inputs; the reordering that gives flat values from the last vertex
  is undone for the recording.
- A program whose vertex inputs need more locations than `GL_MAX_VERTEX_ATTRIBS` fails to link.
- Mip levels may be given in any order: levels set before the base image, or before a level
  that shows the first guess at the texture's shape was wrong, keep their contents.
- In ES contexts cube maps are always seamless, so their wrap modes no longer change what
  the face edges sample.
- A struct uniform larger than 256 bytes that was never set reads as zero throughout; its
  zero buffer was only 256 bytes long.
- `GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING` is `GL_LINEAR` for an attachment with no storage
  yet. In ES contexts a new renderbuffer reports `GL_RGBA4`, a blit from an image onto the
  same image is `GL_INVALID_OPERATION`, and `GL_EXT_conservative_depth` is listed.
- `glGetTexLevelParameter` on a cube map face reads that face, not the first.
- `glGetObjectLabel` with a `bufSize` of 0 gives the label's length; only a negative size is an
  error. `glGetInternalformativ` writes only the values it has, leaving the rest of the buffer.
- In ES contexts, `glGet` of a desktop-only capability such as `GL_FRAMEBUFFER_SRGB` is
  `GL_INVALID_ENUM`.
- `glBlendColor` takes effect; the constant blend factors used black.
- Program introspection follows GL for blocks: a uniform-block array of arrays is listed one
  uniform per outer element, every level of an array of structures in a block is listed, each
  element of a block array lists the block's variables, an unused `shared` or `std140` block
  is still active, and `GL_TOP_LEVEL_ARRAY_SIZE` and `_STRIDE` count only an array of
  structures or arrays, with 0 for an unsized one.
- `glGetTexLevelParameter` on a level with no image returns GL's defaults; only a level past
  the largest a texture could have is `GL_INVALID_VALUE`.
- `GL_DEBUG_LOGGED_MESSAGES`, `GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH` and the debug group depth
  can be read, and the `KHR`-suffixed `GL_KHR_debug` calls an ES program looks for exist.
- Alpha and luminance textures in ES contexts hold their texels; they read as 0. RGB
  framebuffers read back and blit with alpha 1, and a depth-only or stencil-only blit of a
  packed depth-stencil image leaves the other part alone.
- `glGetShaderPrecisionFormat` gives the range and precision of 32-bit integers for the int
  types, and the elements of a sampler array with no `binding` all start at unit 0.
- `glGet*` reads every capability, such as `GL_BLEND`, every hint, and the buffer, vertex
  array, renderbuffer and sampler bindings, which read 0. `glGetInteger64v` returns every
  value of a query with several, colours and depths read as integers span the integer range,
  and values round rather than truncate. `GL_GENERATE_MIPMAP_HINT` exists in compatibility
  and ES contexts.
- Bool uniform arrays, and bools in struct uniforms, keep every element; past the first they
  read back 0. Array uniforms are reported with `[0]` on the end, a uniform two stages use is
  listed once, and a struct of nothing but samplers no longer leaves a stray member behind.
- A 3D texture deeper than it is wide or tall has all its mip levels.
- `glClearDepth` clamps to [0, 1], and a stencil clear value keeps only the stencil bits.
- A new shader reports `GL_COMPILE_STATUS` false until it compiles, and
  `GL_SHADER_SOURCE_LENGTH` counts the closing null.
- Texture parameters set or read across float and integer round to the nearest value.
- `glBlitFramebuffer` leaves alone the destination pixels whose source is outside the read
  framebuffer.
- Shader functions named like Metal's own, such as `plus` or `minus`, compile.
- Scissored clears and draws with a scissor box partly or wholly off the left or bottom of the
  target touch only the box. They covered the whole target.
- `glBlitFramebuffer` mirrors when a rectangle runs backwards, filters with `GL_LINEAR`, and
  clips a destination off the target. It also gives `GL_INVALID_OPERATION` for integer and
  other colour together, a `GL_LINEAR` blit of integers, depth or stencil formats that differ,
  and a multisampled side with rectangles of different sizes.
- `GL_SAMPLE_COVERAGE` and `glSampleCoverage` work; they had no effect.
- RGB formats read and blend with alpha 1. MooGL keeps them in RGBA, and clears wrote the
  clear colour's alpha there.
- `gl_FrontFacing` follows `glFrontFace` with culling off, and `glCullFace(GL_FRONT_AND_BACK)`
  draws no polygons. Culling stayed on when it was turned off part way through a pass.
- `gl_DepthRange` works. glslang leaves it out when making SPIR-V for GL.
- `glProvokingVertex` is kept, and `GL_PROVOKING_VERTEX` starts as `GL_LAST_VERTEX_CONVENTION`.
  In compatibility and ES contexts, flat inputs take the last vertex of each primitive.
- A restart index in a point list draws nothing, and a 32-bit restart index with client arrays
  no longer crashes.
- `glClear` and `glClearBuffer*` do nothing while `GL_RASTERIZER_DISCARD` is on.
- `GL_ACTIVE_ATTRIBUTES` and `GL_ACTIVE_ATTRIBUTE_MAX_LENGTH` count vertex inputs only.
- `glVertexAttribIPointer` takes client memory in compatibility and ES contexts, and
  `GL_FIXED` attributes can be tightly packed.
- A shader or program name used where the other kind goes gives `GL_INVALID_OPERATION`, and a
  name that is neither gives `GL_INVALID_VALUE`, across the program and shader calls.
  `glDetachShader` of a shader that is not attached is `GL_INVALID_OPERATION`, and an unknown
  `glGetProgramiv` pname is `GL_INVALID_ENUM`.
- `glBindVertexArray`, `glBindTransformFeedback` and `glBeginQuery` take only generated names,
  and a query name only becomes a query when first used. `glIsTransformFeedback(0)` is false.
- A texture keeps the target it was first bound to; binding it to another is
  `GL_INVALID_OPERATION`.
- A deleted buffer stays readable in vertex arrays other than the bound one. It was freed
  under them.
- Deleting the bound framebuffer binds the window's, and `glBindFramebuffer` checks its target.
- `glReadPixels` from a multisampled framebuffer is `GL_INVALID_OPERATION`.
- `glBufferSubData` checks its range even with no data, and `glDeleteFramebuffers`,
  `glGenTransformFeedbacks` and `glDeleteTransformFeedbacks` refuse a negative count.
- Compressed images past the size limits and compressed updates past the last possible level
  are `GL_INVALID_VALUE`. `glCopyTexImage2D` into a cube face needs a square image; a
  rectangle crashed.
- A texture's last possible level can be attached to a framebuffer.
- Samplers in a struct uniform are listed and found under their GL names, such as `s.tex`.
- A shader with a parameter named `atomic_uint` is refused again.
- Defining a new mip level or cube face no longer loses what the GPU drew or copied into the
  texture's other levels, and a cube face given in another format leaves the other faces alone.
- `glCopyTexImage2D` into an alpha, luminance-alpha, RGB or integer format puts each channel
  where it belongs, and copies from a slice of a 3D texture convert too.
- Attaching a level a texture does not have makes the framebuffer incomplete rather than an
  error; only a level no texture could have, or past an immutable texture's levels, is
  `GL_INVALID_VALUE`.
- Transform feedback records a whole array named without an index, such as `"attrib"` for
  `out vec4 attrib[4]`. Before, the program failed to link.
- A function that takes atomic counters counts on the counters it is given. Before, the
  shader failed to build.
- `GL_ACTIVE_UNIFORM_MAX_LENGTH` covers samplers, images and block members. It was 0 for a
  program whose only uniforms are samplers.
- Multisample textures keep `fixedsamplelocations`: `glGetTexLevelParameter` reports it, and a
  framebuffer mixing fixed and varying sample locations is `GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE`.
- `glGetBooleanv(GL_SAMPLE_MASK)` and `glGetIntegeri_v(GL_SAMPLE_MASK_VALUE)` work.
  `glGetMultisamplefv` gives `GL_INVALID_VALUE` for an index at or past `GL_SAMPLES`, and
  `glTexStorage2DMultisample` refuses the array target.
- `glFramebufferTextureLayer` takes only textures with layers or faces, and
  `glFramebufferParameteri` refuses sizes past the `GL_MAX_FRAMEBUFFER_*` limits.
- `glBlitFramebuffer` copies depth and stencil to and from the window's framebuffer, and a
  `GL_LINEAR` blit of depth or stencil is `GL_INVALID_OPERATION`.
- In compatibility contexts, `glGetTexImage` reads luminance and alpha formats back.
- A clean build generated entry points that read the current context as a plain global and
  crashed on the first GL call; they read the thread's context again.
- `glCopyTexSubImage*` into a texture of another format, such as from the window into an
  RGBA texture, converts the colour. It copied the bytes, so red and blue came out swapped.
- In compatibility contexts, a unit with texturing on and no complete texture now leaves the
  colour alone, as GL says. It drew black.
- `glBindBufferARB` and the rest of `GL_ARB_vertex_buffer_object`'s calls exist. The
  extension was listed without them.
- `glTextureParameter*` on a name from `glGenTextures` that was never bound gives
  `GL_INVALID_OPERATION`. It made a texture for it instead.
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