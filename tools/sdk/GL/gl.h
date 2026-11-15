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
 * <GL/gl.h> for code written for other systems, and for <GL/glu.h>, which
 * includes it. GL up to 2.1 is Apple's gl.h, the rest Khronos's glext.h, as
 * Mesa's gl.h does; the calls land in libmoogl.
 */

#ifndef MOOGL_GL_GL_H
#define MOOGL_GL_GL_H

#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif

// GL_GLEXT_LEGACY keeps Apple's glext.h out; it clashes with Khronos's
#ifdef GL_GLEXT_LEGACY
#include <OpenGL/gl.h>
#else
#define GL_GLEXT_LEGACY
#include <OpenGL/gl.h>
#undef GL_GLEXT_LEGACY
#define MOOGL_GL_GLEXT
#endif

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

#ifdef MOOGL_GL_GLEXT
#include <GL/glext.h>
#endif

#endif
