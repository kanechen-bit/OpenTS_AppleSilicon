#!/usr/bin/env python3
"""Transpile tools/artcnn_c4f16.glsl to Metal compute shaders and wrap them in bgfx CSH blobs.

ArtCNN C4F16 (MIT License, Copyright (c) 2024 Joao Chrisostomo, Kacper Michajlow)
provides 7 convolutional feature-extraction/reconstruction passes over luma plus
Depth-To-Space 2x upscaling. This script emits 9 standalone Metal compute kernels
(rgb_to_luma, conv0..conv6, and fused d2s_recombine), validates every kernel with
xcrun metal, packs each into a bgfx v11 compute shader container ('C','S','H',11),
and writes code/cs_artcnn.bin.h.

Run after editing this script or tools/artcnn_c4f16.glsl:

    python3 tools/make_artcnn_bin.py
"""

from __future__ import annotations

import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GLSL_SOURCE = ROOT / "tools" / "artcnn_c4f16.glsl"
HEADER = ROOT / "code" / "cs_artcnn.bin.h"

BGFX_SHADER_BIN_VERSION = 11
NUM_THREADS = (16, 16, 1)

COMMON_HELPERS = """#include <metal_stdlib>
using namespace metal;

inline half read_luma_h(texture2d<float, access::read> t, int2 p, int dx, int dy)
{
    int2 c = clamp(p + int2(dx, dy), int2(0), int2(int(t.get_width()) - 1, int(t.get_height()) - 1));
    return half(t.read(uint2(c)).x);
}

inline half4 read_feat_h(texture2d<half, access::read> t, int2 lr_p, int sub_c, int dx, int dy)
{
    int2 lr_max = int2(int(t.get_width()) / 2 - 1, int(t.get_height()) / 2 - 1);
    int2 c = clamp(lr_p + int2(dx, dy), int2(0), lr_max) * 2 + int2(sub_c & 1, sub_c >> 1);
    return t.read(uint2(c));
}
"""

RGB_TO_LUMA_MSL = """#include <metal_stdlib>
using namespace metal;

kernel void xlatMtlMain(
    texture2d<float, access::sample> src_rgb [[texture(0)]],
    texture2d<float, access::write> dst_luma [[texture(1)]],
    uint2 gid [[thread_position_in_grid]])
{
    uint w = dst_luma.get_width();
    uint h = dst_luma.get_height();
    if (gid.x >= w || gid.y >= h) {
        return;
    }
    float3 rgb = src_rgb.read(gid).rgb;
    float y = clamp(dot(rgb, float3(0.2126f, 0.7152f, 0.0722f)), 0.0f, 1.0f);
    dst_luma.write(float4(y, 0.0f, 0.0f, 1.0f), gid);
}
"""

D2S_RECOMBINE_MSL = """#include <metal_stdlib>
using namespace metal;

kernel void xlatMtlMain(
    texture2d<half, access::read> in_d2s [[texture(0)]],
    texture2d<float, access::sample> src_rgb [[texture(1)]],
    texture2d<float, access::write> out_rgba [[texture(2)]],
    uint2 gid [[thread_position_in_grid]])
{
    uint out_w = out_rgba.get_width();
    uint out_h = out_rgba.get_height();
    if (gid.x >= out_w || gid.y >= out_h) {
        return;
    }
    half4 v = in_d2s.read(gid >> 1);
    uint sub = (gid.y & 1u) * 2u + (gid.x & 1u);
    float y_hr = clamp(float(sub == 0u ? v.x : (sub == 1u ? v.y : (sub == 2u ? v.z : v.w))), 0.0f, 1.0f);
    constexpr sampler linear_clamp(coord::normalized, filter::linear, address::clamp_to_edge);
    float2 uv = (float2(gid) + 0.5f) / float2(out_w, out_h);
    float3 rgb_lr = src_rgb.sample(linear_clamp, uv).rgb;
    float y_lr = dot(rgb_lr, float3(0.2126f, 0.7152f, 0.0722f));
    out_rgba.write(float4(clamp(rgb_lr + (y_hr - y_lr), 0.0f, 1.0f), 1.0f), gid);
}
"""


