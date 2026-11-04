# Sierra Fox Beta

A modern web browser for Intel Macs that stock Firefox no longer supports.

Sierra Fox Beta is an unofficial port of [Firefox](https://www.mozilla.org/firefox/) rebuilt from upstream Mozilla source to run on **macOS 10.13 High Sierra**, targeting the Mac Pro 5,1 and other 2010–2012 era Intel Macs. It gives older hardware access to a current browser engine, with up-to-date JavaScript, modern CSS, current TLS and codec support, on machines Mozilla stopped supporting years ago.

This is a community port. Mozilla is not affiliated with or responsible for this project.

---

## Downloads

Pre-built `.dmg` bundles are published on the [Releases](../../releases) page.

Pick the build that matches your Mac's CPU. Picking the wrong one will cause the browser to crash on launch with an illegal-instruction error.

| Download | Supported CPU | Covered Macs |
|---|---|---|
| **Sierra Fox Beta — Westmere+** | SSE4.2 + AES-NI + PCLMULQDQ | Mac Pro 5,1 (2010 / 2012), MacBook Pro 15"/17" mid-2010, every Mac from 2011 onwards (Sandy Bridge and later) |
| **Sierra Fox Beta — Nehalem** *(coming)* | SSE4.2, no AES-NI | Adds Mac Pro 4,1 (2009), iMac late-2009 / mid-2010 Core i5/i7 |
| **Sierra Fox Beta — Penryn** *(coming)* | SSE4.1 (clang default) | Adds Mac mini 2010, MacBook Air 2010, MacBook Pro 13" 2010, iMac late-2009 Core 2 Duo, Mac Pro 3,1 |

**Not sure which build you need?** If your Mac is a Mac Pro 5,1 or anything from 2011 or later, use the Westmere build. If you're on 2009–2010 hardware with a Core i5/i7, use Nehalem. If you're on 2010-or-earlier hardware with a Core 2 Duo, use Penryn.

### Installation

1. Download the `.zip` for your CPU tier from the Releases page.
2. Extract the `.zip` and drag **Sierra Fox Beta** into `/Applications`.
3. Right-click → **Open**, on the first launch (macOS will warn about an unidentified developer; this is expected for a community build).

### System requirements

- macOS 10.13 High Sierra or newer
- One of the CPU tiers listed above
- 4 GB RAM minimum, 8 GB recommended
- A Metal-capable GPU (included on all officially 10.13-supported Macs)

---

## What makes this different from stock Firefox

- **Runs on macOS 10.13**, which upstream Firefox dropped support for years ago.
- **Patched for older hardware**: Metal 2.1+ API calls are shimmed and runtime-gated, dark-mode code paths fall back cleanly on 10.13's older appearance system, and the custom allocator's initialisation path avoids a libplatform deadlock specific to 10.13.
- **No automatic updates.** Stock Firefox's auto-updater would silently replace this patched binary with an upstream build that no longer runs on 10.13. The updater is disabled at build time; new versions are released manually here on GitHub.
- **No telemetry opt-in prompts beyond what upstream ships.** No additional data collection added by this port.
- **Rebranded as "Sierra Fox Beta"** to clearly distinguish it from official Firefox in your Applications folder, Dock, and About dialog.

## Known limitations

- **AI features disabled.** Firefox's built-in AI features (URL-bar suggestions, PDF alt-text, local chat, translation models, speech-to-text) depend on a library that requires AVX / AVX2 instructions. Westmere and earlier CPUs don't have these, so the features are left off by default and must stay off; enabling them in `about:config` will crash the browser. See [`PORT_PLAN.md`](PORT_PLAN.md) for the plan to address this.
- **Hardware video decode is limited.** H.264 playback (including YouTube) uses VideoToolbox and works smoothly. HEVC / VP9 / AV1 hardware decode is not available on this hardware generation; those codecs fall back to software decode.
- **WebRender may need to run in software mode** on older GPUs. If the browser renders incorrectly, set `gfx.webrender.software` to `true` in `about:config` and restart.
- **Experimental.** This is a beta port. Expect occasional crashes; please file them as issues.

---

## Reporting issues

Please open an [Issue](../../issues) with:

- Which build you downloaded (Westmere / Nehalem / Penryn)
- Your Mac model (About This Mac → Mac Pro 5,1 / iMac 11,3 / etc.)
- macOS version
- A clear description of what you were doing and what went wrong
- Any crash log from `~/Library/Logs/DiagnosticReports/` that mentions `firefox` or `Sierra Fox Beta`

---

## Building from source

Sierra Fox Beta is built from upstream `mozilla-central` with a small set of patches applied on top. If you want to reproduce or modify the build yourself, the orchestration and patch rationale live in three top-level docs:

- [`PORT_PLAN.md`](PORT_PLAN.md) — scope, patch categories, verification milestones, GPU compatibility, planned follow-up work
- [`DEVLOG.md`](DEVLOG.md) — chronological log of every change, with the diagnosis and reasoning behind each patch
- [`CLAUDE.md`](CLAUDE.md) — orchestration rules and build invocation

In short: clone a recent `mozilla-central`, copy the modified source files and the matching `mozconfig.highsierra*` into place, then `./mach build` with the environment described in `CLAUDE.md`. The three mozconfigs produce builds for the three CPU tiers.

---

## Credits and licence

All browser code is the work of **[Mozilla](https://www.mozilla.org/)** and the Firefox community. Sierra Fox Beta only adds the patches needed to compile and run on macOS 10.13 and older Intel hardware. The port is released under the same [Mozilla Public License 2.0](LICENSE) as upstream Firefox.

"Firefox" and the Firefox logo are trademarks of the Mozilla Foundation. The Sierra Fox Beta name, artwork, and branding are used to distinguish this unofficial port from official Firefox releases and are not endorsed by Mozilla.
