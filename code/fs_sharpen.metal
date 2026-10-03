/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Contrast-adaptive sharpening for the supersampling filter's present pass.
//
// ScaleMode=SuperSample renders the frame at twice the window's whole multiple and
// lets the window shrink it two to one. That box average is the right answer for
// staircase edges and the wrong answer for micro-contrast: thin one-texel lines,
// dialog text and flat fills lose their pop. This pass puts it back.
//
// The kernel is a five-tap unsharp mask clamped to the neighbourhood's range. The
// clamp is what makes it contrast adaptive: the sharpened value can never leave
// [min, max] of the centre and its four neighbours, so an edge is pulled toward its
// full contrast but never past it — no ringing, no halos, no blown highlights. With
// u_sharpness at zero the pass degenerates to the plain two-to-one minify.
//
// The layout mirrors what bgfx's own shaderc emits for this engine's single quad
// pipeline (see fs_ocornut_imgui): the same vertex shader's varyings arrive through
// [[stage_in]], the frame texture is the combined sampler s_tex at slot 0, and the
// uniforms live in a struct bound at buffer 0. u_sharpness.x carries the sharpening
// knob from SUN.INI (Video/Sharpness, 0..1).
//
// This file is text, not a compiled library: the Metal runtime compiler turns it
// into a library once at program creation (renderer_mtl.cpp hands the stored bytes
// to newLibraryWithSource). tools/make_fs_sharpen_bin.py wraps it in bgfx's shader
// container to produce fs_sharpen.bin.h.

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct _Global
{
    float4 u_sharpness;
};

struct xlatMtlMain_out
{
    float4 bgfx_FragData0 [[color(0)]];
};

struct xlatMtlMain_in
{
    float4 v_color0 [[user(locn0)]];
    float2 v_texcoord0 [[user(locn1)]];
};

fragment xlatMtlMain_out xlatMtlMain(xlatMtlMain_in in [[stage_in]],
                                     constant _Global& _mtl_u [[buffer(0)]],
                                     texture2d<float> s_tex [[texture(0)]],
                                     sampler s_texSampler [[sampler(0)]])
{
    xlatMtlMain_out out = {};

    float2 uv = in.v_texcoord0;
    float2 texel = 1.0 / float2(s_tex.get_width(), s_tex.get_height());

    // The centre tap is the window pixel the box minify produced; the four
    // neighbours are one target texel out, each a shifted two-by-two box.
    float4 c = s_tex.sample(s_texSampler, uv);
    float4 l = s_tex.sample(s_texSampler, uv + float2(-texel.x, 0.0));
    float4 r = s_tex.sample(s_texSampler, uv + float2( texel.x, 0.0));
    float4 t = s_tex.sample(s_texSampler, uv + float2(0.0, -texel.y));
    float4 b = s_tex.sample(s_texSampler, uv + float2(0.0,  texel.y));

    float4 mn = min(c, min(min(l, r), min(t, b)));
    float4 mx = max(c, max(max(l, r), max(t, b)));

    float4 blur = (l + r + t + b) * 0.25;
    float amount = _mtl_u.u_sharpness.x * 3.0;
    float4 sharpened = c + (c - blur) * amount;

    out.bgfx_FragData0 = float4(clamp(sharpened.rgb, mn.rgb, mx.rgb), c.a);
    return out;
}
