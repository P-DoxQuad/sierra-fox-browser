# sierra-fox-browser

Firefox 161.0a1 macOS 10.13 port. Directives for working on this repo. Scope, patch history, GPU compatibility, and release artefact table are in `PORT_PLAN.md`, `DEVLOG.md`, and `README.md`.

## Writing style for this file

- Directives, not narrative. No mission statements. No reinforcement loops. No journal entries.
- Short imperatives. One rule per bullet.
- Concrete and verifiable. Name the command, the file, or the flag.
- Multi-step reference procedures go in `.claude/rules/*.md` or as skills. Not here.

## Build

```bash
cd <repo-root>/161.0a1
source ~/miniforge3/etc/profile.d/conda.sh && conda activate firefox-build
export NODEJS="$(which node)"
export MOZCONFIG="$PWD/mozconfig.highsierra"
./mach build
```

- Full build: `./mach build` (60–90 min).
- C/C++/Obj-C/Rust only: `./mach build binaries` — skips front-end.
- Missing conda env or `NODEJS` → `could not find Node.js executable later than 12.22.12`.
- Dev-stage output at `obj-highsierra/dist/Sierra Fox Beta.app` is NOT distributable — see `.claude/rules/distribution-hygiene.md`.
- Redirect slow `./mach` output to a scratch file and read selectively. Never pipe through `tail`/`grep`/`head` — re-reading cannot require a rebuild.

Launch against an isolated test profile:

```bash
open -n -a "obj-highsierra/dist/Sierra Fox Beta.app" --args --profile ~/.firefox-hs-profile --no-remote
```

## Code

- Comments minimum. Only on non-trivial code, non-obvious arguments, non-obvious member semantics. Never narrate what the code does.
- Do not remove existing comments unrelated to the change.
- No emoji in code.
- Run `./mach format` on modified files before considering a patch done.
- Use `rg` with narrow path scoping. Blind recursion from the repo root is too slow.
- `searchfox-cli` searches indexed `mozilla-central`, not this port's patched tree. Use `Grep`/`rg` locally.
- `161.0a1/CLAUDE.md` is upstream Mozilla's. Do not modify.

## Workflow

- No commits without explicit per-commit instruction. Prior yes does not carry forward.
- No destructive git or filesystem commands without confirmation.
- Announce every long-running action before starting it — build, rebuild, clobber, rsync, push, release upload, mozconfig edit that invalidates cache, any `./mach` taking more than a minute, any filesystem rename. State what, why, and duration; wait for confirmation. "Doing X now" is not announcing.
- Preserve cwd. Stage symlink fallbacks before any rename / move / unmount of an ancestor directory.
- Commit messages name project changes (source, mozconfig, branding, patches). Docs (`README.md`, `DEVLOG.md`, `PORT_PLAN.md`, `CLAUDE.md`) ride along in the diff silently and are never narrated in the message.
- After 2–3 failed attempts on the same error: stop and reconsider the approach.
- Preempt predictable failures in a single patch pass. Catalog known instances before building, not after reproducing each.
- Repo work follows standard conventions without being asked: no personal paths, no build artefacts committed, no files above GitHub's 100 MB limit, user-facing README written for end users, MPL-compliant attribution.

## Release

- Every release build uses PGO (full three-phase workflow: instrumented build → `profileserver.py` → `llvm-profdata merge` → optimised rebuild) + ThinLTO + `-march=` for the target CPU tier + updater-disabled + all branding patches. Non-optional.
- No absolute build-host paths in anything distributed. Full catalogue of leak sources, controls, and the release-time checklist: `.claude/rules/distribution-hygiene.md`.
- The distributable bundle is always produced via `./mach package`, never by zipping the dev-staged `.app`. Workflow: `.claude/rules/distribution-hygiene.md`.

## Style — assistant output

- Give the plan. No warnings about time, effort, or difficulty.
- One position per answer. Pivot explicitly when needed.
- Match length to the question. No preamble, no trailing summary.
- State decisions directly. Do not narrate deliberation.
- Omit unrequested caveats, warnings, and negatives.
- Ask when hardware or environment matters. Do not guess.

## Environment

- Toolchain: bundled clang LLVM 22.1.8, rustc 1.99.1 (LLVM 23.1.1). Bitcode-matched features (cross-LTO, cross-language PGO) fail. ThinLTO works. Bare `--enable-profile-generate` works (no `=cross`).
- Target CPU: Westmere Xeon. SSE4.2 ceiling, no AVX/AVX2/BMI2/FMA/F16C. `-march=westmere` unlocks AES-NI, PCLMULQDQ, SSE4.2, POPCNT. Default clang target is `penryn`, safe.
- `third_party/llama.cpp/moz.build` hardcodes `-mavx -mavx2 -mbmi2 -mf16c -mfma`. `libmozinference.dylib` must stay unloaded on Westmere; keep Firefox AI features off in `about:config`.
- 10.13 SDK `__MAC_OS_X_VERSION_MAX_ALLOWED` is `101304`, not `101300`. SDK guards use `< 101400`.
- Metal 2.1+ APIs are shimmed and runtime-gated. Fallback: `gfx.webrender.software = true`. No HEVC / VP9 / AV1 HW decode on this hardware. H.264 works via VideoToolbox.
- `performSelector:` raises on missing selectors; use `respondsToSelector:` for behavioural gating.
- 10.13 `getenv()` takes `os_unfair_lock`. PGO init path (`__llvm_profile_initialize` → `setenv` → `malloc` → mozjemalloc init → `getenv`) deadlocks. Walk `environ` directly (`xgetenv` extended to POSIX by this port).
- PGO `.profraw` files land in process `cwd`, not the objdir. `profileserver.py` runs under `./mach python` with `cwd=topsrcdir`, so profiles accumulate in `161.0a1/`. Merge manually: `~/.mozbuild/clang/bin/llvm-profdata merge -o obj-highsierra/merged.profdata *.profraw`, then delete the raw files (~1 GB).
- Silent process death with no system crash log → uncaught Obj-C exception into `std::terminate`. Check `<profile>/crashes/events/<uuid>` for the Breakpad event (binary-contaminated after `FontName`; parse by key patterns, not strict JSON).
- `atos` hangs on the 293 MB XUL. Use `nm` + offset arithmetic over the sorted symbol table.
- Layout: `Firefox-Legacy/` (local-only parent) contains `sierra-fox-browser/` (the git repo), `obj-highsierra*/` (build output, gitignored, ~73 GB each), `toolchain-build/` (local libc++ 19 build, gitignored, ~2 GB). Mozconfigs set `MOZ_OBJDIR=@TOPSRCDIR@/../../obj-highsierra*` so the obj dir is outside the repo.
- El Capitan (10.11) and Yosemite (10.10) ports are separate forks, not stages. Do not bake assumptions that obstruct them.
- Rebase quarterly against upstream `mozilla-central`.
