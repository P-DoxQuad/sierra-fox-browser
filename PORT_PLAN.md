# Firefox-Legacy: Port of Firefox 161.0a1 to macOS 10.13 High Sierra

## Release build requirements (mandatory)

Every release build of Sierra Fox Beta — any `.zip`/`.dmg` that will be published to the GitHub Releases page — must include every established performance optimization. Specifically:

1. **ThinLTO** (`ac_add_options --enable-lto=thin`).
2. **PGO**, consumed from a `merged.profdata` collected against an instrumented build of the same release. The full three-phase workflow (instrumented build → `./mach python build/pgo/profileserver.py --binary=<instrumented .app>/Contents/MacOS/firefox` → `llvm-profdata merge -o merged.profdata *.profraw`) is prerequisite to every release; the final release build consumes the merged profile via `--enable-profile-use` + `--with-pgo-profile-path`.
3. **CPU-tier targeting** (`-march=westmere` / `-march=nehalem` / default `penryn`) applied to `CFLAGS`, `CXXFLAGS`, and `RUSTFLAGS`.
4. **Updater disabled** (`ac_add_options --disable-updater`) and all branding patches applied.
5. **All distribution-hygiene flags present**: `-Wl,-rpath,@executable_path`, `-ffile-prefix-map`, `--remap-path-prefix`. See CLAUDE.md § Distribution hygiene for the full catalogue.

Dropping any of these from a release build for convenience or time pressure is not permitted. If `merged.profdata` is missing (local-state loss, `./mach clobber` ran, disk failure), the correct response is to re-run the PGO workflow — never to publish a release without it. Expected release-build time end-to-end is ~3.5-4 hours (phase 1 build ~90 min, profileserver run ~15-30 min, phase 3 build ~90 min, packaging ~10 min). Plan around that duration; do not try to shortcut it.

## Scope

This document describes a port of Firefox 161.0a1 Nightly to macOS 10.13 (High Sierra), the earliest release of macOS to ship Metal 2.

The source baseline is `161.0a1/`, pulled from `mozilla-central`. Upstream's current minimum deployment target is macOS 10.15 (Catalina) on Intel and 11.0 (Big Sur) on Apple Silicon.

Ports to earlier macOS versions (El Capitan 10.11, Yosemite 10.10) are separate projects, each forking from this one once it is stable. They are referenced where relevant to avoid baking in assumptions that would obstruct them, but are not part of this port's deliverable.

## Target

- **OS:** macOS 10.13 High Sierra (final build 10.13.6, released 2018)
- **Architecture:** `x86_64-apple-darwin`
- **Graphics backend:** Metal (native on 10.13 via `MTLGPUFamilyMac2`)
- **Build host:** the target machine itself. This is a native build on macOS 10.13.6, not a cross-compile.
- **SDK:** `MacOSX10.14.sdk` (bundled with Xcode 10.1, the last Xcode that runs as a host application on macOS 10.13). The 10.14 SDK is used with `MACOSX_DEPLOYMENT_TARGET=10.13` to produce binaries compatible with 10.13. Apple supports targeting an older OS with a newer SDK via the `-mmacosx-version-min` flag.

## Build host prerequisites

1. macOS 10.13.6 with Xcode 10.1 installed at `/Applications/Xcode-10.1.app`. Command Line Tools 10.1 is also installed; `xcode-select` must point at the full Xcode (not CLT) for the Firefox build.
2. Miniforge or Miniconda for Python environment management, with Python 3.8 or later available (3.12 confirmed working).
3. Mozilla's `./mach bootstrap` installs the modern clang toolchain and Rust toolchain. Apple clang 10.0.0 (bundled with Xcode 10.1) is too old to build Firefox 161 directly. The bootstrap downloads a prebuilt clang and sets up `rustup` with the `x86_64-apple-darwin` target.
4. Sufficient disk space for the Firefox object directory (20–40 GB). The source tree lives on `/Volumes/Data RAID`, which has room.

## Patch inventory

The port requires modifications in six areas. File paths are relative to `161.0a1/`.

### 1. Build configuration

| File | Change |
|---|---|
| `build/moz.configure/toolchain.configure:208` | Change Intel deployment target from `"10.15"` to `"10.13"`. |
| `build/moz.configure/toolchain.configure` (SDK min check, around L190–220) | Lower SDK minimum from 26.5 (Sonoma) to allow 10.13 SDK. |
| `python/mozbuild/mozbuild/mozconfig.py` | Audit for any hardcoded Catalina assumptions. |
| `build/macosx/` common files | Audit for `MACOSX_DEPLOYMENT_TARGET` references. |

### 2. mozconfig

A dedicated mozconfig for this port lives at `mozconfig.highsierra` (to be created at the repo root alongside the standard one). Required entries:

