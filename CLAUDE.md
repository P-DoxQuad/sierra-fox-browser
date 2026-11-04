# Firefox-Legacy — macOS 10.13 port of Firefox 161.0a1

## Purpose

**This project is `sierra-fox-browser`: a public, source-available fork of Firefox for macOS 10.13, distributed as a working browser to end users.** It is NOT an orchestration layer, a docs project, or a patches sidecar. It is a browser port whose repository contains the full patched Firefox source tree together with build configuration, branding assets, build/release tooling, and release artifacts published via GitHub Releases. Users on Mac Pro 5,1 and 2010–2012 Intel Macs download a `.zip` / `.dmg` matching their CPU tier (Westmere / Nehalem / Penryn) and run it.

Every structural, tooling, repo-layout, licensing, versioning, and packaging decision must be evaluated against both parts of the mission: (a) the port is a real, maintainable fork with the source in the repo; and (b) end users can download and run it. Any proposal that strips source, hides patches, or leaves the port as docs-only fails criterion (a) and must be rejected regardless of how "clean" it looks.

**Reinforcement mechanism:** at the start of every session — and whenever the current task involves repo structure, GitHub, packaging, releases, or any question of "what goes in the repo" — reread this Purpose section before proposing anything. If the phrase "orchestration-only," "docs-only," "patches sidecar," or any variant drifts into a proposed plan, that is a signal the Purpose has been forgotten and must be reread immediately.

## Style

Defaults. Any user instruction overrides.

- Give the plan. No warnings about time, effort, or difficulty.
- One position per answer. Pivot explicitly if needed.
- Ask when hardware or environment matters. Do not guess.
- Match length to question. No preamble, no trailing summary.
- State decisions directly. Do not narrate deliberation.
- Deliver the requested artifact. Not meta-advice, not instructions on how to produce one.
- Omit unrequested caveats, warnings, and negatives.
- `PORT_PLAN.md` and `DEVLOG.md` are formal, descriptive prose.

## Docs

- `CLAUDE.md` — project purpose, operating rules, and build invocation. Not a state file. User-edited only unless explicitly instructed otherwise. Reread the Purpose section at the start of every session and before any repo / distribution / packaging work.
- `PORT_PLAN.md` — scope, patch categories, verification milestones, GPU compatibility. Upcoming work is listed under its "Planned follow-up work" section; consult it at the start of any session where the next task is not already specified.
- `DEVLOG.md` — chronological log. State, benchmarks, patch history.

Work done → append to DEVLOG. Rules change → user edits CLAUDE.md. Plan changes → edit PORT_PLAN. No cross-duplication.

## Build

```bash
cd <project-root>/161.0a1
source ~/miniforge3/etc/profile.d/conda.sh && conda activate firefox-build
export NODEJS="$(which node)"
export MOZCONFIG="$PWD/mozconfig.highsierra"
./mach build
```

Missing conda env or `NODEJS` → "could not find Node.js executable later than 12.22.12".

- Full build: `./mach build` (60–90 min).
- C/C++/Obj-C/Rust-only changes: `./mach build binaries` (skips front-end).
- Slow build output: redirect to a file in a scratch directory and read selectively. Never pipe through `tail`, `grep`, `head`; re-reading cannot require a rebuild.

Output: `obj-highsierra/dist/Nightly.app`.

Launch against isolated test profile:

```bash
open -n -a obj-highsierra/dist/Nightly.app --args --profile ~/.firefox-hs-profile --no-remote
```

## Code

- Comment minimum. Add comments only on non-trivial code, non-obvious function arguments, and class members with non-obvious semantics. Never narrate what the code does.
- Do not remove existing comments unless directly related to what is being changed.
- No emoji in code.
- Run `./mach format` on modified files before considering a patch done.
- Prefer `rg` over `grep` for text search. Narrow path scoping is required; blind recursion from the repo root is too slow.
- Mozilla's upstream `searchfox-cli` does not apply to this port: it searches indexed `mozilla-central`, not the locally-patched tree. Use `Grep`/`rg` on the local checkout instead.

## Rules

Hard. Do not break without an explicit toggle of the rule itself.

