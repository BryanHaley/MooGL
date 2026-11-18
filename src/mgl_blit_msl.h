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
 * mgl_blit_msl.h
 * MGL
 *
 * A textured triangle, for the framebuffer blits Metal's own copy cannot do:
 * it only copies between matching pixel formats, and the window is BGRA8 while
 * almost nothing an app renders into is.
 */

#ifndef mgl_blit_msl_h
#define mgl_blit_msl_h

static const char *const mgl_blit_msl_source =
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"struct MGLBlitVary {\n"
"    float4 position [[position]];\n"
"    float2 uv;\n"
"};\n"
"\n"
"struct MGLBlitArgs {\n"
"    float2 src_origin;   // in source texels\n"
"    float2 src_size;\n"
"};\n"
"\n"
"vertex MGLBlitVary mglBlitVertex(uint vid [[vertex_id]],\n"
"                                 constant MGLBlitArgs &args [[buffer(0)]],\n"
"                                 texture2d<float> src [[texture(0)]])\n"
"{\n"
"    // one oversized triangle covering the target\n"
"    float2 p = float2((vid << 1) & 2, vid & 2);\n"
"\n"
"    MGLBlitVary out;\n"
"    out.position = float4(p * 2.0 - 1.0, 0.0, 1.0);\n"
"\n"
"    float2 texel = float2(src.get_width(), src.get_height());\n"
"    // the target's top row takes the source's first row\n"
"    out.uv = (args.src_origin + float2(p.x, 1.0 - p.y) * args.src_size) / texel;\n"
"\n"
"    return out;\n"
"}\n"
"\n"
"fragment float4 mglBlitFragment(MGLBlitVary in [[stage_in]],\n"
"                                texture2d<float> src [[texture(0)]],\n"
"                                sampler samp [[sampler(0)]])\n"
"{\n"
"    return src.sample(samp, in.uv);\n"
"}\n"
"\n"
"// GL picks a nearest texel by flooring the source position of each pixel's\n"
"// centre; doing that exactly keeps edges where GL puts them\n"
"struct MGLBlitNearestArgs {\n"
"    float2 src_origin;\n"
"    float2 src_size;\n"
"    float2 dst_origin;\n"
"    float2 dst_size;\n"
"    float flip;\n"
"};\n"
"\n"
"fragment float4 mglBlitFragmentNearest(MGLBlitVary in [[stage_in]],\n"
"                                       texture2d<float> src [[texture(0)]],\n"
"                                       constant MGLBlitNearestArgs &args [[buffer(0)]])\n"
"{\n"
"    float2 pix = floor(in.position.xy) - args.dst_origin;\n"
"    if (args.flip != 0.0)\n"
"        pix.y = args.dst_size.y - 1.0 - pix.y;\n"
"    float2 at = floor(args.src_origin + (pix + 0.5) * (args.src_size / args.dst_size));\n"
"    at = clamp(at, float2(0.0), float2(src.get_width() - 1, src.get_height() - 1));\n"
"    return src.read(uint2(at));\n"
"}\n"
"\n"
"// integer texels are read, not filtered\n"
"vertex MGLBlitVary mglBlitVertexInt(uint vid [[vertex_id]],\n"
"                                    constant MGLBlitArgs &args [[buffer(0)]],\n"
"                                    constant float2 &texel [[buffer(1)]])\n"
"{\n"
"    float2 p = float2((vid << 1) & 2, vid & 2);\n"
"\n"
"    MGLBlitVary out;\n"
"    out.position = float4(p * 2.0 - 1.0, 0.0, 1.0);\n"
"    out.uv = (args.src_origin + float2(p.x, 1.0 - p.y) * args.src_size) / texel;\n"
"\n"
"    return out;\n"
"}\n"
"\n"
"fragment int4 mglBlitFragmentInt(MGLBlitVary in [[stage_in]],\n"
"                                 texture2d<int> src [[texture(0)]])\n"
"{\n"
"    return src.read(uint2(in.uv * float2(src.get_width(), src.get_height())));\n"
"}\n"
"\n"
"fragment uint4 mglBlitFragmentUint(MGLBlitVary in [[stage_in]],\n"
"                                   texture2d<uint> src [[texture(0)]])\n"
"{\n"
"    return src.read(uint2(in.uv * float2(src.get_width(), src.get_height())));\n"
"}\n"
"\n"
;

#endif /* mgl_blit_msl_h */
