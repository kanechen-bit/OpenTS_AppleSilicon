# MTLFXSpatialScaler — implementation scope

Question: how much work to add Apple's MetalFX Spatial Scaler to the present
path, and is it the right tool for this pipeline?

Measured on this machine (Apple M4 Max, macOS 27.0.1, Xcode macOS 27 SDK),
not estimated from documentation. Probe source: `/tmp/mtlfx_probe.mm`.

## 1. Verified facts

| Fact | Result |
| --- | --- |
| `MetalFX.framework` present in `/System/Library/Frameworks` | yes |
| SDK headers (macOS 27 SDK) | `MTLFXSpatialScaler.h` (195 lines), `MTLFXTemporalScaler.h`, `MTLFXFrameInterpolator.h`, `MTL4FXSpatialScaler.h` |
| `+[MTLFXSpatialScalerDescriptor supportsDevice:]` | **YES** on the M4 Max |
| Minimum usages reported | `colorTextureUsage = 0x1`, `outputTextureUsage = 0x5` |
| Output texture storage mode | header: "You are responsible for providing a texture with a private `storageMode`" |
| `gain` / `scaleTransform` on the scaler | **not in the macOS 27 protocol** — do not code against them |
| Runtime availability floor | macOS 13.0+ (`API_AVAILABLE(macos(13.0), ios(16.0))`) |
| Run-to-run determinism | bit-identical across two runs, so the existing A/B harness still works |

Compiled and ran a standalone probe: descriptor → `newSpatialScalerWithDevice:`
→ per-frame `colorTexture` / `inputContentWidth` / `outputTexture` →
`encodeToCommandBuffer:` → blit → CPU readback. Build line that works:

```
clang++ -std=c++17 -fno-objc-arc -F$SDK/System/Library/Frameworks \
    -o probe probe.mm -framework MetalFX -framework Metal -framework Foundation
```

(`-framework Foundation` is required or the link fails on `_objc_msgSend`.)

## 2. What it does to pixel art at exact 2x

Probe input: 64x48 synthetic pixel art (flat fill, hard horizontal edge,
diagonal staircase, blocky glyph lines) upscaled to 128x96, compared against
the ideal nearest-neighbour replication.

| metric (64x48 → 128x96) | MetalFX | bilinear | nearest (ideal) |
| --- | --- | --- | --- |
| 2x2 blocks perfectly uniform (pixel grid intact) | 70.1 % | 70.4 % | **100 %** |
| flat-fill interior deviation | 0 | 0 | 0 |
| deviation at region boundaries | ±24/255 over a 3-row band | ±32/255 | 0 |
| hard edge, rows 27/28 (luminance) | 49 / 205 | 64 / 191 | 0 / 255 |
| RMS error vs ideal | 20.93 | 21.35 | 0 |
| mean gradient energy | 9.25 | 8.01 | 8.17 |

Reading: MetalFX behaves like a slightly over-sharp bilinear. Flat interiors
survive bit-perfect, but hard edges become a 2-row ramp landing ~0.5 px early,
and ~15 % of pixels leave the ideal pixel grid. It is not pixel-grid
preserving at integer ratios, which is exactly the property `ScaleMode=PixelArt`
already gives for free.

## 3. Why the direction matters

`MTLFXSpatialScaler` only **upsizes**. It has no minifying mode, so it cannot
replace the current SuperSample "oversample then minify" pair, and it provides
no supersampling. Today's path is:

```
game frame 640x480 → prescale 2560x1920 (linear) → present quad, 4-tap box minify into the window
```

Adding MetalFX means inserting an upscale between the game frame and the
present quad — i.e. feeding it the 640x480 frame and asking for the window
size. That is a real pipeline edit, not a drop-in filter.

Second point: MetalFX's pitch is "render cheaper at low resolution, upscale".
The expensive part here is the engine's own software renderer and its texture
upload, not the present pass, so there is **no performance argument** for
adopting it in this tree. This is purely a quality question.

