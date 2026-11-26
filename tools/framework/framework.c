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
 * framework.c
 * MGL
 *
 * MooGL.framework's own binary. It re-exports libmoogl and libGLU, so a
 * program whose OpenGL.framework load command now names MooGL.framework binds
 * to MooGL. AppKit and the rest of the system still link Apple's framework,
 * so every call into it is interposed (interpose.s), and so is dlopen of it,
 * which is how SDL and GLFW find their GL functions.
 */

#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

void *MGLgetCurrentContext(void);

extern void *mgl_fw_table[];
extern const char *const mgl_fw_names[];
extern const unsigned mgl_fw_count;

static char fw_path[PATH_MAX], gl_path[PATH_MAX], glu_path[PATH_MAX];

__attribute__((visibility("hidden"))) const char *mgl_fw_redirect(const char *path);

__attribute__((constructor)) static void mgl_fw_init(void)
{
    Dl_info info, self;
    const char *slash;
    void *moogl, *glu;

    if (!dladdr((const void *)MGLgetCurrentContext, &info) || info.dli_fname == NULL ||
        !dladdr((const void *)mgl_fw_redirect, &self) || self.dli_fname == NULL)
        return;

    slash = strrchr(info.dli_fname, '/');
    snprintf(gl_path, sizeof gl_path, "%s", info.dli_fname);
    snprintf(glu_path, sizeof glu_path, "%.*s/libGLU.1.dylib", slash ? (int)(slash - info.dli_fname) : 1,
             slash ? info.dli_fname : ".");
    moogl = dlopen(gl_path, RTLD_LAZY | RTLD_NOLOAD);
    glu = dlopen(glu_path, RTLD_LAZY | RTLD_NOLOAD);

    for (unsigned i = 0; i < mgl_fw_count; i++)
    {
        void *p = moogl ? dlsym(moogl, mgl_fw_names[i]) : NULL;

        if (p == NULL && glu)
            p = dlsym(glu, mgl_fw_names[i]);
        if (p)
            mgl_fw_table[i] = p;
    }

    // set last: until here, dlopen of Apple's framework stays Apple's
    snprintf(fw_path, sizeof fw_path, "%s", self.dli_fname);
}

// The path dlopen should open in place of Apple's. libGL and libGLU go
// straight to MooGL's, as dlsym on this framework would search Apple's
// framework too, which it links for the interposers.
__attribute__((visibility("hidden"))) const char *mgl_fw_redirect(const char *path)
{
    static const char dir[] = "/System/Library/Frameworks/OpenGL.framework/";
    const char *name;

    if (path == NULL || fw_path[0] == 0 || strncmp(path, dir, sizeof dir - 1) != 0)
        return path;

    name = strrchr(path, '/') + 1;
    if (strcmp(name, "OpenGL") == 0)
        return fw_path;
    if (strcmp(name, "libGL.dylib") == 0)
        return gl_path;
    if (strcmp(name, "libGLU.dylib") == 0)
        return glu_path;
    return path;
}

// dlopen finds @rpath and @loader_path from its caller's return address, so
// this ends in a jump to the real one with the caller's still in x30
__attribute__((naked)) static void *mgl_fw_dlopen(const char *path, int mode)
{
    __asm__ volatile(
        "stp x29, x30, [sp, #-32]!\n"
        "mov x29, sp\n"
        "str x1, [sp, #16]\n"
        "bl _mgl_fw_redirect\n"
        "ldr x1, [sp, #16]\n"
        "ldp x29, x30, [sp], #32\n"
        "b _dlopen\n");
}

__attribute__((used, section("__DATA,__interpose"))) static const struct {
    const void *replacement, *original;
} mgl_fw_dlopen_interpose = { (const void *)mgl_fw_dlopen, (const void *)dlopen };
