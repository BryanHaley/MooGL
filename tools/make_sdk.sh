#!/bin/bash
# Copyright (C) The MooGL Project
#
# Builds the MooGL SDK: dylibs, headers, a CMake package, an example and the
# licences, as dist/MooGL-SDK-<version>-macos-arm64.tar.gz.
#
#   tools/make_sdk.sh [version] [--no-deps]
#
# version is 0.1.0 unless given; a release passes its tag, and a leading "v" is
# dropped. It ends up in the SDK's name, the dylibs, GL_VERSION and the CMake
# package. --no-deps skips tools/build_deps.sh when
# the dependencies are already built. With dotnet installed, the macOS FSR 2
# C# package (tools/fsr2_csharp) goes in too.
#
# Signing, so macOS loads the downloaded libraries without complaint:
#   MOOGL_SIGN_IDENTITY   a "Developer ID Application: ..." identity in the keychain
# and then notarizing, with either
#   MOOGL_NOTARY_PROFILE  a profile saved with `xcrun notarytool store-credentials`
# or an App Store Connect API key:
#   MOOGL_NOTARY_KEY (path to the .p8), MOOGL_NOTARY_KEY_ID, MOOGL_NOTARY_ISSUER

set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

version=""
deps=1
for arg in "$@"; do
    case "$arg" in
        --no-deps) deps=0 ;;
        *) version="$arg" ;;
    esac
done
version="${version:-0.1.0}"
version="${version#v}"

export MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-13.0}"
build="$root/build/sdk"
name="MooGL-SDK-$version-macos-arm64"
out="$root/dist/$name"
fsr2="submodules/FidelityFX-FSR2-OpenGL/src/ffx-fsr2-api"

[ "$deps" = 1 ] && tools/build_deps.sh

# its own build folder, so the deployment target cannot mix with a dev build
# the SDK has OpenGL ES contexts, which a plain make leaves out
make -j"$(sysctl -n hw.ncpu)" build_dir=build/sdk MOOGL_VERSION="$version" OGL_ES=1 lib

# stock SDL2 builds too, but makes Apple's OpenGL contexts
otool -L "$build/libSDL2-2.0.0.dylib" | grep -q libmoogl ||
    { echo "libSDL2 is not linked to MooGL: submodules/SDL2 is not MooGL's SDL2 fork" >&2; exit 1; }

libs="libmoogl.dylib libglfw.dylib libSDL2-2.0.0.dylib libSDL3.0.dylib libglut.3.dylib libGLU.1.dylib libffx_fsr2_api.dylib"
# installed by their own builds, with their CMake packages
sfml3="$build/sfml3/install"
sfml2="$build/sfml2/install"
raylib="$build/raylib/install"


# the Makefile points the repo-root dylib links at whatever it built last
[ -f build/libmoogl.dylib ] && ln -fs build/libmoogl.dylib .

rm -rf "$out"
mkdir -p "$out/lib/cmake/MooGL" "$out/include/GL" "$out/include/KHR" "$out/include/GLFW" \
    "$out/include/ffx-fsr2/gl" "$out/include/ffx-fsr2/shaders" "$out/include/ffx-fsr2/glad" \
    "$out/include/imgui" "$out/include/GLUT" "$out/include/AGL" "$out/examples" "$out/licenses" \
    "$out/lib/cmake/glfw3" "$out/sfml2"

for lib in $libs; do
    cp "$build/$lib" "$out/lib/"
done
# programs built against a stock GLFW or SDL2 ask for them by these names
cp "$build/libimgui.a" "$out/lib/"
ln -s libglfw.dylib "$out/lib/libglfw.3.dylib"
ln -s libSDL2-2.0.0.dylib "$out/lib/libSDL2-2.0.dylib"
ln -s libSDL2-2.0.0.dylib "$out/lib/libSDL2.dylib"
ln -s libglut.3.dylib "$out/lib/libglut.dylib"
ln -s libGLU.1.dylib "$out/lib/libGLU.dylib"
ln -s libSDL3.0.dylib "$out/lib/libSDL3.dylib"
cp -a "$sfml3"/lib/libsfml-*.dylib "$raylib"/lib/libraylib*.dylib "$out/lib/"