## 4. Cost to implement

Working in this tree, on top of the existing `SuperSample` + `Sharpness`
plumbing.

| # | Task | Est. |
| --- | --- | --- |
| 1 | ObjC++ build wiring. Confirmed: the `OpenTS` target compiles with `-std=gnu++20` and **no** `-ObjC++` (only bgfx's own target has it, from `thirdparty/bgfx.cmake/CMakeLists.txt`). MetalFX is Objective-C, so either enable `OBJCXX` + a `.mm` shim, or add `-ObjC++`; plus link `Metal`, `MetalFX`, `Foundation` and add the SDK framework search path. | 0.5–1 h |
| 2 | MetalFX lifecycle: descriptor, `supportsDevice:` check, scaler creation, output texture at drawable size, recreate on resize, destroy at shutdown. Mostly a wrapper around the probe code above. | 2–3 h |
| 3 | Command-buffer access. bgfx's `videoEnsureCommandBuffer(RendererContextMtl*)` is file-static inside `thirdparty/bgfx.cmake/bgfx/src/{video_mtl,renderer_mtl}.cpp`. Export it (small patch to the vendored copy, declared in a tree header) so the encoder runs in the same buffer as the upload and the present — avoids fence/semaphore races. | 1.5–3 h |
| 4 | Input texture interop: get an `id<MTLTexture>` for the game-frame texture out of bgfx. The backend currently only ever passes `bgfx::TextureHandle` around, so this needs a raw-texture accessor or a mirrored CPU-side upload. | 2–4 h |
| 5 | Output binding into the present path: register the scaler's output as a bgfx-samplable texture / framebuffer, have the present quad sample it, keep letterbox rect, vertical flip and the `VIEW_DUMP` readback consistent. If bgfx texture usage bits don't satisfy MetalFX's `outputTextureUsage`, fall back to a hand-rolled raw Metal present pass (PSO + vertex buffer + render encoder) using the existing `fs_sharpen` MSL conventions. | 3–5 h (+ 8–12 h for the raw-Metal fallback) |
| 6 | Options: a `ScaleMode=MetalFX` value (or a `MetalFX=` toggle), fallback to SuperSample when `supportsDevice:` is false, log line. | 1 h |
| 7 | Headless verification with the existing `OPENTS_AADUMP` + determinism harness; A/B against SuperSample and PixelArt on settled dialog frames. | 2–3 h |
| 8 | Docs, skill note, memory. | 0.5–1 h |

**Total: 13–23 h (roughly 2–3 working days)**, with the mass in tasks 3–5.
A decision spike that answers the quality question is **~3–4 h** and is
already ~80 % done — generalise the probe, run it at 2x and at a non-integer
ratio (e.g. 640x480 → 1440x900), and compare against the real shader output.

## 5. Recommendation

- At the user's actual ratio (window exactly 2x the frame), MetalFX measures
  **worse** than `ScaleMode=PixelArt` (100 % grid vs 70 %) and no better than
  the current SuperSample + `Sharpness` path. Don't build it for that case.
- The one case it plausibly wins is a **non-integer** window/frame ratio where
  `PixelArt` cannot use integer scaling and linear upscale looks soft. Even
  there, spike first.
- There is no performance case: MetalFX exists to save GPU render time, and the
  present pass is not the bottleneck.

Cheaper options already available or small work:

| Option | Cost | Notes |
| --- | --- | --- |
| Retune `Sharpness` (already 1.0) | 0 | knob takes 0.0–1.0, no rebuild |
| Use `ScaleMode=PixelArt` at integer window sizes | 0 | 100 % pixel-grid preserving |
| Sharper kernel (CAS-style) at non-integer scales | 3–6 h | confined to the existing shader blob pipeline, no framework or command-buffer work |
| MetalFX, gated to non-integer windows only | same 13–23 h | narrower benefit, same cost |
