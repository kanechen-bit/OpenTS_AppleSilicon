# Image quality: modern anti-aliasing and upscaling for a 640x480 frame

Status: survey plus one shipped change. The CAS-style sharpening pass of the
recommended ladder (row 1) is implemented: `code/fs_sharpen.metal`, wrapped into
`code/fs_sharpen.bin.h` by `tools/make_fs_sharpen_bin.py`, driven by
`Video/Sharpness` in `SUN.INI` (0.0 off through 1.0). The rest of this page
records what the trade is, which techniques exist, and what each one costs to
add to this tree. [UI system design](UI_DESIGN.md) owns the frame path;
[Project direction](DIRECTION.md) owns the wider plan.

Survey date: 2026-10-02. Hardware assumed: Apple Silicon (M-series) macOS.

## 1. What the picture actually is

Tiberian Sun is not hand-drawn pixel art. It is a software rasteriser drawing
real polygons and textured sprites into a 640x480 16-bit surface, which then
makes two stops on its way to the window:

| Stage | What happens | Code |
| --- | --- | --- |
| 1. Render | CPU rasteriser fills `VisibleSurface` at 640x480, `RGB565` | `_surface.cpp`, `_alpha.cpp` |
| 2. Upload | `bgfx::updateTexture2D` copies it into a `B5G6R5` texture | `bgfxbackend.cpp:582` |
| 3. Magnify | one textured quad blows the texture up to the next whole multiple **x2**, sampler `LINEAR` | `VIEW_PRESCALE`, `bgfxbackend.cpp:634` |
| 4. Minify | the present quad samples that target back down 2:1, sampler `LINEAR` -> four samples a quarter texel apart | `VIEW_PRESENT`, `bgfxbackend.cpp:664` |
| 5. Present | `bgfx::frame()` | `bgfxbackend.cpp:723` |

`ScaleMode=SuperSample` is stage 3 and 4 together. Verified by reading the
present path; the output was checked against a numeric model of the same
filter and matched to 100.0000% of pixels.

Two things follow from this that shape every decision below.

**The GPU here rasterises nothing but two fullscreen quads.** MSAA and its
variants - including RGSS - are inert, because there is no geometry edge for
them to cover. That was established earlier and still holds.

**There is no fragment shader of our own.** The only program in the back end is
bgfx's `vs_ocornut_imgui` / `fs_ocornut_imgui`, which does exactly one
`texture2D` tap (`bgfxbackend.cpp:375` to `:384`). Stage 3 and stage 4 are pure
sampler state. Every modern technique in section 3 is a shader, so "add a
filter" means "add a program", and section 5 covers what that costs.

## 2. What "jagged" means in this specific picture

There are two separate artifacts and they want different fixes.

1. **Geometric aliasing of the low-resolution render.** A diagonal wall or a
   ship hull is real 3D geometry sampled once per texel, so it arrives with
   hard stair-steps already burned in at 640x480. This is what supersampling
   fixes: render or sample it more densely and average back down.
2. **Texel staircase from magnification.** Even a perfectly rendered frame
   goes pointy the moment 640x480 is blown up onto a Retina panel where the
   scale factor is not an integer. This is what ratio-aware or edge-aware
   magnification fixes, and it is why integer mode (`ScaleMode=PixelArt`) can
   be crisp *and* ugly at the same time.

SuperSample already handles 1 correctly. What it does not do is put any
*sharpness* back. A 2:1 box average is the right answer for edges and the
wrong answer for micro-contrast - fine flat fills, thin 1-texel lines, dialog
text. That is the gap the rest of this page is about.

## 3. The technique landscape

### 3a. Supersampling family - free, already shipped

| Technique | Idea | Verdict here |
| --- | --- | --- |
| SSAA 2x / 4x | magnify, then average back down with a box filter | **shipped**, `ScaleMode=SuperSample` |
| RGSS | four taps on a rotated grid instead of a 4x box | inert: no rasterised geometry |
| MSAA | hardware multisampled render target | inert: same reason |
| Minification filter choice | `LINEAR` (4 taps) vs `TRIANGLE`/box | minor; `LINEAR` at 2:1 already is a box |

Cost on Apple Silicon is irrelevant at this size: 1.3 MP of textured output
per frame, which is nothing for a unified-memory GPU that fills several GPix/s.
Twice the supersample factor costs one extra pass, not one extra pipeline.

### 3b. Edge-aware magnification of the low-resolution frame (the retro standard)

These run *instead of* the magnify pass, at the source resolution, and try to
reconstruct what a perfect magnifier would have produced.