```
ac_add_options --target=x86_64-apple-darwin
ac_add_options --with-macos-sdk=/Applications/Xcode-10.1.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.14.sdk
export MACOSX_DEPLOYMENT_TARGET=10.13
```

Note: the 10.14 SDK is used with a 10.13 deployment target. This is Apple's standard mechanism for building against an older OS using a newer SDK and is enforced by the `-mmacosx-version-min=10.13` flag that mach emits from `MACOSX_DEPLOYMENT_TARGET`.

### 3. `@available(macOS ...)` guards

An audit of `widget/cocoa/`, `dom/media/platforms/apple/`, `toolkit/`, and `security/` found 38 `@available` guards across 12 files. All target macOS 11.0 or later; none target 10.13 or earlier. For the 10.13 build, every one of these guards must take its `else` branch. Three files hold the bulk:

| File | Guard count | Minimum version | Nature |
|---|---|---|---|
| `widget/cocoa/MOZDynamicCursor.mm` | 16 | 11.0 | Dynamic cursor features |
| `widget/cocoa/nsCocoaWindow.mm` | 6 | 11.0 | Window chrome / toolbar APIs |
| `widget/cocoa/nsMacShellService.mm` | 4 | 12.0 | Default-browser / sharing APIs |

The remaining ~12 guards are distributed across `NativeMenuMac.mm`, `MOZMenuOpeningCoordinator`, WebRTC screen capture, speech synthesis, geolocation, accessibility, and `ASWebAuthSessionHandler` for versions 11.0 through 14.0.

Approach: for each guard, either implement a pre-11.0 fallback using older AppKit APIs, or stub to a no-op where the feature is cosmetic.

### 4. Dark mode and `NSAppearance`

22 references across 9 files assume `NSAppearance` dark-mode APIs introduced in macOS 10.14 (Mojave). The 10.13 build must either:
- Force light theme and short-circuit `NSAppearance` queries, or
- Stub the dark-mode queries to always report light appearance.

Affected files include `widget/cocoa/nsCocoaWindow.mm`, `widget/cocoa/NativeMenuMac.mm`, and seven others. A full list to be inventoried during the patch pass.

### 5. Rust crate chain

Vendored under `third_party/rust/`. Required changes:

| Crate | Change |
|---|---|
| `core-foundation` | Enable `mac_os_10_7_support` feature in `Cargo.toml`. This crate already supports pre-10.14 fallbacks; the feature just needs enabling. |
| `objc2-io-surface` | Verify `rust-version = 1.71` doesn't pin macOS >= 10.14 (currently appears not to; confirm during build). |
| `objc2-metal` | Metal 2 features used in Firefox (`MTLSharedEvent` from Metal 2.1) require 10.13+. Already compatible with target. |
| Other `objc2-*` crates | Audit for hard macOS version pins at `Cargo.toml` level. Fork and vendor patched versions as needed. |

### 6. Vendored C++ dependencies

Verification required, not expected to need patches for 10.13:
- **Skia** — Firefox's config does not require Metal-only Skia; works on 10.13.
- **libwebrtc** — audit for Mac deployment target overrides.
- **cubeb** — CoreAudio APIs used are 10.6+, compatible.
- **ICU, NSS** — portable, no changes expected.

## Verification plan

The port is verified in four milestones.

### M1: Toolchain proof

Build unmodified Firefox 161.0a1 on the modern host, targeting macOS 10.15 (upstream default). Produces a working `.app`. Proves the build host and toolchain are correct before any patches are introduced.

### M2: Deployment target drop

Apply only the build configuration patches (section 1) and mozconfig (section 2). Attempt to build against the 10.13 SDK. Expect compiler errors from `@available` guards and `NSAppearance` references. Capture the full error inventory as the authoritative patch list.

### M3: Compiles clean

Apply all patches from sections 3–6. Build produces a `.app` with no compiler or linker errors.

### M4: Runs on target

Launch the produced `.app` on macOS 10.13 (VM or hardware). Smoke tests:
- Load `https://www.google.com`
- Play HTML5 video (H.264)
- Open DevTools
- Run a WebGL demo
- Open and close multiple tabs without crash

Dynamic linking errors (`dyld: Symbol not found`) at launch indicate missing `@available` fallbacks that compiled but reference newer AppKit symbols. These must be fixed by changing the fallback to an API that exists in 10.13.

## Current status

For the current milestone status, benchmark results, in-progress work, and patch inventory, **see `CLAUDE.md`** at the project root. That file is maintained as the authoritative current-state snapshot.

For the chronological history of how we got here — individual session work, specific crash diagnoses, patch justifications — **see `DEVLOG.md`**.