# SFML 2.6 has the same names as SFML 3, so it gets its own folder; it finds
# libmoogl in the SDK's lib
cp -a "$sfml2/include" "$sfml2/lib" "$out/sfml2/"
install_name_tool -add_rpath @loader_path/../../lib "$out/sfml2/lib/libsfml-window.2.6.2.dylib"

cp include/GL/glcorearb.h "$out/include/GL/"
cp include/KHR/khrplatform.h "$out/include/KHR/"
cp include/MGLContext.h "$out/include/"
cp submodules/glfw/include/GLFW/glfw3.h submodules/glfw/include/GLFW/glfw3native.h "$out/include/GLFW/"
cp -R "$build/sdl2/install/include/SDL2" "$out/include/"
cp submodules/freeglut/include/GL/*.h "$out/include/GL/"
cp tools/sdk/GLUT/glut.h "$out/include/GLUT/"
cp submodules/glu/include/GL/glu.h tools/sdk/GL/gl.h tools/sdk/GL/glext.h "$out/include/GL/"
cp include/AGL/agl.h "$out/include/AGL/"
# Khronos's OpenGL ES headers, for code on ES contexts
cp -R tools/sdk/GLES2 tools/sdk/GLES3 "$out/include/"
cp -R "$build/sdl2/install/lib/cmake/SDL2" "$out/lib/cmake/"
cp -R "$build/sdl3/install/include/SDL3" "$out/include/"
cp -R "$build/sdl3/install/lib/cmake/SDL3" "$out/lib/cmake/"
cp -R "$sfml3/include/SFML" "$out/include/"
cp -R "$sfml3/lib/cmake/SFML" "$out/lib/cmake/"
cp "$raylib"/include/*.h "$out/include/"
cp -R "$raylib/lib/cmake/raylib" "$out/lib/cmake/"
# raylib's package names the libmoogl it was built against by its full path
sed -i '' "s|\"$PWD/build/sdk/libmoogl.dylib\"|\"\${_IMPORT_PREFIX}/lib/libmoogl.dylib\"|" \
    "$out/lib/cmake/raylib/raylib-targets.cmake"
cp tools/sdk/glfw3/*.cmake "$out/lib/cmake/glfw3/"
cp "$build/sdl2/install/lib/libSDL2main.a" "$out/lib/"

cp submodules/imgui/imgui.h submodules/imgui/imconfig.h submodules/imgui/imgui_internal.h submodules/imgui/imstb_*.h \
    submodules/imgui/backends/imgui_impl_{glfw,sdl2,opengl3}.h "$out/include/imgui/"

cp "$fsr2"/*.h "$out/include/ffx-fsr2/"
cp "$fsr2"/shaders/*.h "$out/include/ffx-fsr2/shaders/"
cp "$fsr2"/gl/ffx_fsr2_gl.h "$out/include/ffx-fsr2/gl/"
cp -R "$fsr2"/gl/external/glad/include/ "$out/include/ffx-fsr2/glad/"

cp tools/sdk/MooGLConfig.cmake "$out/lib/cmake/MooGL/"
cat > "$out/lib/cmake/MooGL/MooGLConfigVersion.cmake" <<EOF
set(PACKAGE_VERSION "$version")
if(PACKAGE_FIND_VERSION VERSION_GREATER PACKAGE_VERSION)
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
    if(PACKAGE_FIND_VERSION STREQUAL PACKAGE_VERSION)
        set(PACKAGE_VERSION_EXACT TRUE)
    endif()
endif()
EOF
cp -R examples/triangle examples/sdl2_triangle examples/sdl3_triangle examples/imgui examples/imgui_sdl2 \
    examples/glut_teapot examples/sfml_shapes examples/raylib_shapes "$out/examples/"
rm -rf "$out"/examples/*/build