def mat4_to_msl(vec_type: str, mat_type: str, scalars_str: str) -> str:
    nums = [s.strip() for s in scalars_str.split(",")]
    if len(nums) != 16:
        raise ValueError(f"Expected 16 scalars in matrix, got {len(nums)}")
    cols = [f"{vec_type}({','.join(nums[c * 4:(c + 1) * 4])})" for c in range(4)]
    return f"{mat_type}({','.join(cols)})"


def convert_all_matrices(body: str, src_pattern: str, vec_type: str, mat_type: str) -> str:
    pattern = re.compile(rf"\b{src_pattern}\s*\(([^()]+)\)")
    return pattern.sub(lambda m: mat4_to_msl(vec_type, mat_type, m.group(1)), body)


def transpile_conv_passes(glsl_path: Path) -> list[tuple[str, str]]:
    text = glsl_path.read_text()
    blocks = re.split(r"(?=^//!DESC ArtCNN)", text, flags=re.MULTILINE)
    conv_shaders: list[tuple[str, str]] = []

    for blk in blocks:
        if "//!DESC ArtCNN" not in blk:
            continue
        desc = re.search(r"^//!DESC\s+(.+)$", blk, re.MULTILINE).group(1).strip()
        if "Depth-To-Space" in desc:
            continue

        binds = [m.strip() for m in re.findall(r"^//!BIND\s+(.+)$", blk, re.MULTILINE)]
        is_2x = bool(re.search(r"^//!WIDTH\s+LUMA\.w\s+2\.0\s*\*", blk, re.MULTILINE))

        match = re.search(r"barrier\(\);\s*(.*?)\s*const ivec2 output_base\s*=(.*)$", blk, re.DOTALL)
        if not match:
            raise ValueError(f"Could not extract body for {desc}")
        math_body = match.group(1)
        tail = match.group(2)

        math_body = convert_all_matrices(math_body, r"M4", "half4", "half4x4")
        math_body = re.sub(r"\bV4\b", "half4", math_body)
        math_body = re.sub(r"\bF\b", "half", math_body)

        def repl_inp(m: re.Match[str]) -> str:
            sub_c = int(m.group(1))
            ky = int(m.group(2))
            kx = int(m.group(3))
            dx, dy = kx - 1, ky - 1
            if binds == ["LUMA"]:
                assert sub_c == 0
                return f"read_luma_h(in_luma, int2(gid), {dx}, {dy})"
            if len(binds) == 1:
                return f"read_feat_h(in_tex, int2(gid), {sub_c}, {dx}, {dy})"
            if set(binds) == {"conv2d", "conv2d_5"}:
                return (
                    f"(read_feat_h(in_prev, int2(gid), {sub_c}, {dx}, {dy}) + "
                    f"read_feat_h(in_skip, int2(gid), {sub_c}, {dx}, {dy}))"
                )
            raise ValueError(f"Unexpected binds in {desc}: {binds}")

        math_body = re.sub(
            r"inp\[(\d+)\]\[local_xy\.y\s*\+\s*(\d+)\]\[local_xy\.x\s*\+\s*(\d+)\]",
            repl_inp,
            math_body,
        )

        writes: list[str] = []
        for sm in re.finditer(
            r"imageStore\(out_image,\s*output_base\s*\+\s*ivec2\((\d+),\s*(\d+)\),\s*([^;]+)\);",
            tail,
        ):
            ox, oy, expr = int(sm.group(1)), int(sm.group(2)), sm.group(3)
            expr = re.sub(r"\bV4\b", "half4", expr)
            if is_2x:
                writes.append(f"    out_tex.write({expr}, gid * 2 + uint2({ox}, {oy}));")
            else:
                assert ox == 0 and oy == 0
                writes.append(f"    out_tex.write({expr}, gid);")
        if not writes:
            raise ValueError(f"No imageStore writes parsed in {desc}")

        idx = len(conv_shaders)
        if binds == ["LUMA"]:
            params = [
                "    texture2d<float, access::read> in_luma [[texture(0)]]",
                "    texture2d<half, access::write> out_tex [[texture(1)]]",
            ]
        elif set(binds) == {"conv2d", "conv2d_5"}:
            params = [
                "    texture2d<half, access::read> in_prev [[texture(0)]]",
                "    texture2d<half, access::read> in_skip [[texture(1)]]",
                "    texture2d<half, access::write> out_tex [[texture(2)]]",
            ]
        else:
            params = [
                "    texture2d<half, access::read> in_tex [[texture(0)]]",
                "    texture2d<half, access::write> out_tex [[texture(1)]]",
            ]
        params.append("    uint2 gid [[thread_position_in_grid]]")

        if is_2x:
            bounds = "    if (gid.x >= out_tex.get_width() / 2 || gid.y >= out_tex.get_height() / 2) {\n        return;\n    }"
        else:
            bounds = "    if (gid.x >= out_tex.get_width() || gid.y >= out_tex.get_height()) {\n        return;\n    }"

        indented = "\n".join("    " + ln.strip() for ln in math_body.strip().splitlines() if ln.strip())
        msl = (
            COMMON_HELPERS
            + "\nkernel void xlatMtlMain(\n"
            + ",\n".join(params)
            + ")\n{\n"
            + bounds + "\n"
            + indented + "\n"
            + "\n".join(writes) + "\n"
            + "}\n"
        )
        conv_shaders.append((f"cs_artcnn_conv{idx}_mtl", msl))

    if len(conv_shaders) != 7:
        raise SystemExit(f"Expected 7 conv passes in {glsl_path}, found {len(conv_shaders)}")
    return conv_shaders