- No commits without explicit instruction.
- No destructive git or filesystem commands without confirmation.
- Repo work follows standard conventions without being asked. Any task touching version control, repository structure, release preparation, or distribution — `.gitignore`, `README.md`, `LICENSE`, repo layout, release tagging, artifact packaging — must proactively include every piece of hygiene that a professional repo would have: no personal paths, no build artefacts committed, no files above GitHub's 100 MB limit, user-facing README written for the audience the project ships to (end users, not developers), MPL-compliant attribution, correct licence file, release tag conventions, and so on. Do not ship a partial setup and then ask the user whether each piece was wanted; produce the complete, conventional artefact in one pass and surface only genuine decisions that require their input.

Defaults. User instruction overrides.

- `161.0a1/CLAUDE.md` belongs to Mozilla upstream. Do not modify.
- Preserve cwd. Stage symlink fallbacks before any rename, move, or unmount of an ancestor directory. Project volume has a space-stripped name with a compatibility symlink; preserve both.
- El Capitan (10.11) and Yosemite (10.10) ports are separate forks. Do not bake assumptions that obstruct them.
- Rebase quarterly against upstream `mozilla-central`.
- Preempt predictable failures in a single patch pass. Catalog known instances before building, not after reproducing each.
- After 2–3 failed attempts on the same error: stop and reconsider.

## Environment

- Bundled clang is LLVM 22.1.8. rustc 1.99.1 ships LLVM 23.1.1. Features requiring matching bitcode fail (cross-LTO, cross-language PGO). ThinLTO works. Bare `--enable-profile-generate` (no `=cross`) works — `moz_pgo_rust` in `build/moz.configure/lto-pgo.configure` only enables Rust PGO when `=cross` is set.
- Target CPU is Westmere Xeon. SSE4.2 ceiling, no AVX/AVX2/BMI2/FMA/F16C. Default clang target `penryn` is safe. `-march=westmere` unlocks AES-NI, PCLMULQDQ, SSE4.2 strings, POPCNT.
- `third_party/llama.cpp/moz.build` hardcodes `-mavx -mavx2 -mbmi2 -mf16c -mfma`. `libmozinference.dylib` is `dlopen`'d by Firefox AI features. Those features must stay disabled in `about:config` on Westmere.
- 10.13 SDK `__MAC_OS_X_VERSION_MAX_ALLOWED` is 101304, not 101300. SDK guards use `< 101400`.
- Metal 2.1+ APIs are shimmed and runtime-gated. Target hardware may lack argument buffers tier 2, non-uniform threadgroups, resource heaps. Fallback: `gfx.webrender.software = true`. HEVC/VP9/AV1 HW decode unavailable. H.264 works via VideoToolbox.
- `performSelector:` raises on missing selectors just like direct dispatch. Use `respondsToSelector:` for real behavioral gating.
- 10.13 `getenv()` takes an `os_unfair_lock`. PGO's `__llvm_profile_initialize` → `setenv` → `malloc` → mozjemalloc init → `getenv` deadlocks. Walk `environ` directly to bypass. Mozilla has `xgetenv` in `memory/build/Globals.cpp` for Windows; extended to POSIX by this port.
- PGO `.profraw` files land in the process `cwd` at launch, not the objdir. `profileserver.py` runs under `./mach python` whose cwd is `topsrcdir`, so profile data accumulates in `161.0a1/` as `default_<pid>_random_<hash>.profraw`. The script only runs `llvm-profdata merge` when `LLVM_PROFDATA` is in the invoking env (Taskcluster sets this; local runs do not), so merge manually with `~/.mozbuild/clang/bin/llvm-profdata merge -o obj-highsierra/merged.profdata *.profraw` and delete the raw files afterwards (~1 GB).
- Silent process death with no system crash log → uncaught Obj-C exception into `std::terminate`. Check `<profile>/crashes/events/<uuid>` for Breakpad event with exception message in `Notes` field. Event file is binary-contaminated after `FontName`; parse by key patterns, not strict JSON.
- `atos` on the 293 MB XUL hangs. Use `nm` + offset arithmetic over sorted symbol table.