cp LICENSE "$out/licenses/MooGL.txt"
cp submodules/glfw/LICENSE.md "$out/licenses/GLFW.txt"
cp submodules/SPIRV-Cross/LICENSE "$out/licenses/SPIRV-Cross.txt"
cp submodules/SPIRV-Headers/LICENSE "$out/licenses/SPIRV-Headers.txt"
cp submodules/SPIRV-Tools/LICENSE "$out/licenses/SPIRV-Tools.txt"
cp submodules/glslang/LICENSE.txt "$out/licenses/glslang.txt"
cp submodules/FidelityFX-FSR2-OpenGL/LICENSE.txt "$out/licenses/FidelityFX-FSR2.txt"
cp submodules/SDL2/LICENSE.txt "$out/licenses/SDL2.txt"
cp submodules/imgui/LICENSE.txt "$out/licenses/DearImGui.txt"
cp submodules/freeglut/COPYING "$out/licenses/freeglut.txt"
cp submodules/SDL3/LICENSE.txt "$out/licenses/SDL3.txt"
cp submodules/SFML/license.md "$out/licenses/SFML.txt"
cp submodules/SFML2/license.md "$out/licenses/SFML2.txt"
cp submodules/raylib/LICENSE "$out/licenses/raylib.txt"
# SFML 3 builds these into its graphics library; SFML 2.6 ships FreeType as a framework
cp "$build/sfml3/_deps/freetype-src/docs/FTL.TXT" "$out/licenses/FreeType.txt"
cp "$build/sfml3/_deps/harfbuzz-src/COPYING" "$out/licenses/HarfBuzz.txt"
cp "$build/sfml3/_deps/sheenbidi-src/LICENSE" "$out/licenses/SheenBidi.txt"
# GLU has no licence file; it is the comment at the top of each source
sed -n '2,/^ \*\//s/^ \*\{0,1\} \{0,1\}//p' submodules/glu/src/libutil/project.c | sed '$d' > "$out/licenses/GLU.txt"

if command -v dotnet >/dev/null; then
    BUILD_DIR="$build" tools/fsr2_csharp/build.sh
    mkdir -p "$out/nuget"
    cp "$build"/nuget/FFX_FSR2.*-macos.nupkg "$out/nuget/"
fi

cat > "$out/README.md" <<EOF
# MooGL SDK $version

OpenGL 4.6 core on Metal, for Apple silicon Macs on macOS $MACOSX_DEPLOYMENT_TARGET or later.