Summary as of 2026-11-02: all four milestones (M1 toolchain proof, M2 deployment target drop, M3 compiles clean, M4 runs on target) are complete. Firefox 161.0a1 launches and runs cleanly on the Mac Pro 5.1 target hardware, handles real workloads (YouTube, Teams meetings with webcam, Google Maps, extensions), and benchmarks favorably against the last officially-supported Firefox 115 ESR on the same hardware (comparable MotionMark, JetStream 3 completes cleanly where 115 ESR crashes outright). Compiler optimization pass since M4 completion added `-march=westmere` and ThinLTO, raising JetStream 3 from 48.38 to 50.42. PGO work currently in progress.

## Known risks

- **Hidden deployment target assumptions** beyond the ones catalogued above may surface at M3 or M4. The build process is iterative: compile, read the error, patch, repeat. In practice this has produced ~35 source patches as of 2026-10-30.
- **Rust crate churn** — vendored `objc2-*` crates are a fast-moving ecosystem. Future upstream rebases may bring in versions with stricter macOS pins; the patches here may need to be re-applied forward.
- **Firefox Nightly moves fast.** This port targets 161.0a1 specifically. Rebasing to a later Nightly is a separate maintenance exercise.
- **libc++ 19 link resolution.** Resolved 2026-10-31 by injecting `-L~/.mozbuild/clang/lib` + `-Wl,-rpath,~/.mozbuild/clang/lib` into both LDFLAGS and HOST_LDFLAGS. Runtime resolution required additional symlinks for `libc++abi.1.dylib` and `libunwind.1.dylib` next to the libc++.1.dylib symlink in `~/.mozbuild/clang/lib`.
- **CPU baseline (Westmere, no AVX/AVX2).** The target Mac Pro 5.1 runs on Intel Westmere Xeons: SSE4.2 but no AVX/AVX2/BMI2/FMA/F16C. Mozilla's clang 22 defaults to `-target-cpu penryn` (SSE4.1), so the baseline compile is safe — confirmed empirically: `otool -tV` on `XUL`, `libmozglue.dylib`, `rlbox.wasm_*.o` shows no AVX/AVX2 instructions. The SIGILL seen on 2026-10-31 attributed to AVX2 was actually a `libplatform` abort from `OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION`; `otool`'s AVX2 hit was in CFI/eh_frame unwind trampolines, not executable code. **One real remaining risk: `third_party/llama.cpp/moz.build` unconditionally sets `-mavx -mavx2 -mbmi2 -mf16c -mfma -msse4.2` for every llama.cpp translation unit.** `libmozinference.dylib` isn't a load-time dep of libxul (it is `dlopen`'d by `MLSuggest`, PDF alt-text, Translation Models, Parakeet speech-to-text, local chat), so until the user turns on an AI feature, this never fires. When it does fire, the fix is either strip the AVX flags in that moz.build or gate the AI features off via prefs on Westmere. Documented but deferred.
- **Metal 1 ceiling on 10.13.** 10.13 ships Metal 1 (feature set `macOS_GPUFamily1_v3`). All shimmed Metal APIs (`MTLSharedEvent`, `MTLSharedEventListener`, `MTLGPUFamily`, `MTLDeviceLocation`, `MTLCompileOptions.preserveInvariance`, `MTLBinaryArchive`) are runtime-gated and fall back cleanly. The actual GPU in the Mac Pro 5.1 may support fewer Metal features than Firefox assumes (argument buffers tier 2, non-uniform threadgroup sizes, resource heaps). If the compositor fails, `gfx.webrender.software = true` in `about:config` is the escape hatch. HEVC/VP9/AV1 hardware decode will not be available (requires Skylake or newer iGPU on 10.13); H.264 HW decode works and runs smoothly on YouTube per 2026-10-31 testing. The fullscreen crash currently under investigation may itself be a Metal / NSWindow fullscreen interaction with the older GPU.

## Planned follow-up work

Items identified but not yet scheduled. Listed in the order they are expected to be tackled.

### 1. Stub out the auto-updater

Firefox's built-in updater polls Mozilla's AUS (Application Update Service) for new builds and offers to download them. On this port, applying a stock Mozilla update would silently replace the patched 10.13 binary with one that no longer runs on 10.13 (SDK guards, Metal shims, mozjemalloc getenv patches would all be gone). The updater must be prevented from contacting Mozilla before the user ever sees an update prompt.

Scope:

- Set `app.update.enabled`, `app.update.auto`, `app.update.background.enabled`, and `app.update.service.enabled` to `false` in the default preferences layer (`browser/app/profile/firefox.js` or equivalent) so the behavior is baked into the build rather than per-profile.
- Point `app.update.url` at an empty / local value so even if a check is forced manually it cannot reach Mozilla's AUS.
- Hide or remove the "Check for Updates…" entry in the Help menu and the update section of `about:preferences`. Removing is preferred over disabling since the button has no useful destination on this port.
- Leave the updater binary (`toolkit/mozapps/update/updater/updater.cpp` and its moz.build) in place rather than excising it from the build. Keeping it compiled but unreachable minimises patch surface against upstream rebases.

