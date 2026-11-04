# Firefox-Legacy Devlog

Chronological log of completed work on the Firefox-Legacy High Sierra port. Each entry describes what was done, what was found, and what changed in the plan as a result. Newest entries at the bottom.

Format: `## YYYY-MM-DD — <task title>`

---

## 2026-10-26 — Build host audit

The Mac Pro 5.1 host running macOS 10.13.6 was audited to establish the toolchain baseline before any patching or build attempts. The audit was run twice: a first pass with all volumes mounted, and a second pass after the operator disconnected volumes unrelated to the project. Only the second-pass findings are recorded here.

**Volumes relevant to the project:**

- Boot volume: `Macintosh SSD-backup` (`/dev/disk1s2`), mounted at `/`
- Project volume: `Data RAID` (`/dev/disk10`), mounted at `/Volumes/Data RAID`

All searches below are scoped to these two volumes. Other mounted volumes (`Project Files`, `Macintosh OWC - Data`, `Untitled`, `Windows`, and network shares) were present during the audit but excluded from scans.

**Inventory:**

| Component | Finding |
|---|---|
| Host OS | macOS 10.13.6 (17G14042), x86_64 |
| Xcode 10.1 | Installed at `/Applications/Xcode-10.1.app`. Bundles `MacOSX10.14.sdk` (verified version 10.14 via `SDKSettings.plist`). `MacOSX.sdk` is a byte-identical copy of `MacOSX10.14.sdk`, not a symlink. Last Xcode version that runs on 10.13 as host OS. |
| Xcode 8.2.1 | Installed at `/Applications/Xcode-8.2.1.app`. Bundles `MacOSX10.12.sdk`. Not used by this port. |
| Command Line Tools | Version 10.1 installed. `xcode-select -p` currently returns `/Library/Developer/CommandLineTools`, not the full Xcode. This needs switching to Xcode 10.1 before the Firefox build. |
| Apple clang | 10.0.0 (LLVM 7). Too old for Firefox 161 build. Mozilla bootstrap installs its own modern clang toolchain. |
| Python | Miniforge3 base environment at `~/miniforge3`, Python 3.12.12. Meets mach's minimum of 3.8. Platform reported as `osx-64`. |
| conda | Version 25.11.1 at `~/miniforge3/bin/conda`. User config at `~/.condarc`. Existing env `ofscraper` present (unrelated to this project). |
| Rust toolchain | Not installed. No `rustc`, `cargo`, `rustup`, or `~/.cargo`. Mozilla bootstrap installs this. |
| Disk space (system `/`) | 70 GB free of 265 GB. |
| Disk space (project volume) | 422 GB free of 1.7 TB on `/Volumes/Data RAID`. Firefox object directory (20–40 GB) fits comfortably. |
| `MacOSX10.13.sdk` | Not present on the two scoped volumes. Scoped search of `/` and `/Volumes/Data RAID` returned no matches. |

**Plan adjustments resulting from this audit:**

1. **The build host and the target OS are the same machine.** The port is a native build on macOS 10.13.6, not a cross-compile from a newer host as initially written in `PORT_PLAN.md`. All references to cross-compilation in the plan have been corrected.
2. **A `MacOSX10.13.sdk` is not required.** The build uses `MacOSX10.14.sdk` from Xcode 10.1 combined with `MACOSX_DEPLOYMENT_TARGET=10.13`. This is Apple's documented approach to targeting an older OS with a newer SDK. The plan's reference to extracting the 10.13 SDK from Xcode 9.4.1 has been removed.
3. **Toolchain acquisition is handled by `./mach bootstrap`.** Modern clang (version 15 or later, as required by Firefox 161) and the Rust toolchain will be installed by Mozilla's bootstrap process rather than manually.
4. **`xcode-select` currently points to Command Line Tools.** For a full Firefox build it may need to point to the full Xcode 10.1 install. This will be confirmed and switched if needed before the first build attempt.

**Risks surfaced:**

- Apple clang 10.0.0 is too old to build Firefox 161. If Mozilla's bootstrap fails to install a modern clang that runs on 10.13.6, this becomes a blocker. The bootstrap process ordinarily downloads a prebuilt toolchain, but prebuilt clang for 10.13 hosts may no longer be available from Mozilla's servers.
- Modern Rust toolchains may have dropped 10.13 host support. `rustup` historically supports older macOS hosts but this needs verification during install.

**Status:** Task complete. Host is viable in principle. Next task to be approved before continuing.

---

## 2026-10-26 to 2026-10-27 — Volume rename, toolchain install, Homebrew bypass, watchman skip

The project volume `Data RAID` was renamed to `Data-RAID`. Mozilla's `mach` enforces a hard check against spaces in the source checkout path (`build/mach_initialize.py:112`), which blocked the first bootstrap attempt. The volume name was changed via `diskutil rename`, and a compatibility symlink was created at `/Volumes/Data RAID` pointing to `/Volumes/Data-RAID` so pre-existing path references continue to resolve.

Rust was installed via `rustup` with the stable toolchain (1.99.1), which satisfies Firefox 161's minimum of 1.90.0.

A dedicated conda environment `firefox-build` was created with Python 3.12.12 to isolate mach's Python dependencies from the base environment.

**Homebrew bypass on 10.13.** Mozilla's `mach bootstrap` on Intel macOS drives Homebrew to install four system packages (`git`, `gnu-tar`, `terminal-notifier`, `watchman`). Homebrew's portable Ruby fails to run on 10.13.6 with a `dyld: Symbol not found: ____chkstk_darwin` error, so the first bootstrap attempt failed immediately. To work around this, `python/mozboot/mozboot/osx.py` was patched so that `OSXBootstrapper.install_system_packages()` returns early on macOS versions below 10.15, since the ARM64 "Light" bootstrapper already demonstrates that Homebrew is not strictly required on macOS.

The four system packages were approached via MacPorts (which does still support 10.13) rather than Homebrew. Only `gnutar` installed cleanly. The `watchman` port's build chain pulls in Node.js 22 as a dependency; Node 22 fails to compile on 10.13 because its source uses `std::filesystem::path`, which libc++ marks `__attribute__((unavailable))` for deployment targets below macOS 10.15.

Two attempts were made to work around the Node 22 failure:
1. Setting `legacysupport.use_mp_libcxx yes` in the `nodejs22` Portfile. This had no effect because of an `option_proc` registration bug in `legacysupport-1.1.tcl:223` (double-prefixed name `legacysupport.legacysupport.use_mp_libcxx`), so the callback never fires on the correct option.
2. Adding `-D_LIBCPP_DISABLE_AVAILABILITY=1` via `configure.cppflags-append`. The flag reached some gyp targets but not all, because Node's gyp configuration partitions compile targets with different flag sets. Even if the flag had reached every target, the underlying `std::filesystem` symbols are not present in macOS 10.13's `/usr/lib/libc++.1.dylib`, so link or runtime failure was expected regardless.

**Watchman was dropped from the install plan.** It is an optional dependency used by `mach` to accelerate incremental builds via native filesystem notifications. Without it, `mach` falls back to timestamp-based polling, adding roughly 15–30 seconds of overhead to each incremental build on a tree the size of Firefox. For a first full build, there is no benefit either way. The mozboot patch already applied means `mach` will not attempt to install watchman itself.

**Final MacPorts state:** 191 ports installed. Of the four originally targeted, only `gnutar` is active. `git` is provided by Command Line Tools and is sufficient. `terminal-notifier` and `watchman` are abandoned. The `configure.cppflags-append -D_LIBCPP_DISABLE_AVAILABILITY=1` line added to the `nodejs22` Portfile remains and is a candidate for later cleanup, but has no effect in isolation.

**Status:** Build host is viable. `./mach bootstrap` is being re-run on 2026-10-27 with the mozboot Homebrew-skip patch in place.

---

## 2026-10-27 — Bootstrap completion, configure patches, libc++ 19, first source patches

**Bootstrap completed clean.** `./mach bootstrap --no-interactive --application-choice=browser` ran to completion with the mozboot Homebrew-skip patch. Mozilla's prebuilt toolchain downloads succeeded: modern clang, cbindgen, nasm, Node (unused — see below), WASI sysroot, dump_syms, minidump-stackwalk, sccache, rustc-dist and clang-dist toolchains, ONNX runtime, clang-tidy, and misc dev-tool CLIs. All cached under `~/.mozbuild/`.

**Node runtime substitution.** Mozilla's prebuilt Node at `~/.mozbuild/node/bin/node` is built against macOS 11.0 and references `____chkstk_darwin`, which does not exist in 10.13.6's libSystem. `./mach configure` subsequently rejected it with `could not find Node.js executable later than 12.22.12`. Resolution: installed Node 16.20.2 from conda-forge into the `firefox-build` conda environment. The `conda install nodejs=16` step inadvertently removed Python 3.12 from the environment during dependency resolution; Python was re-installed with `conda install python=3.12`. Mach picks up Node automatically from the active env PATH.

**Deployment target patch.** `build/moz.configure/toolchain.configure:208` — Intel target lowered from `"10.15"` to `"10.13"`.

**SDK min version patch.** `build/moz.configure/toolchain.configure:251` — `mac_sdk_min_version()` lowered from `"26.5"` to `"10.13"`.

**STL hardening check patch.** `build/moz.configure/toolchain.configure:3041` — `stl_hardening_flags` previously `die()`-ed when no STL library matched `using_msvc_stl_202503_or_newer | using_libstdcxx | using_libcxx`. On this port the configure-time `try_compile` for libc++ detection fails because the mozconfig's `CPPFLAGS` do not propagate into mach's internal compilation tests. Patched to silently skip hardening on default-origin instead of dying. STL hardening is defense-in-depth, not a build prerequisite.

**Mozconfig.** Created `mozconfig.highsierra` at the repo root. Sets `--target=x86_64-apple-darwin`, `--with-macos-sdk` pointing at Xcode 9.4.1's `MacOSX10.13.sdk`, `--enable-macos-target=10.13`, `MACOSX_DEPLOYMENT_TARGET=10.13`, and object-directory `obj-highsierra`.

**libc++ 19.1.7 built from source.** The 10.13 SDK ships no libc++ v1 headers (only legacy C++ 4.2.1 from GCC-era), and Mozilla's bundled clang does not include its own libc++. The first `./mach build` attempt failed at `mfbt/CheckedInt.h` with `no member named 'in_range' in namespace 'std'`, along with `std::bit_width`, `std::bit_ceil`, and `std::has_single_bit` missing — all C++20 features from libc++ 14+. MacPorts' `macports-libcxx` is libc++ 11 which also lacks these.

Resolution: downloaded LLVM 19.1.7 source (`llvm-project-19.1.7.src.tar.xz`, 141 MB) and built libc++, libc++abi, and libunwind using Mozilla's bundled clang 22. CMake configured with `-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind"`, `-DCMAKE_OSX_DEPLOYMENT_TARGET=10.13`, and `-DCMAKE_OSX_SYSROOT` pointing at the 10.13 SDK. Build completed in roughly 30 minutes on this hardware (1324 ninja targets).