def build_compute_blob(msl: bytes) -> bytes:
    magic = ord("C") | (ord("S") << 8) | (ord("H") << 16) | (BGFX_SHADER_BIN_VERSION << 24)
    out = bytearray()
    out += struct.pack("<III", magic, 0, 0)
    out += struct.pack("<H", 0)
    out += struct.pack("<HHH", *NUM_THREADS)
    out += struct.pack("<I", len(msl))
    out += msl
    out += b"\x00"
    return bytes(out)


def parse_like_bgfx(name: str, blob: bytes) -> None:
    off = 0
    magic, hash_in, hash_out = struct.unpack_from("<III", blob, off)
    off += 12
    if (magic & 0xFFFFFF) != (ord("C") | (ord("S") << 8) | (ord("H") << 16)):
        raise SystemExit(f"{name}: bad magic {magic:08x}")
    count = struct.unpack_from("<H", blob, off)[0]
    off += 2
    if count != 0:
        raise SystemExit(f"{name}: unexpected uniforms {count}")
    threads = struct.unpack_from("<HHH", blob, off)
    off += 6
    size = struct.unpack_from("<I", blob, off)[0]
    off += 4
    code = blob[off:off + size + 1]
    if len(code) != size + 1 or code[-1] != 0 or off + size + 1 != len(blob):
        raise SystemExit(f"{name}: size mismatch")
    print(f"  {name}: threads={threads} shaderSize={size} total={len(blob)} hashIn={hash_in:x} hashOut={hash_out:x}")