- \`lib/\`: libmoogl (OpenGL), and libglfw, libSDL2, libSDL3, SFML 3, raylib and libglut (freeglut), built to make
  MooGL contexts; libGLU, libimgui (Dear ImGui with docking, drawing with MooGL) and the FSR 2 libraries on MetalFX
- \`include/\`: GL/glcorearb.h, GLFW, SDL2, SDL3, SFML, raylib, GLUT (\`GL/freeglut.h\`, \`GLUT/glut.h\`), \`GL/gl.h\`,
  \`GL/glext.h\`, \`GL/glu.h\`, GLES2 and GLES3, AGL/agl.h, Dear ImGui, MGLContext.h, and AMD's FSR 2
  headers
- \`lib/cmake\`: \`find_package(MooGL)\` with MooGL::GL, MooGL::glfw, MooGL::glut, MooGL::GLU, MooGL::imgui and MooGL::FSR2,
  and the packages of SDL2, SDL3, SFML, raylib and glfw3 (MooGL's GLFW)
- \`sfml2/\`: SFML 2.6, with its own \`include/\`, \`lib/\` and CMake package
- \`examples/\`: \`cmake -B build -DCMAKE_PREFIX_PATH=\$PWD/../.. && cmake --build build\`
- \`nuget/\`: the FFX_FSR2 C# package for macOS, if present

The full guide is at https://github.com/BryanHaley/MGL.
EOF

# the examples have to build against what was just packed
cc -O1 -o "$build/triangle" examples/triangle/main.c -I"$out/include" -L"$out/lib" -lglfw -lmoogl \
    -Wl,-rpath,"$out/lib"
cc -O1 -o "$build/sdl2_triangle" examples/sdl2_triangle/main.c -I"$out/include" -L"$out/lib" -lSDL2 -lmoogl \
    -Wl,-rpath,"$out/lib"
cc -O1 -o "$build/glut_teapot" examples/glut_teapot/main.c -I"$out/include" -L"$out/lib" -lglut -lmoogl \
    -Wl,-rpath,"$out/lib"
cc -O1 -o "$build/sdl3_triangle" examples/sdl3_triangle/main.c -I"$out/include" -L"$out/lib" -lSDL3 -lmoogl \
    -Wl,-rpath,"$out/lib"
c++ -std=c++17 -O1 -o "$build/sfml_shapes" examples/sfml_shapes/main.cpp -I"$out/include" -L"$out/lib" \
    -lsfml-graphics -lsfml-window -lsfml-system -Wl,-rpath,"$out/lib"
cc -O1 -o "$build/raylib_shapes" examples/raylib_shapes/main.c -I"$out/include" -L"$out/lib" -lraylib \
    -Wl,-rpath,"$out/lib"
# code from other systems: <GL/gl.h>, <GL/glext.h> and <GL/glu.h> together
cc -std=c99 -Wall -Wextra -pedantic -Werror -DGL_GLEXT_PROTOTYPES -x c -o "$build/glext_check" - \
    -I"$out/include" -L"$out/lib" -lGLU -lmoogl <<'EOF'
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>
int main(void)
{
    GLuint b = 0;
    PFNGLCREATEBUFFERSPROC create = glCreateBuffers;
    create(1, &b);
    glDispatchCompute(1, 1, 1);
    gluPerspective(60.0, 1.0, 1.0, 10.0);
    return 0;
}
EOF
c++ -std=c++17 -O1 -o "$build/imgui_demo" examples/imgui/main.cpp -I"$out/include" -I"$out/include/imgui" \
    -L"$out/lib" -limgui -lglfw -lmoogl -Wl,-rpath,"$out/lib"
c++ -std=c++17 -O1 -o "$build/imgui_sdl2_demo" examples/imgui_sdl2/main.cpp -I"$out/include" -I"$out/include/imgui" \
    -I"$out/include/SDL2" -L"$out/lib" -limgui -lSDL2 -lmoogl -Wl,-rpath,"$out/lib"

if [ -n "${MOOGL_SIGN_IDENTITY:-}" ]; then
    for lib in "$out"/lib/*.dylib "$out"/sfml2/lib/*.dylib; do
        [ -L "$lib" ] || codesign --force --timestamp --options runtime --sign "$MOOGL_SIGN_IDENTITY" "$lib"
    done
    codesign --force --timestamp --options runtime --sign "$MOOGL_SIGN_IDENTITY" "$out/sfml2/lib/freetype.framework"
else
    codesign --force --sign - "$out/sfml2/lib/libsfml-window.2.6.2.dylib"
fi

notary=()
if [ -n "${MOOGL_NOTARY_PROFILE:-}" ]; then
    notary=(--keychain-profile "$MOOGL_NOTARY_PROFILE")
elif [ -n "${MOOGL_NOTARY_KEY:-}" ]; then
    notary=(--key "$MOOGL_NOTARY_KEY" --key-id "$MOOGL_NOTARY_KEY_ID" --issuer "$MOOGL_NOTARY_ISSUER")
fi

if [ -n "${MOOGL_SIGN_IDENTITY:-}" ] && [ ${#notary[@]} -gt 0 ]; then
    rm -f "$build/notarize.zip"
    ditto -c -k --keepParent "$out" "$build/notarize.zip"
    xcrun notarytool submit "$build/notarize.zip" "${notary[@]}" --wait | tee "$build/notarize.log"
    if ! grep -q "status: Accepted" "$build/notarize.log"; then
        id=$(awk '/^ *id: /{print $2; exit}' "$build/notarize.log")
        [ -n "$id" ] && xcrun notarytool log "$id" "${notary[@]}"
        echo "notarization failed" >&2
        exit 1
    fi
else
    echo "not signed and notarized: macOS will quarantine these libraries when downloaded"
fi

mkdir -p dist
tar -C dist -czf "dist/$name.tar.gz" "$name"
echo "built dist/$name.tar.gz"