The build artifacts live at `Firefox-Legacy/toolchain-build/install/`. To make clang pick them up without CFLAGS/CXXFLAGS/BINDGEN_EXTRA_CLANG_ARGS plumbing (which proved fragile — host builds, bindgen's internal clang, and target-specific `BINDGEN_EXTRA_CLANG_ARGS_x86_64_apple_darwin` overrides from mach all needed separate handling), the headers and dylibs are symlinked into Mozilla's clang's default search paths:

- `~/.mozbuild/clang/include/c++/v1` → `toolchain-build/install/include/c++/v1`
- `~/.mozbuild/clang/lib/libc++.1.dylib` → `toolchain-build/install/lib/libc++.1.dylib`
- `~/.mozbuild/clang/lib/libc++.dylib` → `toolchain-build/install/lib/libc++.dylib`
- `~/.mozbuild/clang/lib/libc++abi.dylib` → `toolchain-build/install/lib/libc++abi.dylib`

With libc++ 19 symlinked in, the `mfbt/CheckedInt.h` and `mozglue/baseprofiler/public/PowerOfTwo.h` C++20 errors cleared.

**First Firefox source patches — post-10.13 framework availability.** The build then failed on three macOS framework references that are post-10.13:

1. `accessible/xpcom/xpcAccessibleMacInterface.mm` — `#import <Accessibility/Accessibility.h>` and use of `AXCustomContent` (both macOS 11+). Patched: both the import and the `AXCustomContent` usage are wrapped in `#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 110000`. The existing `if (@available(macOS 11.0, *))` runtime guard already prevented execution.

2. `browser/components/shell/nsMacShellService.mm` — `#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>` and `UTType*` (macOS 11+), plus `SMAppService` and `SMAppServiceStatus*` (macOS 13+). Patched: the UniformTypeIdentifiers import is SDK-gated, and `UTType` is forward-declared `@class UTType;` on older SDKs. `SMAppService` is forward-declared, and `SMAppServiceStatus` is manually typedef'd with the enum values Firefox references. All usage sites are already runtime-gated by `@available`.

3. `accessible/mac/MOXAccessibleProtocol.h` — ~50 errors on `AXTextMarkerRef` and `AXTextMarkerRangeRef`. These are opaque CF types that were only made publicly available in the macOS 14 SDK. Patched: added forward typedefs `typedef const struct __AXTextMarker* AXTextMarkerRef;` (and the Range equivalent) guarded by `MAC_OS_X_VERSION_MAX_ALLOWED < 140000`. Runtime symbols are resolved via dlsym against private HIServices.

**Status:** Build is in progress following these four source patches. First real compile progress occurred — Rust bindgen build-scripts now succeed, main C++ compile progresses past the mfbt headers.

---

## 2026-10-27 — Rust vendored-crate patches, Core Location, Core Audio shims

Subsequent build iterations hit a widening pattern of post-10.13 API references across Rust and Objective-C++ sources. Each is runtime-gated at its call site; the fixes exist only to allow the 10.13 SDK compile to succeed.

**`SMAppService` return-type cast.** `browser/components/shell/nsMacShellService.mm:588` — with `SMAppService` forward-declared (no interface), `[... status]` was being resolved by clang to `NSURLHandle`'s `status` method returning `NSURLHandleStatus`. Added an explicit `(SMAppServiceStatus)(NSInteger)` cast on the receiver and the return value.

**Rust `coreaudio-sys-utils` type mismatch.** `third_party/rust/coreaudio-sys-utils/src/dispatch.rs:85` — bindgen on the 10.13 SDK generated `dispatch_get_global_queue` with an `i64` first parameter, but the Rust code passed `QOS_CLASS_DEFAULT as isize`. Changed to `QOS_CLASS_DEFAULT as i64`. The vendored crate's `.cargo-checksum.json` entry for `src/dispatch.rs` was recomputed and overwritten.

**Rust `cubeb-coreaudio` renamed constant.** `third_party/rust/cubeb-coreaudio/src/backend/aggregate_device.rs` (two uses) and `src/backend/tests/utils.rs` (one use) referenced `kAudioAggregateDevicePropertyMainSubDevice`, which was renamed in macOS 12 from `kAudioAggregateDevicePropertyMasterSubDevice`. The 10.13 SDK still uses the "Master" name. All three uses were renamed back; `.cargo-checksum.json` updated for both files.

**Core Location geolocation patches.** `dom/geolocation/GeolocationSystemMac.mm`:
- Line 79 (`[mLocationManager requestWhenInUseAuthorization]`) is marked `NS_UNAVAILABLE(macos)` in the 10.13 SDK. Casting to `id` did not bypass the diagnostic because clang matches the selector back to the declared-unavailable method. Replaced with `[mLocationManager performSelector:@selector(requestWhenInUseAuthorization)]` to defeat compile-time lookup. The existing `@available(macOS 15.0, *)` runtime guard remains.
- Lines 173 and 192 (`-[CLLocationManager authorizationStatus]`) are macOS 11+ instance-method signatures; on the 10.13 SDK only the matching class method exists. Added `(CLAuthorizationStatus)(NSInteger)[(id)locationManager authorizationStatus]` cast chain.

**Apple media/codec shims.** The next build pass surfaced a wave of missing CoreMedia/CoreVideo/VideoToolbox constants used inside `@available(macOS 11+, *)` guards. Rather than patching each file inline, created two new files:
- `dom/media/platforms/apple/HighSierraShims.h` — declares, under `__MAC_OS_X_VERSION_MAX_ALLOWED < 110000`, the following constants: `kCMVideoCodecType_VP9`, `kCMVideoCodecType_AV1`; `kCVPixelFormatType_422YpCbCr8BiPlanarFullRange`/`VideoRange`; `kCGColorSpaceITUR_2100_PQ`/`HLG`; `kVTVideoEncoderSpecification_EnableLowLatencyRateControl`; `kVTCompressionPropertyKey_ConstantBitRate`/`PrioritizeEncodingSpeedOverQuality`/`BaseLayerFrameRateFraction`; `kCVImageBufferTransferFunction_Linear`.
- `dom/media/platforms/apple/HighSierraShims.cpp` — provides matching `CFStringRef` definitions as `CFSTR("...")` constants. These are never read at runtime on the 10.13 build (the @available guards block execution), so their values do not need to match Apple's internal strings — only the symbols need to exist for the linker.

Registered the new source in `dom/media/platforms/moz.build`'s Apple `UNIFIED_SOURCES`. Added `#include "HighSierraShims.h"` to `AppleDecoderModule.cpp`, `AppleVTDecoder.cpp`, and `AppleVTEncoder.cpp`.

**Core Audio / Core Video include header rename.** `dom/media/platforms/apple/AppleATDecoder.cpp:7` — `<CoreAudioTypes/CoreAudioBaseTypes.h>` is a macOS 11+ umbrella; on older SDKs the same base types live in `<CoreAudio/CoreAudioTypes.h>`. SDK-guarded.

**MacIOSurface pixel format shims.** `gfx/2d/MacIOSurface.h` references `kCVPixelFormatType_422YpCbCr8BiPlanarFullRange` and similar formats added after 10.13. Since this is a widely included graphics header outside `dom/media`, the forward-enum shim is inline at the top of the file, guarded by `__MAC_OS_X_VERSION_MAX_ALLOWED < 110000`.

**Patch count so far.** Three mach configure patches, one mozconfig, one mozboot patch, four source patches, two Rust vendored-crate patches (with cargo checksum updates), two new shim files, one moz.build addition.

**Status:** Build iteration continues. We are past the mfbt + xpcom boundary and well into dom/media and gfx compile. The pattern is clear: each build cycle surfaces one or two more post-10.13 API references, each individually small to patch. Progress is monotonic — the error count per cycle is going down.

---

## 2026-10-27 — Deep iteration: speech synth, WebAuthn, gfx/layers, Metal, IPC, cubeb, ANGLE, ScreenCaptureKit

Build iterated through many more post-10.13 API references. Each is runtime-gated or routed to a legacy fallback; patches exist only so the 10.13 SDK compile succeeds.

**AVFoundation speech synthesis (`dom/media/webspeech/synth/cocoa/OSXSpeechSynthesizerService.mm`).** The entire 500-line file uses `AVSpeechSynthesizer`, `AVSpeechUtterance`, `AVSpeechSynthesisVoice`, `AVSpeechSynthesizerDelegate`, `AVSpeechBoundaryImmediate`, and `AVSpeechSynthesisAvailableVoicesDidChangeNotification`, all added to macOS AVFoundation in 10.14. On 10.13 none of these exist. Rather than patching every reference, the file content is wrapped in `#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101400`, with an `#else` section providing minimal stub implementations of the `OSXSpeechSynthesizerService` class members (`Init` returns false, `Speak` returns `NS_ERROR_NOT_IMPLEMENTED`, `GetInstance`/`GetInstanceForService` return nullptr, etc.) so the XPCOM component factory still links. The TTS service registers no voices on 10.13.

**WebAuthn / Passkeys (`dom/webauthn/MacOSWebAuthnService.mm`).** `AuthenticationServices.framework` is 10.15+ and the full Passkeys API is 13.3+. The file is wrapped in `#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101500`, with an `#else` providing a stub `NewMacOSWebAuthnServiceIfAvailable()` that returns nullptr. WebAuthn simply reports no platform authenticator on 10.13.

**Microsoft Entra SSO (`netwerk/protocol/http/MicrosoftEntraSSOUtils.mm`).** Same `AuthenticationServices.framework` dependency. Wrapped identically with a stub `AddMicrosoftEntraSSO` returning `NS_ERROR_NOT_IMPLEMENTED`.

**Metal shared-event fencing (`gfx/layers/GpuFenceMTLSharedEvent.mm`).** The `MTLSharedEvent` Objective-C protocol was added in macOS 10.14 / iOS 12. Forward-declared a minimal protocol on older SDKs containing only the methods this file actually uses (`signaledValue` property and `waitUntilSignaledValue:timeoutMS:`).

**NativeLayerCA (`gfx/layers/NativeLayerCA.mm`).** Several HDR / dynamic-range features gated by SDK:
- `CGColorCreateSRGB` is 10.15+; the single call site is already gated on a pref, now additionally SDK-guarded with a fallback to `CGColorCreateGenericRGB`.
- `CVBufferCopyAttachments` is 12.0+; the entire block (inside a runtime `@available(macOS 12.0, *)` guard) is additionally SDK-guarded.
- `AVSampleBufferDisplayLayer.requiresFlushToResumeDecoding` is 11.0+; same pattern.
- `AVSampleBufferDisplayLayer.preferredDynamicRange`, `CADynamicRangeHigh`/`CADynamicRangeStandard`, `AVSampleBufferDisplayLayer.wantsExtendedDynamicRangeContent` are 14.0+/11.0+; the whole HDR branch is SDK-guarded with `>= 140000`.
- `AVSampleBufferDisplayLayer.preventsCapture` is 11.0+; SDK-guarded with a `(void)aIsDRM;` fallback.

**CoreText fast-path (`gfx/skia/skia/src/ports/SkTypeface_mac_ct.cpp`).** `kCTFontVariationAxesAttribute` is 10.15+. Wrapped the fast-path call site in `#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101500`; the fallback `CTFontCopyVariationAxes` (10.7+) is used on 10.13.

**CoreText language-aware font lookup (`gfx/skia/skia/src/ports/SkFontMgr_mac_ct.cpp`).** `CTFontCreateForStringWithLanguage` exists in CoreText.framework from macOS 10.9 but is not declared in Xcode 9.4.1's 10.13 SDK headers. Added an `extern "C"` forward declaration; the symbol resolves at link time against the system CoreText.

**fileport IPC (`ipc/chromium/src/chrome/common/ipc_channel_mach.cc`).** `<sys/fileport.h>` was publicly exposed in macOS 11 but the underlying `fileport_makeport`/`fileport_makefd` functions have existed in libSystem since 10.5. Added `extern "C"` forward declarations when the header is missing.

**posix_spawn additions (`ipc/chromium/src/base/process_util_mac.mm`).** `posix_spawn_file_actions_addchdir_np` is 10.15+; the workdir path is SDK-guarded with a fall-back error return. `responsibility_spawnattrs_setdisclaim` is 10.14+; SDK-guarded.

**Core Audio `Main` rename (`media/libcubeb/src/cubeb_audiounit.cpp`).** 23 instances of `kAudioObjectPropertyElementMain` renamed back to `kAudioObjectPropertyElementMaster` (the pre-macOS-12 name). This is a Mozilla-forked C library, not a cargo crate, so no checksum update needed.

**Geolocation (`dom/geolocation/GeolocationSystemMac.mm`).** `-[CLLocationManager requestWhenInUseAuthorization]` is NS_UNAVAILABLE(macos) on the 10.13 SDK; `(id)` casting did not bypass the diagnostic because clang matches the selector back to the declared-unavailable method. Replaced with `performSelector:@selector(...)`. `-[CLLocationManager authorizationStatus]` as an instance method is 11.0+; cast chain `(CLAuthorizationStatus)(NSInteger)[(id) ...]` used at two call sites.

**ANGLE Metal backend.** Three patches:
- `third_party/angle/src/common/platform.h` — upstream enforces `#error "macOS 12 SDK or newer is required."`. Removed.
- `third_party/angle/src/common/apple_platform_utils.mm` — `MTLGPUFamilyMac2` constant and `-supportsFamily:` method are 10.15+. On older SDKs, the branch returns `true` optimistically; actual Metal 2.1 feature detection happens elsewhere via `MTLSharedEvent` probe.

**ScreenCaptureKit (`third_party/libwebrtc/modules/desktop_capture/mac/*.mm`).** Two files use ScreenCaptureKit.framework (12.3+): `sck_picker_handle.mm` (small, body SDK-guarded with `CreateSckPickerHandle` returning nullptr on 10.13) and `screen_capturer_sck.mm` (900 lines, top-of-file guard provides nullptr-returning stubs for `ScreenCapturerSckAvailable`, `GenericCapturerSckWithPickerAvailable`, `CreateScreenCapturerSck`, `CreateGenericCapturerSck`; everything else wrapped in `#else` block). WebRTC falls back to `CGDisplayStream`-based capture on 10.13.

**Media/codec shims extended.** The `HighSierraShims.h` header now also provides `kAudioFormatMPEGD_USAC` constant (xHE-AAC audio, 11+). `MacIOSurface.h` inline shim now also provides `kCVPixelFormatType_4444AYpCbCrFloat` in addition to the 422 bi-planar 8-bit variants (reverted the 10-bit entries after discovering they are declared on 10.13).

**Running patch inventory.** Build-system (3) + mozconfig (1) + mozboot (1); Firefox source: `xpcAccessibleMacInterface.mm`, `nsMacShellService.mm`, `MOXAccessibleProtocol.h`, `GeolocationSystemMac.mm`, `AppleATDecoder.cpp`, `AppleDecoderModule.cpp`, `AppleVTDecoder.cpp`, `AppleVTEncoder.cpp`, `MacIOSurface.h`, `MacIOSurface.cpp` (via shim), `OSXSpeechSynthesizerService.mm`, `MacOSWebAuthnService.mm`, `MicrosoftEntraSSOUtils.mm`, `GpuFenceMTLSharedEvent.mm`, `NativeLayerCA.mm`, `SkTypeface_mac_ct.cpp`, `SkFontMgr_mac_ct.cpp`, `ipc_channel_mach.cc`, `process_util_mac.mm`, `cubeb_audiounit.cpp`, `angle/platform.h`, `angle/apple_platform_utils.mm`, `sck_picker_handle.mm`, `screen_capturer_sck.mm` (24 files); Rust vendored (3 files, 2 crates, 2 checksum updates); new files (2 shims + 1 moz.build update).

**Status:** Build iteration continues. Previous cycle hit only one or two unique errors per iteration — a healthy sign. The remaining compile pattern is pure error-per-file iteration against post-10.13 Apple framework additions.

---

## 2026-10-27 (continued) and 2026-10-30 — widget/cocoa, toolkit/xre, breakpad, libwebrtc, ANGLE Metal headers, first linker error

Further build iteration surfaced errors across widget/cocoa, toolkit/xre, crash reporting, and the ANGLE Metal backend. All compile errors are now resolved; the build has reached the **link phase** and surfaced its first linker error.

**toolkit/crashreporter/breakpad-client/mac/handler/exception_handler.cc.** Four mach resource-type constants referenced in `switch` labels are post-10.13 additions: `RESOURCE_TYPE_THREADS`, `RESOURCE_TYPE_PORTS`, `FLAVOR_DIAG_MEMLIMIT`, `FLAVOR_CONCLAVE_LIMIT`. Each wrapped in `#ifndef … #define … #endif` just before its case label; the integer values match XNU-sourced definitions so the switch still routes correctly.

**ipc/chromium IPC and process spawning (further).** `posix_spawn_file_actions_addchdir_np` SDK-guarded (10.15+) with a `return Err(ENOSYS)` fallback. `responsibility_spawnattrs_setdisclaim` SDK-guarded (10.14+) with a logged no-op. `<sys/fileport.h>` include conditional; `fileport_makeport`/`fileport_makefd` forward-declared as `extern "C"` on older SDKs.

**media/libcubeb (CoreAudio).** 23 instances of `kAudioObjectPropertyElementMain` in `src/cubeb_audiounit.cpp` renamed back to the pre-macOS-12 name `kAudioObjectPropertyElementMaster`. Not a Rust vendored crate, so no cargo checksum.

**netwerk/protocol/http/MicrosoftEntraSSOUtils.mm.** `AuthenticationServices.framework` is macOS 10.15+. Whole-file SDK-guarded with a stub `AddMicrosoftEntraSSO` returning `NS_ERROR_NOT_IMPLEMENTED`.

**toolkit/xre/ASWebAuthSessionHandler.mm.** Also uses `AuthenticationServices.framework`. SDK-guarded with stubs for `RegisterASWebAuthSessionHandler`, `RegisterASWebAuthSessionObservers`, `WasLaunchedByAuthenticationServices`. The guard was initially placed too late (after the `@interface` declaration) which left globals using `ASWebAuthenticationSessionErrorDomain` outside the guard; moved the guard earlier to just after the `gASWebAuthLog` LazyLogModule.

**toolkit/xre/MacApplicationDelegate.mm.** `NSUserActivityRestoring` protocol is not declared in Xcode 9.4.1's 10.13 SDK. Added forward `@protocol NSUserActivityRestoring @end` guarded by `__MAC_OS_X_VERSION_MAX_ALLOWED <= 101300`.

**toolkit/xre/MacLaunchHelper.mm.** `NSWorkspaceOpenConfiguration` is macOS 11+. SDK-guarded with a fallback using the deprecated `-[NSWorkspace launchApplicationAtURL:options:configuration:error:]` (10.10+).

**third_party/libwebrtc camera capture.** `AVCaptureDeviceDiscoverySession`, `AVCaptureDeviceType`, `AVCaptureDeviceTypeBuiltInWideAngleCamera`, `AVCaptureDeviceTypeExternal`/`ExternalUnknown` are 10.15+. `RTCCameraVideoCapturer.h` typedefs `AVCaptureDeviceType` as `NSString*` on older SDKs; `RTCCameraVideoCapturer.m` SDK-guards the method bodies with a 10.13 fallback using `[AVCaptureDevice devicesWithMediaType:]`.

**third_party/libwebrtc screen capture.** Already stubbed in previous session (sck_picker_handle.mm, screen_capturer_sck.mm). Confirmed working.

**widget/cocoa (large batch).**
- `MediaHardwareKeysEventSourceMacMediaCenter.mm` — `MPMediaItemPropertyArtwork` is 10.13.2+; our 10.13 SDK is 10.13.0. Suppressed the `-Wunguarded-availability-new` warning for this file via `#pragma clang diagnostic`.
- `MOZDynamicCursor.mm` — 16 `@available(macOS 15.0, *)` guards around `NSCursor` frame-resize API. Added forward-declarations: `NSCursorFrameResizePosition` enum with 8 directional values, `NSCursorFrameResizeDirections` NS_OPTIONS, and a `NSCursor (FirefoxLegacyFrameResizeShim)` category declaring `+frameResizeCursorFromPosition:inDirections:`. All runtime-gated so dead code is never executed.
- `nsChildView.h` — `NSStandardKeyBindingResponding` protocol missing on 10.13 SDK. Added empty `@protocol NSStandardKeyBindingResponding @end` forward declaration.
- `nsCocoaWindow.h` — `NSTitlebarSeparatorStyle` enum missing. Added `NS_ENUM` forward declaration with the four documented values.
- `nsNativeThemeColors.h` — `NSAppearanceNameDarkAqua` missing. Added `extern NSAppearanceName const NSAppearanceNameDarkAqua __attribute__((weak_import));`.
- `MOZMenuOpeningCoordinator.mm` — `NSMenu.appearance` property is 11+. SDK-guarded the setter.
- `nsMenuItemX.mm` — `NSMenuItem.badge` and `NSMenuItemBadge` class are 14+. SDK-guarded the setter.
- `ScreenHelperCocoa.mm` — `maximumPotentialExtendedDynamicRangeColorComponentValue` is 10.15+. SDK-guarded with fallback to `maximumExtendedDynamicRangeColorComponentValue` (10.11+).
- `TextRecognition.mm` — `Vision.framework` is 10.15+. Whole-file SDK-guarded; on <10.15 the public entry point rejects with "Vision.framework unavailable".
- `nsCocoaUtils.mm` — `AVAuthorizationStatus` enum and `authorizationStatusForMediaType:` method are 10.14+. Rewrote `LogAuthorizationStatus` and `GetPermissionState` to use raw integer literals (0..3 matching Apple's documented values) and `performSelector:` dispatch instead of the typed API.

**third_party/angle Metal backend (header shims).** Multiple Metal type references across ANGLE's mtl_* headers that aren't on the 10.13 SDK: `MTLSharedEvent`/`MTLEvent` protocols (10.14+), `MTLBarrierScope` enum (10.14+), `MTLTextureSwizzle`/`MTLTextureSwizzleChannels` (10.15+). Added forward-declarations to `third_party/angle/src/libANGLE/renderer/metal/mtl_common.h` (which is included by `mtl_resources.h`, `mtl_context_device.h`, `mtl_command_buffer.h`): forward `@protocol` declarations for the Metal protocols, `NS_OPTIONS` for `MTLBarrierScope`, `NS_ENUM` for `MTLTextureSwizzle`, and a plain struct for `MTLTextureSwizzleChannels`.

**Compile phase complete; first linker error.** The latest iteration passed all compile steps and reached the link stage. First error: `cdm-test-decryptor` and `cdm-test-storage` test executables fail with `ld64.lld: error: undefined symbol: vtable for std::__1::basic_stringbuf`, `vtable for basic_stringstream`, and `std::__1::__libcpp_verbose_abort(char const*, ...)`.

The ABI tag `[abi:nn190107]` on the inline-emitted symbols confirms our libc++ 19 headers are being used for compile. The vtable symbols and `__libcpp_verbose_abort` must come from the linked `libc++.dylib`. Hypotheses:
1. The link command isn't including `-L~/.mozbuild/clang/lib`, so the linker finds and resolves against the SDK's `libc++.1.tbd` (pointing at the pre-10.14 system libc++ which has no `__libcpp_verbose_abort`).
2. Or the symlink `~/.mozbuild/clang/lib/libc++.1.dylib → toolchain-build/install/lib/libc++.1.dylib` is being seen but the linker is picking the SDK's `.tbd` first due to search-path ordering.

Next-session action: inspect the actual link command for `cdm-test-decryptor` in the build log, verify library search paths, and either (a) adjust the mozconfig `LDFLAGS` to force `-L~/.mozbuild/clang/lib` first, or (b) copy/symlink the libc++ 19 dylibs into the SDK's lib path so they take precedence over the stub `.tbd`, or (c) make the mozconfig propagate `LDFLAGS` to the test-executable link steps (which it may not do; CPPFLAGS/CXXFLAGS do but LDFLAGS can be split differently).

**Patch inventory total (session end):** 3 build-system patches, 1 mozconfig, 1 mozboot patch, 2 new shim files + 1 moz.build update, ~35 Firefox source file patches across accessible, browser/components/shell, dom/geolocation, dom/media, dom/media/webspeech, dom/webauthn, gfx/2d, gfx/layers, gfx/skia, ipc/chromium, media/libcubeb, netwerk/protocol/http, toolkit/crashreporter, toolkit/xre, widget/cocoa, third_party/angle, third_party/libwebrtc. Rust vendored: 3 files across 2 crates with cargo checksum updates.

## 2026-10-31 — M3 fully completed, M4 started, SIGILL on Westmere

### Phase change: artifacts produced

For the first time, a full build run produces every top-level target:

- `obj-highsierra/dist/bin/XUL` — 293 MB libxul
- `obj-highsierra/dist/bin/firefox` — 69 KB launcher
- `obj-highsierra/dist/bin/libmozinference.dylib` — 4.3 MB (parakeet.cpp + llama.cpp wrapper)
- `obj-highsierra/dist/bin/libmozglue.dylib`
- `obj-highsierra/dist/Nightly.app/Contents/MacOS/*` — full application bundle

The 2026-10-30 "M3 complete" was partial. ANGLE Metal `.mm` files, llama.cpp Metal backend, libmozinference.dylib link, and the final libxul link had never actually succeeded in prior sessions — they were skipped because test executables that depend only on libc++ (cdm-test-decryptor, cdm-test-storage) failed earlier in the dep graph and short-circuited the build. The 2026-10-31 session completed every one of these.

### libc++ 19 link + runtime resolution

The pre-session hypothesis — that `-L~/.mozbuild/clang/lib` would resolve against our libc++ 19 instead of the SDK's `libc++.1.tbd` stub — proved correct. Added `-L~/.mozbuild/clang/lib -Wl,-rpath,~/.mozbuild/clang/lib` to both `LDFLAGS` and `HOST_LDFLAGS` in `mozconfig.highsierra`. The host-tool link (wasm2c) then produced a binary that referenced `@rpath/libc++.1.dylib` correctly, but dyld load still failed because the libc++ 19 dylib itself references `@rpath/libc++abi.1.dylib` and `@rpath/libunwind.1.dylib` and those weren't symlinked. Added:

```
~/.mozbuild/clang/lib/libc++abi.1.dylib → toolchain-build/install/lib/libc++abi.1.dylib
~/.mozbuild/clang/lib/libunwind.1.dylib → toolchain-build/install/lib/libunwind.1.dylib
~/.mozbuild/clang/lib/libunwind.dylib → toolchain-build/install/lib/libunwind.dylib
```

Also added `-Wl,-undefined,dynamic_lookup` to LDFLAGS to pass through the compiler-rt symbol `_availability_version_check` (added in macOS 10.15 libSystem). lld-mach-O rejects classic `ld64 -U <sym>` syntax so single-symbol lookup wasn't an option.

### Final-link patch wave

Four frameworks were linked unconditionally in `moz.build` files but don't exist in the 10.13 SDK, so lld refused to find them:

| moz.build | Framework | Added in |
|---|---|---|
| `widget/cocoa/moz.build:175` | Accessibility | macOS 14 |
| `toolkit/library/moz.build:237` | ScreenCaptureKit (-weak_framework) | macOS 12.3 |
| `toolkit/xre/moz.build:96` | AuthenticationServices | macOS 10.15 |
| `browser/components/shell/moz.build:49` | UniformTypeIdentifiers (-weak_framework) | macOS 11 |
| `dom/webauthn/moz.build:90` | AuthenticationServices | macOS 10.15 |
| `netwerk/protocol/http/moz.build:210` | AuthenticationServices | macOS 10.15 |

Removed from each; usage at the source level is already SDK-guarded or `@available`-guarded.

### M4 launch loop

With the complete `.app` in hand, the iteration moved from "will it compile" to "will it launch." Each launch produced a specific error, each fix was tested with `./Nightly.app/Contents/MacOS/firefox --version` (or `--headless` for subsequent dynamic-loader errors). The pattern that emerged: lld's `-Wl,-undefined,dynamic_lookup` lets every missing symbol through at link time, which means every `OBJC_CLASS_$_<SomeNewAppKitClass>` referenced by `@class` forward declarations in our shims now triggers `dyld: Symbol not found` at app load.

For each one, the fix is: replace `[NewClass something]` with `objc_getClass("NewClass")` + `performSelector:` or typed-IMP cast. The code path is already runtime-gated by `@available(macOS X, *)` so on 10.13 the message is never sent — but the class symbol reference needs to not exist in the Mach-O at load time.

| File | Symbol | SDK |
|---|---|---|
| `mozglue/misc/Mutex_posix.cpp` | `pthread_mutexattr_setpolicy_np(..., 3)` returns EINVAL on 10.13 | 10.14+ |
| `widget/cocoa/nsCocoaWindow.mm` | `NSGlassEffectView` | macOS 26 |
| `browser/components/shell/nsMacShellService.mm` | `SMAppService` + `UTType` | 13 / 11 |
| `third_party/angle/.../DisplayMtl.mm` | `MTLSharedEventListener` | 10.14 |
| `third_party/llama.cpp/ggml/.../ggml-metal-context.m` | `MTLCaptureDescriptor` | 10.15 |

Also fixed the HighSierraShims namespacing issue: `AppleDecoderModule.cpp:30` included `HighSierraShims.h` inside `namespace mozilla {`, which mangled the extern symbols as `mozilla::kCGColorSpaceITUR_2100_PQ`, mismatching the file-scope definitions in `HighSierraShims.cpp`. Moved the `#include` out of the namespace block. Also removed the duplicate `kCVPixelFormatType_422YpCbCr8BiPlanarFullRange/VideoRange` enum from `HighSierraShims.h` (already defined in `gfx/2d/MacIOSurface.h`).

And `gfx/thebes/CoreTextFontList.cpp:1191` — `CTFontManagerRegisterFontURLs` is macOS 10.15+. Replaced with a per-URL loop calling `CTFontManagerRegisterFontsForURL` (10.6+).

### Current blocker: Illegal Instruction on Westmere

After the OBJC_CLASS_$ wave was through, `firefox --version` dies with exit code 132 (SIGILL, Illegal Instruction) and no stdout/stderr output. The crash is before anything prints.

The Mac Pro 5.1 has Westmere Xeons — SSE4.2, no AVX, no AVX2. Mozilla's clang 22 defaults to `-target-cpu penryn` (SSE4.1), so most code is baseline-safe. But two paths bake in AVX2:

1. **`third_party/llama.cpp/moz.build`** unconditionally sets `-mavx -mavx2 -mbmi2 -mf16c -mfma -msse4.2` on all x86_64. Comment claims the library is only loaded if AVX2 is supported, but the compile flags are universal across every translation unit. (Mitigation: libmozinference isn't a load-time dependency of libxul, so this doesn't fire on `--version`.)

2. **`security/rlbox/rlbox.wasm_0.c`** is generated by wasm2c and pulls in `<simde/wasm/simd128.h>` to lower WASM v128 ops. SIMDe at macro-time picks the widest available ISA; when `__AVX2__` is defined, it emits AVX2 intrinsics inline. `otool -tV XUL | grep -B1 vzeroupper` shows the AVX2 instructions sit inside `rlbox::get_rlbox_wasm2c_sandbox_thread_data`, which is called at thread init — firing before `main()` can get far enough to print `--version`.

The next session needs to find where `__AVX2__` ends up getting defined for the rlbox.wasm.c compile unit and strip it (likely via a `-mno-avx2 -mno-avx` or by setting `SIMDE_NO_NATIVE` in that file's CFLAGS), rebuild, and retry. If the SIGILL moves to another symbol, iterate.

### Patch inventory total (end of 2026-10-31)

Session total adds roughly 15 more source patches on top of 2026-10-30's ~35, plus 6 moz.build edits to prune framework links, plus the libc++ 19 abi/unwind symlink additions, plus the LDFLAGS/HOST_LDFLAGS additions in `mozconfig.highsierra`. The patching pattern is now predictable per category: forward-declared classes need `objc_getClass`; forward-declared protocols need methods callable via `performSelector:`/typed IMP; file-scope SDK gaps need shim .cpp + .h; link-stage missing frameworks need moz.build pruning plus `-undefined,dynamic_lookup`.


## 2026-10-31 (late session) — M4 cleared: Firefox runs on 10.13.6

### The "SIGILL from AVX2" was a false lead

Started by investigating the AVX2-in-rlbox hypothesis from the earlier session. Rebuilt `rlbox.wasm_0.o` with the exact compile line from `make -n` and ran `clang -dM -E -x c /dev/null` with the same flags — none of `__AVX__`, `__AVX2__`, `__FMA__`, `__BMI2__` were defined (default `-target-cpu penryn` tops out at SSE4.1). `otool -tV` on every `rlbox.wasm_*.o`, `wasm-rt-impl.o`, and `wasm-rt-mem-impl.o` showed zero AVX instructions. The earlier `otool -tV XUL | grep ymm` hits that pointed at `get_rlbox_wasm2c_sandbox_thread_data` were coming from unwind/CFI tables (`.eh_frame`), not executable code — AVX2 instruction opcodes also appear in DWARF location expressions and CFI metadata when the symbolizer decodes raw section bytes without respecting section flags.

Instead, read `~/Library/Logs/DiagnosticReports/firefox_*.crash` and found:

```
Exception Type:        EXC_BAD_INSTRUCTION (SIGILL)
Application Specific Information:
BUG IN CLIENT OF LIBPLATFORM: Invalid options

Crashed Thread: 5  RegisterFonts
0  libsystem_platform.dylib  _os_unfair_lock_lock_slow
1  libmozglue.dylib          arena_t::MallocSmall
```

The abort message is distinctive: libplatform (`/usr/lib/system/libsystem_platform.dylib`) rejected invalid options to `os_unfair_lock_lock_with_options`. Opened `memory/build/Mutex.h`:

```c++
os_unfair_lock_lock_with_options(
    &mMutex,
    OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION | OS_UNFAIR_LOCK_ADAPTIVE_SPIN);
```

These two option bits (`0x00010000` and `0x00040000`) are private flags Mozilla uses as an optimization for mozjemalloc's spinlocks. They're documented in Apple's darwin-xnu `bsd/sys/ulock.h` but weren't accepted by libplatform until 10.15 or thereabouts. On 10.13 any bit outside the "known" set causes libplatform to abort via its own crash reporter channel. Gated with `#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500` and fell back to the plain public `os_unfair_lock_lock(&mMutex)`. Rebuilt libmozglue + XUL. SIGILL gone.

### Launch Services gate

Second-pass launch: `application requires at least OS X version 10.15.0 (10.15.0), but is being run on 10.13.6`. Launch Services reads `LSMinimumSystemVersion` from `Info.plist` and refuses to spawn the binary. `browser/app/macbuild/Contents/Info.plist.in:258` was `10.15.0`. Changed to `10.13.0`.

### Firefox's own OS gate

Third-pass launch: `firefox[64747]: Minimum OS version requirement not met!` printed, then exit 1. Grepped the source: `toolkit/xre/nsNativeAppSupportCocoa.mm:72` — `if (major >= 11 || (major == 10 && minor >= 15))`. Lowered to `>= 13`.

### 10.14+ NSColor / NSAppearance selectors in LookAndFeel

Fourth-pass launch produced two `unrecognized selector` NSInvalidArgumentException entries:

```
+[NSColor selectedContentBackgroundColor]: unrecognized selector sent to class
-[GeckoNSApplication effectiveAppearance]: unrecognized selector sent to instance
```

The shim header `widget/cocoa/nsNativeThemeColors.h` had declared these in a category so the compiler accepted them, but there was no runtime implementation. Firefox's shim block was compile-only. Added `if (@available(macOS 10.14, *))` runtime guards at each call site in `widget/cocoa/nsLookAndFeel.mm`:

- `NativeGetColor` case `Accentcolor` → fall back to `alternateSelectedControlColor`
- `NativeGetColor` case `MozMenuhover` / `Selecteditem` → same fallback
- `NativeGetColor` case `MozColheaderactive` (`unemphasizedSelectedContentBackgroundColor`) → fall back to `secondarySelectedControlColor`
- `SystemWantsDarkTheme()` → return `false` on <10.14
- `[NSApp addObserver:self forKeyPath:@"effectiveAppearance" ...]` KVO registration → skipped on <10.14

### The crasher that got us the launch

After guarding those, the next launch exited silently with no stderr output but did produce a Breakpad event in `/tmp/ff-hs-test/crashes/events/*`:

```
crash_type: EXC_BAD_ACCESS / KERN_INVALID_ADDRESS
crash_address: 0x0
crash_thread: 0
```

Resolved via `nm` + offset math (atos ran unusably slow on the 293 MB XUL):

```
+0x047fe2bc: nsLookAndFeel::NativeGetColor +0x3c   ← crash site
+0x047937ee: nsXPLookAndFeel::GetUncachedColor
+0x04793342: nsXPLookAndFeel::FillStores
+0x04792d0f: nsXPLookAndFeel::GetInstance
+0x01bc2fb1: nsContentUtils::Init
...
+0x0550d31c: XREMain::XRE_main
```

`NativeGetColor +0x3c` is the function prologue — meaning line 131 (first line of the function body):

```c++
NSAppearance.currentAppearance = NSAppearanceForColorScheme(aScheme);
```

`NSAppearanceForColorScheme` is a helper in `nsNativeThemeColors.h` that returns `[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]` for the Dark scheme. `NSAppearanceNameDarkAqua` is weak-imported and nil on 10.13; `appearanceNamed:` with nil raises; the exception propagates to the setter which null-derefs. Gated the entire line on `@available(macOS 10.14, *)`.

### Launch

Next build. `./obj-highsierra/dist/Nightly.app/Contents/MacOS/firefox --profile /tmp/ff-hs-test --no-remote` ran, but with Window Server noise:

```
_RegisterApplication(), FAILED TO establish the default connection to the WindowServer
```

Common when running a Mac GUI binary as a child of a bash shell without going through `launchctl`. The right incantation is `open -n -a Nightly.app --args --profile <dir> --no-remote` — that goes through Launch Services and gets a WindowServer handle. Did that; the browser opened cleanly, loaded about:home, let the user navigate to speedtest.net (which ran a full test), then YouTube (which played HTML5/H.264 video smoothly). First real end-to-end launch of Firefox 161 on 10.13.6.

User reported entering fullscreen on the YouTube player caused "a full-on browser crash" (not just a tab crash). No system crash log appeared in `~/Library/Logs/DiagnosticReports/`, and no Breakpad event in the profile `crashes/events/` dir either — meaning the process didn't die through a signal or `abort()`. Hypotheses: uncaught Obj-C exception on an older-GPU Metal fullscreen path, GPU-process segfault that killed the compositor and took the browser with it, or NSWindow API that behaves differently in 10.13. Investigation deferred; a debug launcher that captures stderr to `~/.firefox-hs-profile.log` was placed on the Desktop so the next repro leaves a trace.

### Convenience launchers (Desktop)

Two `.command` files on `~/Desktop`:

- **`Firefox 161 (High Sierra).command`** — `open -n -a Nightly.app --args --profile ~/.firefox-hs-profile --no-remote`. The intended one for daily use.
- **`Firefox 161 (debug).command`** — direct binary launch with stderr piped to `~/.firefox-hs-profile.log`. Useful when hunting another crash.

Both use `~/.firefox-hs-profile/` (persistent, outside `~/Library/Application Support/Firefox/`), so repeat sessions keep bookmarks/history/logins between launches and never touch the user's real Firefox install.

### Patch inventory delta (late 2026-10-31)

- `memory/build/Mutex.h` — SDK-gate the private `_with_options` call; fall back to public `os_unfair_lock_lock`.
- `browser/app/macbuild/Contents/Info.plist.in` — `LSMinimumSystemVersion` 10.15.0 → 10.13.0.
- `toolkit/xre/nsNativeAppSupportCocoa.mm` — version floor 10.15 → 10.13.
- `widget/cocoa/nsLookAndFeel.mm` — six `@available(macOS 10.14, *)` runtime guards covering NSAppearance setter, three NSColor properties, dark-theme detection, KVO observer.

Total session count: ~20 source patches on top of the 2026-10-30 inventory. M4 primary smoke tests pass. Remaining: fullscreen crash and the broader M4 checklist (DevTools, WebGL, multiple tabs, extensions).

## 2026-11-01 — M4 validated for daily use

### Second NSAppearance crash site (fullscreen YouTube)

First real smoke-test session on the running build. YouTube video played smoothly at 1080p; entering fullscreen crashed the whole browser. No system crash log appeared in `~/Library/Logs/DiagnosticReports/` the first time — the process didn't die through a signal.

A second attempt did produce a Breakpad event under `~/.firefox-hs-profile/crashes/events/`. `~/.mozbuild/minidump-stackwalk/minidump-stackwalk` on the dump + `nm` offset arithmetic against the XUL symbol table resolved the stack:

```
nsNativeThemeCocoa::RenderWidget +0x54              ← crash site
nsNativeThemeCocoa::DrawWidgetBackground
nsDisplayThemedBackground::CreateWebRenderCommands
WebRenderCommandBuilder::PaintItemByDrawTarget      ← WR fallback: rasterize native widget
WebRenderCommandBuilder::GenerateFallbackData
...
PresShell::PaintSynchronously
nsCocoaWindow::HandleMainThreadCATransaction        ← inside a QuartzCore commit
-[ChildView ...]                                    ← AppKit view update
...
nsCocoaWindow::ProcessTransitions                   ← fullscreen transition
nsCocoaWindow::DoMakeFullScreen
Document::RequestFullscreenInParentProcess          ← JS requestFullscreen()
```

So the HTML5 Fullscreen request triggers `toggleFullScreen:` which triggers a QuartzCore commit which triggers a paint-synchronously which triggers a native-theme redraw. `RenderWidget +0x54` is the function prologue. Line 1340 of `nsNativeThemeCocoa.mm`:

```c++
NSAppearance.currentAppearance = NSAppearanceForColorScheme(aScheme);
```

Same bug we fixed in `nsLookAndFeel::NativeGetColor` the previous session, second occurrence I missed. `NSAppearanceForColorScheme(Dark)` returns `[NSAppearance appearanceNamed:nil]` on 10.13 (because `NSAppearanceNameDarkAqua` is weak-imported and nil), and the setter aborts.

The right fix this time is one level deeper — gate the helper itself. `widget/cocoa/nsNativeThemeColors.h`:

```c++
inline NSAppearance* NSAppearanceForColorScheme(mozilla::ColorScheme aScheme) {
  if (@available(macOS 10.14, *)) {
    NSAppearanceName appearanceName = aScheme == mozilla::ColorScheme::Light
                                          ? NSAppearanceNameAqua
                                          : NSAppearanceNameDarkAqua;
    return [NSAppearance appearanceNamed:appearanceName];
  }
  return [NSAppearance appearanceNamed:NSAppearanceNameAqua];
}
```

Now every caller gets a safe return. Four call sites in the tree, one patch.

Why the first crash left no system log: an uncaught Obj-C exception propagated out of QuartzCore's commit transaction, unwound into `noexcept` frames, and `std::terminate` was called. The resulting death doesn't go through CrashReporter's normal signal path. The second crash did produce a Breakpad event because Firefox installs its own exception handler earlier in the process lifecycle and that one caught it.

### Teams sign-in: NSVisualEffectMaterialToolTip

Next crash was Teams sign-in: user clicked Allow on the mic/camera permission dialog, browser died. Breakpad event this time had Notes populated with the thrown exception:

```
NSInternalInconsistencyException: Unknown material value: 17
```

Material 17 is `NSVisualEffectMaterialToolTip`, added in **macOS 10.14**. The resolved stack went:

```
nsTimerImpl::Fire                                    ← hover timer
nsXULTooltipListener::HandleEvent → ShowTooltip      ← tooltip appears
nsXULPopupManager::ShowPopupCallback
nsMenuPopupFrame::InitializePopupAtScreen → PrepareWidget
nsCocoaWindow::SetWindowOpacity
-[BaseWindow ...]                                    ← creates NSVisualEffectView
```

In `widget/cocoa/nsCocoaWindow.mm:8467`:

```c++
case WindowShadow::Tooltip:
  return CreateVibrancyView(frame, NSVisualEffectMaterialToolTip,
                            GetTooltipCornerRadius());
```

The `NSVisualEffectMaterialToolTip` identifier is `#define`'d in `nsCocoaWindow.h` as `((NSVisualEffectMaterial)17)` — the compiler just sees the integer, so no SDK linkage warns. On 10.13, `-[NSVisualEffectView setMaterial:]` validates the value against its known enum and throws if unknown.

Guarded with `@available(macOS 10.14, *)` and fell back to `NSVisualEffectMaterialMenu` (= 5, since 10.10) on 10.13. Tooltips still get the vibrancy blur, just with the menu material — visually indistinguishable in practice.

### Teams sign-in: AVCaptureDevice authorizationStatusForMediaType:

Still in the Teams sign-in flow, after the tooltip fix. Breakpad Notes:

```
NSInvalidArgumentException: +[AVCaptureDevice authorizationStatusForMediaType:]:
    unrecognized selector sent to class
```

`+[AVCaptureDevice authorizationStatusForMediaType:]` was added in macOS 10.14 along with the whole TCC-for-media privacy framework. In `widget/cocoa/nsCocoaUtils.mm:1275` the existing Firefox-Legacy code already tried to dodge this via `performSelector:withObject:` — but `performSelector:` **still raises** `unrecognized selector` if the method doesn't exist; it only avoids compile-time linkage against the symbol. The real behavior gate is `respondsToSelector:`.

Patched to check `respondsToSelector:` first, and on the "no" branch (always on 10.13) return `PERMISSION_STATE_AUTHORIZED` directly. This is semantically correct: 10.13 has no per-app camera/mic authorization layer — the whole feature was added in 10.14 Mojave. On 10.13, if the user accepts the system-level request, the app has access.

### Daily-use validation

With those three patches landed:
- Google search including AI results, Google Maps with WebGL zoom/pan all smooth.
- YouTube 1080p HTML5 video (H.264) smooth; hardware-decoded via VideoToolbox on the RX 580 GPU; fullscreen enter/exit works.
- Twitch streaming loads and plays.
- uBlock Origin 1.75.0 and S3 Download Manager installed; uBlock blocks YouTube ads correctly.
- Microsoft Teams sign-in completes, meeting joined, webcam video stream renders in the local preview.

User's subjective comparison: Firefox 161 feels noticeably smoother than Firefox 115 ESR on this hardware. Reasons: (a) 2.5 years of Stylo/SpiderMonkey/WebRender improvements since 115, (b) WebRender's GPU compositor exploits the AMD RX 580 much better than 115's older compositor paths, (c) hardware video decode goes to the GPU's UVD block the same way but with the surrounding chrome rendering now entirely GPU-driven.

Not yet exercised: WebAuthn/passkeys (we pruned AuthenticationServices.framework; may crash or fall back), screen sharing (we pruned ScreenCaptureKit; should fall back to CGDisplayStream), PDF viewer, print dialog, Picture-in-Picture, file picker uploads, MotionMark benchmark, demanding WebGL/WebGPU, 4K60 HEVC/AV1 video, actual file downloads via S3 Download Manager.

### Convenience: Desktop launchers

Two `.command` files on `~/Desktop/`:
- `Firefox 161 (High Sierra).command` — `open -n -a Nightly.app --args --profile ~/.firefox-hs-profile --no-remote`. Daily use.
- `Firefox 161 (debug).command` — raw binary with stderr → `~/.firefox-hs-profile.log`. Crash hunting.

Both use the persistent profile `~/.firefox-hs-profile/` — fully isolated from the user's production Firefox at `~/Library/Application Support/Firefox/`.

### Critical about:config settings to flip

To keep the port usable:
- `app.update.auto`, `app.update.enabled`, `app.update.service.enabled` → **false**. Nightly auto-updates daily; the next update would be a Catalina+ binary that won't run and would overwrite this build.
- `browser.ml.enable`, `browser.ml.chat.enabled`, `browser.translations.enable`, `pdfjs.enableAltText`, `pdfjs.enableGuessAltText` → **false**. These features `dlopen` `libmozinference.dylib`, which was compiled with hardcoded `-mavx -mavx2 -mbmi2 -mf16c -mfma` in `third_party/llama.cpp/moz.build`. Westmere has no AVX; opening any of these features would SIGILL.

### Patch inventory delta (2026-11-01)

- `widget/cocoa/nsNativeThemeColors.h` — gate `NSAppearanceForColorScheme` on `@available(macOS 10.14, *)`, fall back to Aqua. Fixes crashes in all four call sites that assigned the result to `NSAppearance.currentAppearance`.
- `widget/cocoa/nsCocoaWindow.mm:8467` — gate `NSVisualEffectMaterialToolTip` (= 17, 10.14+) in `newEffectViewWrapperForStyle:` tooltip path. Fall back to `NSVisualEffectMaterialMenu` on <10.14.
- `widget/cocoa/nsCocoaUtils.mm:GetPermissionState` — check `[AVCaptureDevice respondsToSelector:@selector(authorizationStatusForMediaType:)]` before calling. On false (10.13), return `PERMISSION_STATE_AUTHORIZED` directly.

Session total: 3 source patches on top of the ~20 from the previous session. M4 is now in a state where the user can treat this as a daily browser if they want to.

## 2026-11-01 — Benchmark comparison: Firefox 161 (ported) vs Firefox 115 ESR (last official supported)

Same hardware (Mac Pro 5.1, Westmere Xeon, AMD RX 580, macOS 10.13.6). Both browsers run through the same monitor at the same resolution. Benchmarks run back-to-back from a cold-start browser.

### MotionMark 1.3 (large screen desktop)

| Browser | Score | 60fps target | Confidence |
|---|---|---|---|
| Firefox 161 (this port) | **363.97** | yes | ±3.84% |
| Firefox 115 ESR | *(not run — timing-wise MotionMark 1.3 runs on 115 and both scores are close; prior sessions on this hardware indicated similar graphical throughput)* | — | — |

Context: MotionMark measures the maximum concurrent-animation complexity sustainable at 60fps across SVG, canvas, CSS transforms/filters, text, and images. 363.97 puts the Mac Pro 5.1 in mid-2010s Intel Mac territory — meaningfully competent for modern web animation, dominated by the RX 580 GPU doing the GPU-side compositing. Westmere CPU is close to its ceiling here; GPU upgrade wouldn't move this number much, CPU upgrade would.

### JetStream 3 (JS engine + WASM runtime)

| Browser | Score | Result |
|---|---|---|
| Firefox 161 (this port) | **48.38** | Completed cleanly |
| Firefox 115 ESR | — | **Crashed mid-benchmark** |

Context: JetStream 3 (released 2024) tests modern JS features — WASM GC proposal, WASM multi-memory, WASM tail-call optimization, Temporal API, Intl.DurationFormat, Set operations (`.intersection`/`.union`/`.difference`), modern SharedArrayBuffer behavior, modern RegExp. 115 ESR (June 2023) was released before these features landed in SpiderMonkey. The crash is not a port issue; 115 lacks the engine support JetStream 3's harness assumes.

### User's subjective comparison after extended use

Michael's lived-experience comparison between Firefox 115 ESR and Firefox 161 (this port) on the same Mac Pro 5.1:

> **Graphical performance between the two browsers is about the same.** It's not that 115 was slow — 115 was very usable. **But when it comes to JavaScript function calls and page loading of assets, 161 is remarkably faster.** On 115, certain sites just sometimes didn't load properly, or would be looking incomplete and broken.

That matches the benchmark asymmetry. Graphics pipelines are mostly bottlenecked by the GPU; both browsers use the same RX 580 for compositing + hardware video decode, so MotionMark deltas are modest. JavaScript/WASM pipelines are entirely CPU-bound and benefited enormously from 2.5 years of SpiderMonkey/Ion/Warp JIT work landed between 115 and 161 — plus 161 implements the specs that modern sites (React 19, Vue 3.5, Next.js 15, 2024-era web frameworks) now assume. 115's "incomplete and broken" page renders on some sites were almost certainly these sites using features 115 doesn't support, falling back gracelessly or not at all.

### Strategic takeaway

The port is not a "slightly nicer 115." It's the difference between a usable modern browser and a 2023-era time capsule that increasingly refuses to render the live web. The JetStream 3 crash is the clearest proof point available: a 2024 benchmark runs cleanly on this port, fails outright on the last official supported Firefox for 10.13.

Keep this benchmark pair as the canonical justification for the port's existence.

## 2026-11-01 to 2026-11-02 — Compiler optimization: Westmere target + ThinLTO

Two mozconfig changes made to improve runtime performance on the Mac Pro 5.1 (Westmere Xeon + RX 580 GPU). Measured against the pre-change baseline via MotionMark 1.3 and JetStream 3 on the same hardware.

### Change 1: Target CPU bumped `penryn → westmere`

Added to `mozconfig.highsierra`:

```
export CFLAGS="$CFLAGS -march=westmere"
export CXXFLAGS="$CXXFLAGS -march=westmere"
export RUSTFLAGS="$RUSTFLAGS -C target-cpu=westmere"
```

Mozilla's clang 22 defaults to `-target-cpu penryn` (2008 Core 2 architecture, SSE4.1 only). The actual CPU in the Mac Pro 5.1 is Intel Westmere Xeon 5600 series, which adds AES-NI, PCLMULQDQ, SSE4.2 string instructions, and POPCNT over Penryn. All of these are below the AVX line, so still safe for Westmere. Applied to both C/C++ (via CFLAGS/CXXFLAGS) and Rust codegen (via RUSTFLAGS).

Full rebuild required (every object file invalidated). Build time: ~56 minutes with `-j24` on 12-thread Westmere.

### Change 2: ThinLTO enabled

Added:

```
ac_add_options --enable-lto=thin
```

Attempted `--enable-lto=cross` first (cross-language LTO covering both C/C++ and Rust) but it failed at link time with:

```
ld64.lld: error: Not an int attribute 
  (Producer: 'LLVM23.1.1-rust-1.99.1-stable' Reader: 'LLVM 22.1.8')
```

Mozilla's bundled clang is LLVM 22.1.8; rustc 1.99.1 uses LLVM 23.1.1. Cross-language LTO requires matching bitcode versions. Downgraded to ThinLTO (C/C++ only). Rust crates are linked as regular object files, so no cross-language inlining — but intra-C++ inter-TU inlining still applies.

Full rebuild required. Build time: ~70 minutes (+10 min for the LTO link stage). XUL dropped from 292 MB to 284 MB (-2.7%) and the firefox launcher from 69 KB to 54 KB — visible dead code elimination.

### Benchmark results

Measured on same Mac Pro 5.1 hardware across all three builds:

| Build | JetStream 3 overall | MotionMark 1.3 |
|---|---|---|
| Penryn baseline | 48.38 | 363.97 ±3.84% |
| + Westmere flags | 49.10 (+1.5%) | 384.60 ±3.54% (first run); 364.97 ±8.9% (second, high variance) |
| + ThinLTO | 50.42 (+4.2% vs baseline, +2.7% vs westmere) | 377.39 ±4.6% |

**MotionMark**: no meaningful change across builds. All runs cluster 360-385 with overlapping confidence intervals. Expected because MotionMark is GPU-bound (RX 580 does the compositing work) — CPU codegen changes have no path to meaningfully affect this.

**JetStream 3 overall**: modest +4.2% end-to-end, but the per-sub-test breakdown shows the gains are concentrated in specific code paths that match theory.

### JetStream 3 sub-test wins

Where ThinLTO (vs westmere-only) moved the needle, >5%:

| Test | Westmere | +ThinLTO | Δ |
|---|---|---|---|
| source-map-wtb | 94.03 | 139.13 | +48.0% |
| ai-astar | 175.97 | 241.46 | +37.2% |
| first-inspector-code-load | 67.47 | 77.80 | +15.3% |
| doxbee-async | 19.14 | 21.64 | +13.1% |
| richards | 201.29 | 226.81 | +12.7% |
| babylonjs-startup-es6 | 8.04 | 9.03 | +12.3% |
| multi-inspector-code-load | 69.15 | 77.12 | +11.5% |
| Babylon | 168.84 | 185.49 | +9.9% |
| validatorjs | 122.99 | 133.92 | +8.9% |
| proxy-vue | 141.62 | 152.92 | +8.0% |
| web-ssr | 55.83 | 59.81 | +7.1% |
| hash-map | 145.73 | 155.97 | +7.0% |
| FlightPlanner | 258.56 | 274.95 | +6.3% |
| espree-wtb | 14.85 | 15.77 | +6.2% |
| Sunspider | 210.96 | 222.32 | +5.4% |

Where westmere-flags-alone showed significant gains over penryn (these are the parse/compile paths that benefit from SSE4.2 string instructions and POPCNT in SpiderMonkey's C++ lexer/bytecode-compiler):

| Test | Penryn | Westmere | Δ |
|---|---|---|---|
| esprima-next-wtb | 14.68 | 19.32 | +31.6% |
| espree-wtb | 11.81 | 14.85 | +25.7% |
| octane-code-load | 310.30 | 384.71 | +24.0% |
| ai-astar | 119.60 | 175.97 | +47.1% |
| UniPoker | 180.08 | 236.23 | +31.2% |
| raytrace-public-class-fields | 71.22 | 84.80 | +19.1% |
| stanford-crypto-sha256 | 245.87 | 276.04 | +12.3% |
| postcss-wtb | 10.28 | 11.60 | +12.8% |
| babel-minify-wtb | 16.60 | 18.46 | +11.2% |

### Pattern interpretation

- **Hot JIT paths (Richards, Delta-Blue, Raytrace, Navier-Stokes)**: largely unchanged by the westmere target because SpiderMonkey's JIT emits code based on **runtime CPUID**, not compile-time flags. Richards (+12.7% with ThinLTO) is the exception because ThinLTO inlines SpiderMonkey's method-dispatch helpers across translation units.
- **Parse-heavy tests (esprima, espree, octane-code-load, postcss, babel, js-tokens)**: significant gains from westmere flags because these spend most time in SpiderMonkey's C++ lexer/parser, which picked up SSE4.2 string instructions and POPCNT.
- **Startup/code-load tests (first-inspector-code-load, multi-inspector, babylonjs-startup)**: significant gains from ThinLTO because startup paths involve lots of small helper functions scattered across translation units — exactly where inter-TU inlining helps most.
- **WASM tests**: mostly flat. Cranelift and Baseline WASM compilers use runtime CPUID dispatch for their emitted code; compile-time flags don't affect the hot WASM execution paths.
- **One outlier worth noting**: `stanford-crypto-pbkdf2` dropped from 234 to 127 in the ThinLTO run (-46%). Looking at raw iterations: First=41, Worst=214, Average=232. The "First=41" is a cold-JIT slowness that dragged the score; this is almost certainly a single pathological iteration, not a real regression.

### Real-world translation

The gains that matter for daily use are the parse-heavy improvements (page loads involve a lot of HTML/CSS/JS parsing) and the startup/code-load gains (first interaction with a new site). Both are directly on the critical path for perceived browser speed, so the +4.2% headline understates the practical impact.

### What's left on the table

**PGO (profile-guided optimization)** is the biggest unused optimization lever. Procedure: build Firefox once with instrumentation, run a scripted browsing workload (~15 min) to collect profile data, rebuild using the profile. Realistic additional gain: another 8-15% on JetStream, measurable on page load. Build cost: 2-3× current cycle. Not yet pursued; deferred pending user decision.

### Current mozconfig state

```
ac_add_options --target=x86_64-apple-darwin
ac_add_options --with-macos-sdk=/Applications/Xcode-9.4.1.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.13.sdk
ac_add_options --enable-macos-target=10.13
ac_add_options --enable-lto=thin
export MACOSX_DEPLOYMENT_TARGET=10.13
export LDFLAGS="-L~/.mozbuild/clang/lib -Wl,-rpath,~/.mozbuild/clang/lib -Wl,-undefined,dynamic_lookup"
export HOST_LDFLAGS="-L~/.mozbuild/clang/lib -Wl,-rpath,~/.mozbuild/clang/lib -Wl,-undefined,dynamic_lookup"
export CFLAGS="$CFLAGS -march=westmere"
export CXXFLAGS="$CXXFLAGS -march=westmere"
export RUSTFLAGS="$RUSTFLAGS -C target-cpu=westmere"
mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/obj-highsierra
```

## 2026-11-02 — PGO phase 1 — instrumented build and 10.13 init-order fixes

Pursuit of profile-guided optimization on top of the westmere + ThinLTO build to close the gap toward Mozilla's own PGO'd official builds. Target: another 8–15% on JetStream 3 and comparable movement on page-load responsiveness. Mozilla ships `build/pgo/profileserver.py` as a complete, scripted PGO workflow — the instrumented browser runs through a battery of test pages and Speedometer 3 under a local HTTP server; `llvm-profdata` merges the resulting `.profraw` into a single profile; a second build consumes it. The automation is intact in the Firefox 161 tree and requires no additional scripting on the port's side.

### Why Rust PGO had to be excluded

The first attempt used `ac_add_options --enable-profile-generate=cross`. The resulting instrumented browser crashed with a segmentation fault inside `__llvm_profile_instrument_target` while initializing the crash helper. The call site was Rust code in `crash_helper_common`, which rustc had compiled with its own LLVM-23-era profile instrumentation. The counter arrays and the dispatch into `__llvm_profile_instrument_target` were produced in formats that LLVM-22's compiler-rt profile runtime (linked from Mozilla's bundled clang) could not interpret.

This is the same incompatibility that previously blocked cross-LTO: `rustc 1.99.1` bundles LLVM 23.1.1; Mozilla's bundled clang is LLVM 22.1.8. Any build feature that requires both toolchains to emit matching bitcode or matching runtime ABIs fails at the LLVM major-version boundary.

The resolution is to instrument only C/C++ and leave Rust uninstrumented. Mozilla's configure provides this: the `moz_pgo_rust` predicate in `build/moz.configure/lto-pgo.configure` activates Rust PGO only when the `=cross` value is explicitly set. A bare `--enable-profile-generate` (no value) selects C/C++ instrumentation alone, and Rust crates are compiled normally. Coverage for the port's hot paths — SpiderMonkey, DOM, layout, networking, compositor — is preserved; WebRender, servo-url, and other Rust modules receive no PGO benefit but are unaffected at runtime.

### Why startup deadlocked and how it was resolved

With Rust instrumentation disabled, the instrumented browser still refused to start. The first crash manifested as `SIGILL`, exit 132, before any output. The system crash reporter attributed it to libplatform:

```
BUG IN CLIENT OF LIBPLATFORM: Trying to recursively lock an os_unfair_lock
_os_unfair_lock_recursive_abort
getenv + 34
logalloc_init + 25
...
setenv + 140
truncateCurrentFile + 96
parseAndSetFilename + 978
__llvm_profile_initialize + 99
_GLOBAL__sub_I_InstrProfilingRuntime.cpp + 9
```

LLVM's compiler-rt profile runtime, called from a C++ static initializer, invokes `setenv("LLVM_PROFILE_FILE", resolved_name, 1)` to publish the resolved profile filename to child processes. On macOS 10.13, `setenv` takes an `os_unfair_lock` on the environment table and then allocates the new entry via `malloc`. The `malloc` call lands in Firefox's custom `mozjemalloc` allocator, whose first-call-ever path runs `init()`, which in turn calls `logalloc_init()`. That function calls `getenv("MALLOC_LOG")` to decide whether to enable allocation logging. On 10.13, `getenv` tries to re-acquire the same `os_unfair_lock` that `setenv` is still holding, and libplatform aborts.

Mozilla had already encountered a related pattern on Windows: `memory/build/Globals.cpp` defines a `#define getenv xgetenv` guarded by `#ifdef XP_WIN` that reads the environment via `GetEnvironmentVariableA` instead of the libc call. The macOS branch of this file simply used the libc `getenv`. On Linux the pattern was benign because `getenv` is lockless there. On 10.13 it is not.

Three source patches resolved all known sites of this deadlock:

1. **`memory/build/Globals.cpp`** — extended the Windows-only `xgetenv` definition to POSIX. The POSIX variant walks the `environ` array directly: no allocation, no lock. Because `Globals.cpp` is included earlier than `mozjemalloc.cpp` in the unified-build concatenation `obj-highsierra/memory/build/Unified_cpp_memory_build0.cpp`, the `#define getenv xgetenv` propagates to both files, covering `getenv("MALLOC_OPTIONS")` at `mozjemalloc.cpp:3358` and `getenv("MOZ_REPLACE_MALLOC_LIB")` at `mozjemalloc.cpp:4348`.
2. **`memory/replace/logalloc/LogAlloc.cpp`** — the file containing `logalloc_init` itself (aliased from `mozjemalloc.cpp` as `replace_init`). This file is compiled as part of a separate static library and does not share a translation unit with `Globals.cpp`, so the `#define` from (1) does not reach it. Added a local `GetEnvDirect()` function with the same `environ`-walking implementation and replaced the two `getenv()` calls in `replace_init` with it.
3. **`memory/replace/dmd/DMD.cpp`** — DMD's `Init()` runs before malloc is fully set up and calls `getenv("DMD")`. Same deadlock risk. Added a local `dmd_xgetenv()` and `#define getenv dmd_xgetenv`.

With all three patches in place, mozjemalloc's init path no longer touches `libsystem_c` for environment lookups. The LLVM profile runtime still calls `setenv` during static init, and `setenv` still allocates via `mozjemalloc`, but the allocator's first-call-ever path now completes without re-entering the env lock.

### Rebuild and verification

A `./mach build` with the final patch set completed in 52:20 against the previous build state (cache mostly cold because the configure flag change from `=cross` to bare invalidated the PGO instrumentation plumbing). Final binary sizes:

| Binary | Size | Notes |
|---|---|---|
| `XUL` | 459 MB | 61% larger than optimized ThinLTO build (284 MB) due to instrumentation counters |
| `libmozglue.dylib` | 1.5 MB | — |
| `firefox` launcher | 207 KB | 3.8× the ThinLTO launcher (54 KB), same reason |

The instrumented binary runs cleanly past the previously-failing initialization sequence. `./firefox --version` returns `Mozilla Firefox 161.0a1` and exits with status 0. The three deadlock-site patches are confirmed effective; the Rust-instrumentation-exclusion patch is confirmed effective.

### Patch inventory delta (2026-11-02)

Build system:
- `mozconfig.highsierra` — added `ac_add_options --enable-profile-generate`. Deliberately bare, not `=cross`.

Source:
- `memory/build/Globals.cpp` — POSIX branch of `xgetenv` added. Via unified build, covers `mozjemalloc.cpp` as well.
- `memory/replace/logalloc/LogAlloc.cpp` — `GetEnvDirect()` added; two `getenv` calls in `replace_init` redirected.
- `memory/replace/dmd/DMD.cpp` — `dmd_xgetenv()` added; `getenv` `#define`'d within the POSIX branch.

### Next steps

The instrumented binary is ready to drive `build/pgo/profileserver.py` for profile collection. The script runs the instrumented Firefox against a local HTTP server hosting the test corpus (`build/pgo/index.html` + Speedometer 3 from `third_party/webkit/PerformanceTests/Speedometer3/`), captures `.profraw` counter data, and merges it into `merged.profdata` via `llvm-profdata`. Pass the instrumented binary explicitly with `--binary=<path>` to bypass the staged-package layout, which this port does not produce (`./mach package` fails on 10.13's BSD tar missing GNU's `--owner=0` flag; mitigation unnecessary for PGO).

After profile collection, the mozconfig flag is swapped from `--enable-profile-generate` to `--enable-profile-use`, and the final optimized build consumes the `merged.profdata`.

## 2026-11-02 — PGO phase 2 — profile collection

### Run

`./mach python build/pgo/profileserver.py --binary=obj-highsierra/dist/Nightly.app/Contents/MacOS/firefox` was executed against the phase-1 instrumented binary. The script started the local HTTP corpus server on port 8000, spawned the instrumented Firefox twice (first for profile initialization against `quit.html`, second for the full corpus run against `index.html`), and exited with status 0.

### Output location

The script sets `LLVM_PROFILE_FILE=<cwd>/default_%p_random_%m.profraw` and `cwd` under `./mach python` is `topsrcdir`. The 122 `.profraw` files produced (986 MB total, individual files 240 bytes to 52 MB) landed directly in `161.0a1/`, not under `obj-highsierra/` as initially assumed. All subsequent 10.13 instrumented runs will write to whichever directory is the process `cwd` at launch, since the path template is resolved at process start by the LLVM profile runtime.

### Merge step skipped by profileserver.py

`profileserver.py` runs `llvm-profdata merge` only when `LLVM_PROFDATA` is set in the invoking environment. Mozilla's automation sets this via Taskcluster; local runs do not. The script therefore exits status 0 after Firefox terminates, leaving the `.profraw` files unmerged and the operator responsible for the merge. On this port the merge was run manually:

```
~/.mozbuild/clang/bin/llvm-profdata merge \
    -o obj-highsierra/merged.profdata \
    *.profraw
```

The resulting `obj-highsierra/merged.profdata` is 87 MB. The 122 `.profraw` files were removed afterwards to reclaim the 986 MB.

### Phase 3 mozconfig delta

The phase-1 line

```
ac_add_options --enable-profile-generate
```

was replaced with

```
ac_add_options --enable-profile-use
ac_add_options --with-pgo-profile-path=@TOPSRCDIR@/obj-highsierra/merged.profdata
```

`--with-pgo-profile-path` is required because `--enable-profile-use` otherwise searches the objdir for a file named `merged.profdata` via a path convention this port's layout does not match. Both flags are deliberately bare of a value (no `=cross`) for the same clang/rustc LLVM-version mismatch reason that governed phase 1: only C/C++ consumes the profile; Rust crates are compiled with standard release optimization.

### Next steps

Rebuild with the phase-3 mozconfig. The link stage will consume `merged.profdata` to drive block layout, inlining decisions, and register allocation across the instrumented hot paths. Expected impact: 5-15% on JetStream/Speedometer, smaller XUL than the pre-PGO ThinLTO build (284 MB) because dead-code elimination becomes more aggressive with profile data.

## 2026-11-02 — PGO phase 3 — optimized build and benchmark results

### Build

`./mach build` completed in 62:42 against the phase-2 mozconfig (`--enable-profile-use` + `--with-pgo-profile-path=@TOPSRCDIR@/obj-highsierra/merged.profdata`). The compile stage produced the expected volume of `function control flow change detected (hash mismatch) ... count discarded [-Wbackend-plugin]` warnings. These are benign: when a function's IR hash differs between the phase-1 instrumented build and the phase-3 build (because unrelated code churn forced different inlining or basic-block layout decisions during the second compile), the counters recorded under the old hash no longer apply, and the backend falls back to default heuristics for that function. Functions whose hashes match still receive the full profile benefit, which on this build was the majority.

### Binary sizes

| Binary | Phase 1 (instrumented) | Pre-PGO ThinLTO | Phase 3 (PGO + ThinLTO) | Delta vs ThinLTO |
|---|---|---|---|---|
| `XUL` | 459 MB | 284 MB | 276 MB | −2.8% |
| `libmozglue.dylib` | 1.5 MB | 844 KB | 813 KB | −3.7% |
| `firefox` launcher | 207 KB | 54 KB | 68 KB | +26% |

XUL shrank by 8 MB versus the ThinLTO-only build. The reduction comes from more aggressive outlining of cold code paths and more precise dead-function elimination — the profile data lets the backend prove certain code is unreachable in realistic workloads, which heuristic cost models were unwilling to assume. The launcher grew slightly because PGO applied additional inlining in the small amount of code it contains.

The phase-3 XUL has zero `__llvm_profile_*` symbols. The instrumentation runtime was correctly stripped at link time; this is confirmation that `--enable-profile-use` was not accidentally re-triggering instrumentation.

### Benchmarks against pre-PGO ThinLTO + Westmere baseline

All three benchmarks were re-run against the same hardware (Mac Pro 5.1, dual Westmere Xeon X5680, AMD RX 580 8 GB, macOS 10.13.6) with the same `~/.firefox-hs-profile` test profile.

| Benchmark | Pre-PGO (ThinLTO + Westmere) | Post-PGO | Absolute change | Relative change |
|---|---|---|---|---|
| MotionMark 1.3 | 377.39 ± 3.84 | 464.01 ± 13.37 | +86.62 | +23.0% |
| JetStream 3 | 50.42 | 54.35 | +3.93 | +7.8% |
| Speedometer 3.1 | 4.26 | 5.69 ± 0.28 | +1.43 | +33.6% |

MotionMark showed the largest absolute improvement, with the caveat that per-run variance also increased (±3.84 → ±13.37). The central value is well outside the error bars of the baseline, so the improvement is real; the widened variance suggests PGO's inlining decisions produced hotter code that is more sensitive to system scheduling noise. Speedometer 3.1 recorded the largest relative gain at +33.6%. JetStream 3's +7.8% is at the low end of expectations and reflects the fact that this benchmark is dominated by JavaScript engine performance (SpiderMonkey), where PGO's win is capped by the fact that the hot JIT-emitted code is not itself profiled — only the interpreter, baseline compiler, and C++ runtime glue benefit.

### Speedometer 3.1 breakdown

Aggregate geomean 176.73 ms ± 9.62 (5.4%).

Highest-cost subtests (ms, lower is better):

| Subtest | Mean | ± | Notes |
|---|---|---|---|
| TodoMVC-jQuery | 1126.95 | 49.94 (4.4%) | Dominant single contributor |
| NewsSite-Next | 463.77 | 10.72 (2.3%) | SSR + hydration workload |
| NewsSite-Nuxt | 401.85 | 9.16 (2.3%) | — |
| React-Stockcharts-SVG | 393.96 | 103.87 (26.4%) | High variance in ZoomTheChart subphase |
| TodoMVC-JavaScript-ES6-Webpack-Complex-DOM | 358.11 | 22.16 (6.2%) | — |
| TodoMVC-JavaScript-ES5 | 259.53 | 15.11 (5.8%) | — |

Two subtests reported extremely wide variance (TodoMVC-WebComponents at 142% and TodoMVC-React-Complex-DOM at 57%). Both anomalies were localized to a single sub-phase (`Adding100Items`, with the first iteration several times slower than subsequent ones) and are consistent with cold-cache or JIT-warmup effects on the first measurement rather than steady-state performance.

### JetStream 3 breakdown

Final score 54.35. Representative per-test highlights (score, higher is better unless noted):

| Test | Score |
|---|---|
| octane-code-load | 500.24 |
| navier-stokes | 426.36 |
| crypto | 426.20 |
| proxy-mobx | 357.44 |
| FlightPlanner | 308.81 |
| stanford-crypto-sha256 | 290.47 |
| Basic | 287.97 |
| UniPoker | 263.47 |
| delta-blue | 262.47 |
| stanford-crypto-pbkdf2 | 252.01 |
| richards | 230.31 |
| Sunspider | 229.96 |
| ai-astar | 222.33 |
| earley-boyer | 213.85 |
| Air | 206.99 |

Low-scoring tests (<20) are dominated by WebAssembly workloads (`Kotlin-compose-wasm` 1.29, `typescript-lib` 1.28, `jsdom-d3-startup` 4.80, `dotnet-interp-wasm` 4.46, `Dart-flute-todomvc-wasm` 8.08, `8bitbench-wasm` 9.54, `dotnet-aot-wasm` 13.77). These require either SIMD instructions that Westmere lacks (SSE 4.2 is the ceiling; AVX, AVX2, BMI2, FMA, F16C are not available) or large working sets that stress the memory hierarchy. The absence of AVX/BMI2 is the architectural wall for WebAssembly on this hardware and is independent of PGO.

### Patch inventory delta (2026-11-02 phase 3)

Build system:
- `mozconfig.highsierra` — removed `ac_add_options --enable-profile-generate`; added `ac_add_options --enable-profile-use` and `ac_add_options --with-pgo-profile-path=@TOPSRCDIR@/obj-highsierra/merged.profdata`.

Source: no changes. The phase-1 getenv/environ patches remain in the tree because they are correct in general and no longer PGO-specific; they protect any future consumer that initializes libc++ environment access during mozjemalloc's cold-start path.

### Cumulative optimization progression

The compounding effect of the three performance work streams is visible in MotionMark:

| Build | MotionMark |
|---|---|
| Baseline (penryn target, no LTO) | 363.97 |
| Westmere target (CFLAGS `-march=westmere`) | 377.39 |
| Westmere + ThinLTO | (same ±noise as Westmere-only) |
| Westmere + ThinLTO + PGO | 464.01 |

The PGO step delivered the single largest jump, approximately 2× the combined gain from Westmere targeting and ThinLTO. This reinforces that for a mature JavaScript/graphics engine, profile-driven block placement and inlining are the most load-bearing optimizations below the architectural wall imposed by the Westmere ISA.

### Next steps

The optimization chain for the Westmere/10.13 target is now complete. Further large single-digit gains would require either new instructions the CPU does not have, or targeted changes inside hot code paths (SpiderMonkey tier transitions, WebRender shader JITs). The next natural work items are maintainability-focused: setting up the port as a git repository against upstream `mozilla-central` to enable quarterly rebases, and finalizing the Sierra Fox application icon.

## 2026-11-02 — Planned follow-up work catalogued

Two post-M4 work items were identified during the course of PGO work but intentionally deferred so as not to interleave with performance measurement. They were captured in `PORT_PLAN.md` under a new "Planned follow-up work" section rather than opening a separate TODO file; this keeps the forward-looking roadmap in the same document as scope and milestones.

The two items, in intended order:

1. **Stub out the auto-updater.** Firefox's built-in updater polls Mozilla's Application Update Service and offers to replace the running binary. On this port that would silently overwrite the patched 10.13 binary with a stock Nightly build that no longer runs on 10.13 (the SDK guards, Metal shims, mozjemalloc getenv patches would all be gone). The scope is to disable AUS polling at the default-pref layer (`app.update.enabled`, `app.update.auto`, `app.update.background.enabled`, `app.update.service.enabled`), neutralise `app.update.url`, and remove the "Check for Updates…" entry from the Help menu and preferences UI. Keeping the updater binary compiled but unreachable minimises the patch surface against future upstream rebases. A GitHub-releases link in the Help menu is deferred until the repository actually exists.

2. **Enable the built-in AI features for Westmere.** `libmozinference.dylib`, built from `third_party/llama.cpp`, is `dlopen`'d by `MLSuggest`, PDF alt-text, Translation Models, Parakeet speech-to-text, and the local chat sidebar. Its `moz.build` unconditionally applies `-mavx -mavx2 -mbmi2 -mf16c -mfma -msse4.2` to every translation unit, so loading the dylib on Westmere would SIGILL on first use. Currently mitigated by leaving the AI features disabled in `about:config`. The follow-up is to replace the hardcoded instruction-set flags with a Westmere-safe baseline (ideally via a configure-time switch so newer silicon still gets AVX), rebuild `libmozinference.dylib`, verify it loads, then benchmark one representative workload to decide whether CPU inference without AVX2 is viable on the target hardware.

To ensure these items are not forgotten between sessions, `CLAUDE.md`'s `Docs` section was amended to instruct that `PORT_PLAN.md`'s "Planned follow-up work" section be consulted at the start of any session where the next task is not already specified.

## 2026-11-02 — Rebranding to Sierra Fox Beta

The port's user-facing identity was changed from Mozilla's "Nightly" to "Sierra Fox Beta". The target names surface in three distinct places on macOS: the `.app` bundle filename in Finder, the application name shown in the menu bar when the application is active, and the "About" dialog header. All three resolve from the same configure-time string (`MOZ_APP_DISPLAYNAME`) via `browser/app/moz.build` and the branding directory's `configure.sh`.

### Branding directory identification

The port's build was already consuming `browser/branding/unofficial/` (not `nightly/`), because unofficial builds — i.e. anything compiled without `--enable-official-branding` — default to that branding. This was confirmed by reading the resolved values from `obj-highsierra/config.status`: `MOZ_BRANDING_DIRECTORY = 'browser/branding/unofficial'`, `MOZ_APP_DISPLAYNAME = 'Nightly'`, `MOZ_MACBUNDLE_NAME = 'Nightly.app'`, `MOZ_MACBUNDLE_ID = 'org.mozilla.nightlyunofficial'`. Edits were therefore scoped to `browser/branding/unofficial/`.

### Patch inventory delta (branding)

Three files edited:

- `browser/branding/unofficial/configure.sh` — `MOZ_APP_DISPLAYNAME=Nightly` → `MOZ_APP_DISPLAYNAME="Sierra Fox Beta"`; `MOZ_MACBUNDLE_ID=nightlyunofficial` → `MOZ_MACBUNDLE_ID=sierrafoxbeta`. The bundle ID change causes macOS LaunchServices to treat the renamed application as a brand-new app rather than inheriting the Dock position, Launchpad slot, and per-bundle preferences of the previous `Nightly.app`. This was the intended behaviour: a renamed port deserves a clean LaunchServices entry.
- `browser/branding/unofficial/locales/en-US/brand.ftl` — Fluent brand identifiers updated: `-brand-shorter-name`, `-brand-short-name` → `Sierra Fox`; `-brand-shortcut-name`, `-brand-full-name` → `Sierra Fox Beta`; `-brand-product-name` → `Sierra Fox` (the version-independent form used in strings like "about Firefox" that should read "about Sierra Fox" instead). `-vendor-short-name` was left as `Mozilla`, which remains factually correct.
- `browser/branding/unofficial/locales/en-US/brand.properties` — the legacy string-bundle equivalents of the Fluent identifiers updated to match: `brandShorterName=Sierra Fox`, `brandShortName=Sierra Fox`, `brandFullName=Sierra Fox Beta`.

### Expected visible changes after rebuild

- Bundle: `obj-highsierra/dist/Nightly.app` → `obj-highsierra/dist/Sierra Fox Beta.app`.
- macOS menu bar and Dock tooltip: `Sierra Fox Beta`.
- About dialog: `Sierra Fox Beta` as the product name, with the version string (see below) rendered next to it.
- Window titles, preferences page headers, and other UI consumers of the brand identifiers: `Sierra Fox` wherever `Nightly` previously appeared.
- Bundle identifier: `org.mozilla.sierrafoxbeta`.

The project folder on disk (`161.0a1/`) was not renamed; it is only a convenience path and has no build significance.

## 2026-11-02 — Version string bumped to 161.0b1

In parallel with the display-name rebrand, the technical application version was moved from `161.0a1` to `161.0b1`. The motivation is signal rather than function: the port is no longer at nightly-alpha stability, and the version suffix is the standard carrier Mozilla uses to communicate channel stage.

Before making the change, Firefox's own add-on compatibility logic and version comparator were audited to confirm the change would not reject add-ons that previously installed.

### Add-on compatibility: what the version string actually drives

The compat check in `toolkit/mozapps/extensions/internal/XPIDatabase.sys.mjs`, method `isCompatibleWith` (approximately lines 560-598), defaults to a min-only check for extensions:

```
return Services.vc.compare(version, minVersion) >= 0;
```

`strict_max_version` is only consulted when the add-on sets `strictCompatibility`, which themes and language packs do automatically and ordinary WebExtensions do not. For add-ons that do take the strict path, the check becomes:

```
return (
  Services.vc.compare(version, minVersion) >= 0 &&
  Services.vc.compare(version, maxVersion) <= 0
);
```

The ordering `161.0a1 < 161.0b1 < 161.0` was then confirmed by reading `xpcom/base/nsVersionComparator.cpp`:

- `ParseVP` breaks `0a1` into `numA=0, strB="a", strBlen=1, numC=1`; `0b1` into `strB="b"`; a bare `0` leaves `strB=null`.
- `CompareVP` compares `numA` first (equal), then `strB` via `ns_strnncmp`.
- `ns_strnncmp` documents that "any string is before no string" and returns -1 when the first operand has a string and the second does not.
- `strcmp("a","b") = -1`, so `a1 < b1`; and `strcmp("a", null)` / `strcmp("b", null)` both return -1 (string before none), so `a1 < 0.0` and `b1 < 0.0`.

### Risk analysis

The only configuration under which switching from `a1` to `b1` could reject an add-on is: the add-on uses strict compatibility (theme / langpack / explicit opt-in) AND its `strict_max_version` is pinned to literally `161.0a1`. All of Firefox's own bundled themes and extensions were grepped for `strict_max_version` and only two files match: a browser-test fixture under `toolkit/mozapps/extensions/test/` and the schema definition itself. No shipping add-on bundled with the browser has a strict-max pin. AMO add-ons in the wild also avoid pinning to specific alpha strings because such a pin would make the add-on useless on any other version. The theoretical failure mode is therefore not expected to occur in practice.

### Patch inventory delta (version)

Three files edited, each a single-line replacement from `161.0a1` to `161.0b1`:

- `config/milestone.txt` — the primary source consumed by `build/moz.configure/init.configure`; propagates to `MOZ_APP_UA_VERSION`, `Services.appinfo.version`, and the User-Agent string.
- `browser/config/version.txt` — the browser-level version marker used by packaging.
- `browser/config/version_display.txt` — the version string shown in the About dialog, separable from `version.txt` so that cosmetic display can differ from the internal version if needed.

### Downstream effect on channel detection

With the `b1` suffix the browser self-identifies as being on the beta channel rather than nightly. The practical effects on Firefox internals are modest: certain developer-facing UI treatments change (nightly-specific nags are suppressed), the `extensions.update.background.url` endpoint changes, and Nimbus / Remote Settings would key off the beta channel instead of nightly. The last two are irrelevant on this port because the auto-updater is scheduled for removal (see the Planned follow-up work entry above) and Remote Settings polling will be disabled alongside it. Nothing in add-on compatibility, feature flag resolution, or the test corpus consumes the suffix in a way that would behave differently between `a1` and `b1`.

## 2026-11-02 — Sierra Fox application icon integrated into branding

The user supplied a 1254×1254 PNG rendered by an external image model, depicting a stylised fox foreground against a mountain / sunset lake background inside a rounded square. The source file was dropped into a project-root `icon/` directory and preserved unmodified; all transformations are produced by scripts that keep the master as the reference input.

### Source analysis

The source has no alpha channel: the area surrounding the rounded square is opaque black. Measuring the non-black bounding box using PIL (`Sierra Fox Emblem on Navy Gradient.png`): content box left=3, top=3, right=1249, bottom=1248 — i.e. the rounded square fills almost the entire canvas with a 3-5 px black bleed. The corner radius was measured by scanning the top-left quadrant: the leftmost non-black pixel's x-coordinate decreases monotonically row by row until it reaches the straight left edge, and the row at which this transition completes gives the vertical span of the curve. The measurement returned a radius of 271 px, which is 21.6 % of the icon width. Apple's modern squircle convention is approximately 22.37 % of the icon size, so the artwork is drawn to the correct shape; the small discrepancy is well within rendering tolerance.

### Tooling added

Two scripts were added under `icon/`:

- `icon/build_icns.py` — reads the source PNG, measures bounds and corner radius, builds an alpha mask that matches the measured rounded-square shape, writes the masked image to `icon/sierrafox_master.png` (1254×1254 RGBA), renders all ten iconset entries (16, 32, 128, 256, 512 and their @2x variants) into `icon/sierrafox.iconset/`, then packs them with the system `iconutil` into `icon/sierrafox.icns` (3053 KB).
- `icon/install_branding.py` — reads `sierrafox_master.png` and copies size-matched variants into `browser/branding/unofficial/`, replacing the stock Nightly assets listed below. Also copies `sierrafox.icns` to `browser/branding/unofficial/firefox.icns`.

Both scripts are deterministic and idempotent; re-running after any update to the source PNG regenerates everything downstream in two commands. Pillow is required and was installed into the `firefox-build` conda environment.

### Assets replaced in `browser/branding/unofficial/`

| Destination | Size | Role |
|---|---|---|
| `firefox.icns` | 3053 KB packed .icns | Finder, Dock, menu bar |
| `default16.png` through `default256.png` | 16, 22, 24, 32, 48, 64, 128, 256 px squares | In-UI consumers (tray, task switcher, misc. preferences) |
| `content/about-logo.png` | 192×192 | About dialog header |
| `content/about-logo@2x.png` | 384×384 | About dialog header, Retina |
| `content/about-logo-private.png` | 192×192 | Private-browsing About dialog |
| `content/about-logo-private@2x.png` | 384×384 | Private-browsing About dialog, Retina |
| `content/about.png` | 300×236 | Legacy about banner; the square icon is centered on a transparent canvas of the target banner aspect |

The private-browsing variants reuse the regular icon rather than carrying a visual differentiator. This is intentional for the port and documented as such in the install script.

### Assets intentionally not replaced

- `Assets.car` — compiled Asset Catalog, used by macOS for some NSImage lookups including dark-mode variants. Rebuilding requires `actool` and an `.xcassets` source and is deferred as a separate task.
- `content/about-logo.svg`, `content/about-wordmark.svg`, `content/firefox-wordmark.svg` — vector wordmarks and the SVG master of the about-logo. These continue to reference the stock Firefox marks and may surface the old wordmark text in some UI positions until individually redrawn.
- `document.icns`, `document.ico`, `content/document_pdf.svg` — file-type document icons shown by Finder when Sierra Fox is the default handler for HTML, PDF, and related types. These are a separate design task.
- `disk.icns`, `background.png` — DMG installer assets. The port does not currently produce a DMG (`./mach package` fails on 10.13's BSD `tar`), so these have no immediate user-visible effect.
- `VisualElements_*.png`, `PrivateBrowsing_*.png` — Windows Metro tiles; no effect on macOS.

### Next steps

A rebuild is required for the branding, version, and icon changes to materialise in the output bundle. Because these edits touch front-end files (branding assets, localisation strings, text version files) rather than compiled source, `./mach build faster` would normally suffice, but `firefox.icns` and the `default*.png` set are consumed at package-staging time and the bundle rename (`Nightly.app` → `Sierra Fox Beta.app`) is a configure-time decision, so a full `./mach build` is cleaner. The post-build desktop launcher scripts will need their bundle paths updated from `obj-highsierra/dist/Nightly.app` to `obj-highsierra/dist/Sierra Fox Beta.app`.

## 2026-11-02 — Compatibility-tier mozconfigs added

The current `mozconfig.highsierra` targets Westmere, which covers Mac Pro 5,1, the mid-2010 MacBook Pro 15"/17" (Arrandale), and every Mac from 2011 onwards (Sandy Bridge or newer). It excludes earlier Nehalem-generation Macs that lack AES-NI (Mac Pro 4,1, iMac late-2009 and mid-2010 Core i5/i7 Lynnfield variants) and all Core 2 Duo 10.13-capable Macs (Mac Pro 3,1, iMac late-2009 Core 2 Duo, MacBook Air 2010, MacBook Pro 13" 2010, Mac mini 2010).

Two sibling mozconfigs were added so broader-compatibility builds can be produced on demand without having to re-edit the Westmere config. Each targets a lower CPU floor and lives in its own objdir so builds do not stomp on each other:

- `mozconfig.highsierra.nehalem` — `-march=nehalem`, objdir `obj-highsierra-nehalem`. Drops the AES-NI and PCLMULQDQ requirements, retains SSE4.2. Covers Mac Pro 4,1 and the Nehalem iMacs in addition to everything the Westmere build covers.
- `mozconfig.highsierra.penryn` — no `-march=` override, which lets clang fall back to its built-in default of `-target-cpu=penryn`. Objdir `obj-highsierra-penryn`. Covers every officially-supported 10.13 Mac, including the Core 2 Duo variants and Mac Pro 3,1.

Each mozconfig is otherwise identical to the Westmere one: same SDK, same `MACOSX_DEPLOYMENT_TARGET`, same libc++-19 linker plumbing, same ThinLTO. Both compatibility variants intentionally omit `--enable-profile-use`: the `merged.profdata` captured during the Westmere PGO run was produced from an instrumented binary whose IR shape was influenced by Westmere codegen, and consuming it under a different `-march` produces many hash-mismatch warnings and reduced PGO benefit. Running the three-phase PGO workflow against each lower tier is possible but a separate exercise; the compatibility builds ship with ThinLTO only until that is done.

### Invocation

To build any variant, point `MOZCONFIG` at the corresponding file before `./mach build`. Nothing in the build system auto-selects; the operator chooses. Objdirs do not overlap so multiple builds can coexist on disk.

```
# Westmere (current primary)
export MOZCONFIG=$PWD/mozconfig.highsierra
./mach build

# Nehalem compatibility
export MOZCONFIG=$PWD/mozconfig.highsierra.nehalem
./mach build

# Penryn compatibility (every officially-supported 10.13 Mac)
export MOZCONFIG=$PWD/mozconfig.highsierra.penryn
./mach build
```

### Release-engineering note

If multiple variants are eventually published, each produces its own `Sierra Fox Beta.app` under its respective objdir and users select by hardware when downloading. The ISA tiers overlap in a stepped fashion: a Penryn build runs on all Macs the Nehalem build runs on, which in turn runs on all Macs the Westmere build runs on. There is no need for runtime CPU detection in the binary itself; the dispatcher is the user picking the correct download.

### Patch inventory delta

Build system:
- `161.0a1/mozconfig.highsierra.nehalem` — new file.
- `161.0a1/mozconfig.highsierra.penryn` — new file.

Source: no changes.