Future enhancement, deferred: once the port is published on GitHub, swap the Help-menu entry to a link that opens the Releases page in the default browser. Until the repository exists, no link is better than a broken link.

### 2. Enable the built-in AI features for Westmere

Firefox 161 ships several AI features that `dlopen` `libmozinference.dylib` on first use: `MLSuggest` (URL bar), PDF alt-text generation, inline translation models, Parakeet speech-to-text, and the local chat sidebar. `libmozinference.dylib` is built from `third_party/llama.cpp`, whose `moz.build` unconditionally applies `-mavx -mavx2 -mbmi2 -mf16c -mfma -msse4.2` to every translation unit. On Westmere these instructions are not available (SSE 4.2 is the ceiling), so loading the dylib on the target hardware would `SIGILL` on first use.

Currently mitigated by leaving the AI features disabled in `about:config` so `libmozinference.dylib` is never `dlopen`'d. The follow-up work is to make them actually usable rather than just safely-off:

- Replace the hardcoded instruction-set flags in `third_party/llama.cpp/moz.build` with a Westmere-safe baseline (`-mssse3 -msse4.1 -msse4.2`), ideally by introducing a configure-time switch so the Westmere path does not permanently disable AVX for anyone building on newer silicon.
- Rebuild `libmozinference.dylib` with the Westmere baseline and verify it loads via a focused `dlopen` test harness before exposing any user-facing feature.
- Benchmark one representative AI workload (likely PDF alt-text, since it is deterministic and self-contained) and record whether the performance is viable on the target hardware or whether CPU inference without AVX2 is too slow to be useful. If viable, flip the relevant prefs on by default; if not, document the limitation and leave the features off.
- Translation Models and Parakeet rely on ONNX Runtime or similar heavy dependencies that may have their own AVX assumptions; address those separately after `libmozinference.dylib` itself is working.

Deferred until the updater work is complete so the AI investigation does not interleave with potentially-destabilising front-end changes to the Help menu and preferences UI.

### 3. Decide whether to publish tiered CPU-target builds as releases

Three mozconfigs now exist in the tree covering Westmere, Nehalem, and Penryn CPU floors (`mozconfig.highsierra`, `mozconfig.highsierra.nehalem`, `mozconfig.highsierra.penryn`). Each produces its own `.app` in its own objdir and is built independently; see the 2026-11-02 "Compatibility-tier mozconfigs added" entry in DEVLOG for the invocation details and ISA coverage of each.

The open question is release policy: ship one build (Westmere, as today), two (Westmere + Penryn covering the extremes), or all three. Arguments for publishing more than one:

- The port's identity is "runs on old Mac hardware"; capping support at Mac Pro 5,1 and 2011+ laptops leaves a meaningful population of 10.13-capable Macs on the table.
- Each additional build is a tagged `.app` and nothing more — the user is the dispatcher; no runtime detection code or shared build infrastructure is required.

Arguments against:

- Each additional variant adds 60-90 minutes of build time per release and a corresponding checklist item.
- The compatibility variants ship without PGO (see mozconfig comments and DEVLOG for the rationale), so their performance is noticeably below the Westmere build's; users on compatible hardware who grab the Penryn build "by mistake" would be running a slower binary than necessary.

Decision deferred until a GitHub repository exists and a first public release is scheduled. At that point, publishing at minimum the Westmere build and labelling it unambiguously ("Westmere+; see Nehalem build for Mac Pro 4,1 and earlier Core i-series iMacs") is the floor; the other two variants can be added if demand materialises.

## Related projects

The following are **separate forks** from this port, not stages of it:

- **El Capitan 10.11 port.** Would additionally replace `MTLSharedEvent` (Metal 2.1, 10.13+) at `third_party/angle/src/libANGLE/renderer/metal/DisplayMtl.mm:478,1068` with `MTLFence` or `dispatch_semaphore`-based sync; verify ANGLE's `MTLGPUFamilyMac1` fallback at `mtl_utils.mm:1429-1432`.
- **Yosemite 10.10 port.** Metal does not exist on 10.10. Requires resurrecting the CGL/OpenGL compositor from Firefox git history (removed around Firefox 110) and wiring it into the current compositor abstraction as a backend variant.

Each has its own scope, timeline, and risk profile and is documented separately when work begins.

## GPU compatibility (informational)

Metal support is GPU-dependent independent of OS version. The following GPUs lack Metal support entirely and cannot run this port regardless of OS:

- Intel HD 3000 and older (Sandy Bridge, 2011 and earlier)
- NVIDIA GeForce 320M, 9400M, Fermi (400/500 series)
- AMD/ATI Radeon HD 4000/5000/6000 series

Machines with these GPUs are the target of the Yosemite CGL port, not this one.