| Family | Members | Behaviour | Cost |
| --- | --- | --- | --- |
| Neighbourhood pattern match | Scale2x/3x (EPX, AdvMAME), hq2x-hq4x | splits an enlarged pixel so a diagonal reads as an angle | ~4-6 taps |
| The xBR line | xBRZ, xbr_hybrid, Super-XBR | smoother diagonals, better on detailed sprites | ~13-25 taps |
| Sub-pixel reconstruction | ScaleFX (Hyllian), NED (Negative Edge Detection), CrispSal / SmoothSal | sub-pixel edge placement, sharper than xBR | ~9-16 taps |
| Reconstruction kernels | Catmull-Rom, Mitchell, Lanczos3, Jinc / EWA | smooth, soft, no edge logic | 9-64 taps |
| Neural-ish directional | DCCI, NNEDI3 | good diagonals, needs several passes | 3 passes |

Sources: the RetroArch `slang` shader tree (`pixel-art-scaling/pixel_aa*`,
`xbr`, `scalefx`, `nnedi3` presets); the libretro shader reference describes
`pixel_aa` as period/phase decomposition with a configurable slope
(`slopestep()`), i.e. it anti-aliases only where the pixel grid actually
crosses, and leaves flat texels bit-exact. That property is the whole appeal:
it is the only approach that is *both* crisp and anti-aliased.

The community caveat is real, though. Pattern-matching filters read dithering
as geometry, so ordered-dither gradients - which this engine uses heavily for
gradients and shade ramps - can get smeared into blotches. `CrispSal` /
`SmoothSal` are the safer members of the line for dither-heavy content, and
`PixelArt` (integer point sampling) stays the safe mode for purists.

### 3c. Post-AA on the magnified result (the 3D standard)

Because this frame is *rendered* geometry rather than drawn sprites, running a
real anti-aliasing pass over the magnified image is legitimate, not a crime
against pixel art.

- **FXAA** (and the newer FXAA III console variant): 1 pass, ~13 taps, no
  Motion vectors, cheapest real AA.
- **SMAA**: 2 passes (edge detection + SMAA weighting) plus a resolve, ~13
  taps in the heavy pass. Better edges than FXAA, noticeably softer overall.
- **reverse-aa / advanced-aa**: the RetroArch 2x-then-AA presets; these exist
  precisely to layer AA on an already magnified retro frame.

### 3d. Modern upscaling and sharpening (where the "modern" answer lives)

This is what current engines ship, and it is the right shape for us:

- **FSR 1 = EASU + RCAS.** EASU is a directionally adaptive Lanczos: it
  stretches its kernel along detected edge directions and clamps the result to
  the neighbourhood, so it does not ring. RCAS is a robust contrast-adaptive
  sharpening pass. MIT-licensed, single header, two compute passes, and AMD
  quotes 0.2-0.5 ms at 4K on mid-range desktop GPUs - on Apple Silicon at
  these sizes that is noise.
- **CAS standalone**: contrast-adaptive sharpening with no upscaling, 9 taps.
  This is the standard "undo the softness" step, and it is the single most
  portable piece of the modern toolchain.
- **MetalFX / `MTLFXSpatialScaler`** (macOS 13+, Metal 4 has `MTL4FXSpatialScaler`):
  Apple's own spatial upscaler, a black box that takes an input texture plus
  the content size and writes a larger texture. Apple's software licensing
  disclosures reference AMD FidelityFX, and the framework ships a spatial mode
  akin to FSR 1 and a temporal mode akin to FSR 2. The temporal mode is
  unusable here (no motion vectors out of a software rasteriser), and folding
  the scaler into bgfx's command stream is awkward: bgfx exposes no Metal
  command encoder, and the scaler needs its own input/output usage flags and
  pixel formats. Treat it as a later experiment, not a shortcut.

### 3e. What to skip

- **Neural upscalers** (DLSS, XeSS, FSR 4, Anime4K). They are trained to
  invent plausible detail that a downscale lost. Pixel art has not lost
  anything - every texel is final. Running one smears edges, eats single-pixel
  highlights and palette tricks, and costs a round trip through an inference
  runtime. They also want tensor cores, which means Metal 4 and a much bigger
  dependency than this port wants.
- **TAA**. Needs motion vectors and a history buffer; would make the dialog
  wipe, scrolling text and radar shimmer or ghost. There is no depth buffer to
  disocclude with.
- **Pure integer scaling as the default**. It is already a mode (`PixelArt`),
  and it is the honest answer when the scale factor is an integer. As the only
  mode, on a Retina panel at a non-integer window size, it is the jaggedness.