def verify_metal_compile(shaders: list[tuple[str, str]]) -> None:
    validator_src = """#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

int main(int argc, const char **argv) {
    @autoreleasepool {
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        if (!dev) {
            fprintf(stderr, "No Metal device available; skipping runtime MSL check\\n");
            return 0;
        }
        for (int i = 1; i < argc; i++) {
            NSString *path = [NSString stringWithUTF8String:argv[i]];
            NSError *err = nil;
            NSString *src = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:&err];
            if (!src) {
                fprintf(stderr, "Failed reading %s: %s\\n", argv[i], [[err localizedDescription] UTF8String]);
                return 1;
            }
            id<MTLLibrary> lib = [dev newLibraryWithSource:src options:nil error:&err];
            if (!lib) {
                fprintf(stderr, "MSL compile error in %s:\\n%s\\n", argv[i], [[err localizedDescription] UTF8String]);
                return 1;
            }
            id<MTLFunction> fn = [lib newFunctionWithName:@"xlatMtlMain"];
            if (!fn) {
                fprintf(stderr, "Missing xlatMtlMain in %s\\n", argv[i]);
                return 1;
            }
            id<MTLComputePipelineState> pso = [dev newComputePipelineStateWithFunction:fn error:&err];
            if (!pso) {
                fprintf(stderr, "Pipeline error in %s: %s\\n", argv[i], [[err localizedDescription] UTF8String]);
                return 1;
            }
        }
        return 0;
    }
}
"""
    with tempfile.TemporaryDirectory() as tmp:
        tmp_dir = Path(tmp)
        c_path = tmp_dir / "validate_msl.m"
        bin_path = tmp_dir / "validate_msl"
        c_path.write_text(validator_src)
        build_proc = subprocess.run(
            ["clang", "-x", "objective-c", "-fobjc-arc", "-framework", "Metal", "-framework", "Foundation",
             str(c_path), "-o", str(bin_path)],
            capture_output=True,
            text=True,
        )
        if build_proc.returncode != 0:
            raise SystemExit(f"Validator build failed:\n{build_proc.stderr}")

        metal_paths: list[str] = []
        for symbol, msl in shaders:
            p = tmp_dir / f"{symbol}.metal"
            p.write_text(msl)
            metal_paths.append(str(p))

        run_proc = subprocess.run([str(bin_path), *metal_paths], capture_output=True, text=True)
        if run_proc.returncode != 0:
            raise SystemExit(f"Metal runtime compilation failed:\n{run_proc.stderr}")
        if run_proc.stderr.strip():
            print("  " + run_proc.stderr.strip())


def emit_array(symbol: str, blob: bytes) -> list[str]:
    lines = [f"static const uint8_t {symbol}[{len(blob)}] =", "{"]
    for i in range(0, len(blob), 16):
        chunk = blob[i:i + 16]
        hexes = ", ".join(f"0x{b:02x}" for b in chunk)
        comma = "" if i + 16 >= len(blob) else ","
        lines.append(f"\t{hexes}{comma}")
    lines.append("};")
    lines.append("")
    return lines


def main() -> int:
    if not GLSL_SOURCE.is_file():
        raise SystemExit(f"missing {GLSL_SOURCE}")

    shaders: list[tuple[str, str]] = [("cs_artcnn_rgb_to_luma_mtl", RGB_TO_LUMA_MSL)]
    shaders.extend(transpile_conv_passes(GLSL_SOURCE))
    shaders.append(("cs_artcnn_d2s_recombine_mtl", D2S_RECOMBINE_MSL))

    header_lines = [
        "// Generated by tools/make_artcnn_bin.py from tools/artcnn_c4f16.glsl.",
        "// Derived from ArtCNN C4F16 (MIT License, Copyright (c) 2024 Joao Chrisostomo, Kacper Michajlow).",
        "// bgfx compute shader containers (CSH v11), Metal variant (MSL source compiled at runtime).",
        "// Do not edit by hand; edit tools/make_artcnn_bin.py and re-run it.",
        "",
    ]

    print("Validating ArtCNN Metal shaders (newLibraryWithSource + PSO + ShaderMtl::create parse):")
    verify_metal_compile(shaders)
    for symbol, msl in shaders:
        blob = build_compute_blob(msl.encode("utf-8"))
        parse_like_bgfx(symbol, blob)
        header_lines.extend(emit_array(symbol, blob))

    HEADER.write_text("\n".join(header_lines))
    print(f"Wrote {HEADER}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