## 4. The Apple Silicon angle

| Factor | Consequence |
| --- | --- |
| Fill rate | Not the constraint. 1.3 MP at 60 Hz is ~0.08 GPix/s; even a 5K window is under 1 GTex/s. |
| Unified memory, no VRAM | A second output target at 2x costs a few MB; no allocation strategy needed. |
| `maxTextureSize` | 16384 on Apple Silicon, so even a 4K window fits the 2x prescale target without the existing clamp ever biting. |
| Shader cost centre | ALU plus texture taps. EASU/RCAS/SMAA all fit comfortably; that is why a compute-shader implementation is unnecessary and a fragment shader is enough. |
| Metal sampler features | `MTLSamplerDescriptor.minFilter/magFilter = linear`, clamp-to-edge, LOD bias all available; bgfx exposes the first three through `BGFX_SAMPLER_*` flags. |
| No MSAA benefit | Confirmed again above: the only rasterised geometry is two quads. |

So the decision is not about hardware headroom. It is about how much code we
are willing to own.

## 5. What adding a filter actually costs in this tree

This is the part that changes the ranking, and it is specific to this repo.

1. There is no fragment shader. `Backend_Init` builds one program from
   `vs_ocornut_imgui` / `fs_ocornut_imgui` (`bgfxbackend.cpp:375`).
2. bgfx shaders are **embedded binaries** produced offline, embedded through
   `BGFX_EMBEDDED_SHADER` (`bgfxbackend.cpp:36` to `:38`) and loaded with
   `bgfx::createEmbeddedShader` (`bgfxbackend.cpp:375`). A filter shader means a
   new `fs_*.bin.h` in the binary image.
3. On Metal that `.bin.h` is **MSL source text, not a compiled library**. The
   existing `fs_ocornut_imgui.bin.h` holds the literal string
   `#include <metal_...>` / `fragment ...`, and `renderer_mtl.cpp:3409` hands the
   stored bytes to `newLibraryWithSource(...)` - the shader is compiled on the
   first program creation and then cached by Metal. Two consequences: there is
   no `xcrun metallib` step and no Xcode dependency, and the one-off compile
   happens at init, not per frame.
4. The toolchain to make the first `.bin.h` exists but is switched off:
   `thirdparty/CMakeLists.txt:13` forces `BGFX_BUILD_TOOLS OFF`, so `shaderc`
   has no ninja target today. Turning it on once is enough. `shaderc` builds
   from `thirdparty/bgfx.cmake/bgfx/tools/shaderc`; its Metal path
   (`shaderc_metal.cpp` pulls in `ShaderLang.h` and `spirv_msl.hpp`) is
   glslang -> SPIR-V -> SPIRV-Cross MSL, so it needs no Apple toolchain at all,
   only a C++ compiler. glslang, SPIRV-Cross and SPIRV-Tools are **already
   vendored** under `thirdparty/bgfx.cmake/bgfx/3rdparty` (6.4 M / 3.1 M /
   7.4 M), so nothing is fetched from the network.
5. `BGFX_EMBEDDED_SHADER` is picked by renderer type at compile time, so the
   new entry must carry the Metal bin for the renderer in use, or
   `bgfx::createEmbeddedShader` returns invalid and `Backend_Init` fails.
4. With that one file in hand, the per-filter cost is small: a new
   `_EmbeddedShaders` entry, a second `bgfx::ProgramHandle` for
   `VIEW_PRESENT`, and a `Submit_Quad` that uses it. The prescale view keeps
   the existing program.

Cheaper still, and worth saying plainly: **the whole current SuperSample path is
sampler state and needs no toolchain work.** Any filter that can be expressed
as a magnify factor has to be a shader, but things like going from 2x to 4x
stays inside `Backend_Present`.

## 6. Recommended ladder

Ranked by value per unit of code owned.

| # | Change | Type | What it buys | Effort |
| --- | --- | --- | --- | --- |
| 1 | **Sharpen the final 2:1 minify** with a contrast-adaptive (CAS-style) or unsharp kernel in a new `fs` | 1 shader, ~9 taps | Undoes the softness that SSAA introduced. This is the difference between "supersampled" and "modern". | **shipped** as `fs_sharpen` + `Sharpness` knob |
| 2 | **Ratio-aware AA** (`pixel_aa`-style period/phase, or SMAA) on the present pass | 1 shader, or fold into #1 | Kills residual stair-steps on diagonals without softening flat fills | +1 h |
| 3 | Expose the ladder in `SUN.INI`, e.g. `ScaleMode=SuperSample` plus a `Sharpness` 0-1 and an `AAMode` of `None / FXAA / SMAA / PixelAA` | small | Lets the choice be made per user instead of per build | +1 h |
| 4 | Optional 4x supersample factor as a `Quality` knob | none (sampler) | Free headroom; pick it on a large window | 30 min |
| 5 | Optional CRT flavour: mild scanline, aperture mask or bloom | 1 shader | The game was authored for a CRT; a little glow hides residual aliasing | +1 h |
| 6 | MetalFX spatial scaler as an experiment | invasive | Apple's black box, no code to write, but needs a Metal path outside bgfx | later |

Ordering rationale: sharpening is the cheapest thing that fixes the actual
complaint. Supersampling without sharpening makes an old game *soft*, not
*modern* - the two go together, and the industry has shipped them together as
SSAA + CAS for a decade. Ratio-aware AA is the second complaint (staircase at
non-integer window sizes), and it is the only technique that is both crisper
and smoother than bilinear at the same time.

## 7. How it gets verified

The standing rule applies: it gets tested headlessly, not by asking.

`OPENTS_AADUMP` already writes the finished frame from inside the renderer to
`/tmp/aa_readback.bin` as `[width height]` plus BGRA rows
(`bgfxbackend.cpp:726`). Any new pass sits in the same chain, so:

- measure edge-pixel statistics (intermediate colours along diagonals) before
  and after, the way the SuperSample validation was measured;
- check that flat fills stay bit-identical, which is the property that proves a
  filter is not smearing dithering;
- confirm frame count and no dropped presents during a dialog wipe, since a
  second program means two `bgfx::ProgramHandle`s and one more state change.

The sharpening pass was verified exactly this way (2026-10-02, 1280x960
campaign dialog, `OPENTS_SELFTEST=1 OPENTS_ANIM=1 OPENTS_AADUMP=1`, dump
sampled over time and compared against a `Sharpness=0.0` run):

- flat regions (3x3 range zero): **bit-identical** — dither untouched;
- gradient energy: 3.426→3.612 (x), 3.873→4.076 (y), a 1.05x crispness gain;
- max per-pixel change: 18 of 255 — no blowouts; run-to-run output
  **bit-identical** (the scene is fully deterministic);
- the output at `Sharpness=0` is the plain minify by construction (the clamp
  endpoints include the centre), so the knob is a true off switch.

A zoomed before/after crop of the dialog lives in
[image-quality-sharpen-compare.png](image-quality-sharpen-compare.png).

## Sources

- FSR 1 algorithm and cost figures: AMD GPUOpen, "FidelityFX Super Resolution
  1", https://gpuopen.com/fidelityfx-superresolution/ ; EASU / RCAS internals
  and the header's own input requirements, GPUOpen `ffx_fsr1.h`.
- MetalFX spatial scaler API, Apple Developer Documentation
  (`MTLFXSpatialScaler`, `MTLFXSpatialScalerBase`); MetalFX described as
  spatial + temporal modes, PCGamingWiki glossary of high-fidelity upscaling.
- FSR 1 quality modes and the 1.3x / 1.5x / 1.7x / 2.0x scale factors:
  GPUOpen FidelityFX-FSR reference documentation.
- Retro shader taxonomy and preset list, RetroArch `slang` shader tree
  (`pixel-art-scaling`, `xbr`, `scalefx`, `nnedi3`, `anti-aliasing`);
  `pixel_aa` algorithm description, libretro `glsl-shaders` reference.
- Pixel-art upscaling tradeoffs and the case against AI upscalers on pixel
  art: ImageMint "How to Upscale Pixel Art Without Blurring It".
- Metal sampler filtering: Apple Developer Documentation, "Adding mipmap
  filtering to samplers"; shader-subsystem bottleneck counters, "Reducing
  shader bottlenecks".
- bgfx shader toolchain (`shaderc`, Metal target via SPIRV-Cross MSL),
  bgfx `tools/shaderc/shaderc_metal.cpp` and this tree's `thirdparty/CMakeLists.txt`.

Internal references checked for this page (all in this repo):

- program creation and the single `texture2D` tap: `code/bgfxbackend.cpp:375`
  to `:384`.
- prescale / present views and the 2x supersample sampling:
  `code/bgfxbackend.cpp:634`, `:664`.
- `OPENTS_AADUMP` readback: `code/bgfxbackend.cpp:726`.
- tools forced off: `thirdparty/CMakeLists.txt:13`.
- MSL compiled at runtime, not linked as a metallib:
  `thirdparty/bgfx.cmake/bgfx/src/renderer_mtl.cpp:3409`.
