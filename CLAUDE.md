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

Output: `obj-highsierra/dist/Sierra Fox Beta.app`.

Launch against isolated test profile:

```bash
open -n -a obj-highsierra/dist/Sierra Fox Beta.app --args --profile ~/.firefox-hs-profile --no-remote
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
- **No absolute paths in anything that gets distributed.** Build-host paths like `/Users/<name>/`, `/Volumes/<disk>/`, `/home/<name>/` must never be embedded in binaries, config files committed to the repo, or anything shipped via GitHub Releases. This is a hard distribution-hygiene rule because distributed binaries that leak build-host paths both break on download (the path doesn't exist on the user's Mac) and expose the maintainer's directory structure. The specific sites that produce path leaks and the controls for each are catalogued below under Environment → Distribution hygiene. Before any build that will be published, consult that section and verify every item in the "release-time checklist" subsection by running the listed commands against the staged bundle. A build that fails any checklist item is not shippable regardless of how urgent the schedule feels.
- **Every release build MUST use PGO and every other performance optimization already established for this port.** That means the full three-phase PGO workflow (instrumented build → `profileserver.py` run → `llvm-profdata merge` → optimized rebuild consuming the merged profile), on top of ThinLTO, on top of `-march=` for the target CPU tier, on top of the updater stub and all branding patches. These optimizations are mandatory, not optional. Shipping a release without PGO loses ~7-25 % across the benchmark suite and is not acceptable regardless of time pressure, failed intermediate builds, deleted `merged.profdata`, or any other local-state loss. If PGO data is missing, the correct response is to re-run the three-phase workflow — never to drop PGO and ship without it. Do not make the call to disable any established optimization unilaterally; if a situation seems to require it, surface the trade-off to the user and wait for explicit instruction before proceeding.
- **Announce before every process. No silent actions.** Before starting any long-running operation — a build, a rebuild, a clobber, an rsync, a push, a release upload, a mozconfig edit that will invalidate the build cache, any `./mach` invocation that takes more than a minute, any filesystem move/rename, any git operation — state plainly what is about to happen, why, and how long it will take, THEN wait for explicit user confirmation before executing. This applies even when the action seems like the obvious next step, even when the user just said "go", and even when a previous step completed successfully. "Doing X now" is not announcing — the user has to be able to say "no" before X starts, not after. Specifically flagged triggers: anything that changes `CFLAGS`/`CXXFLAGS`/`RUSTFLAGS`/`LDFLAGS` (invalidates cache → full rebuild), anything that runs `./mach clobber` or `./mach build-backend` (destroys/regenerates build state), anything that creates git commits, anything that changes the obj dir layout, anything that touches files outside the current task scope. Silent action costs the user time, money, and trust; the cost of a one-sentence heads-up is nothing.

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

### Project folder structure

The project folder is `Firefox-Legacy/`. It contains three kinds of thing, kept physically separate so the distributable unit is obvious:

```
Firefox-Legacy/                        ← project folder, LOCAL only, never pushed anywhere whole
├── sierra-fox-browser/                ← git repo, this is what gets committed and transferred
│   ├── .git/
│   ├── 161.0a1/                       ← patched Firefox source (upstream + port patches)
│   ├── icon/
│   ├── dist/                          ← built release artefacts (Sierra-Fox-Beta-*.zip), gitignored
│   ├── CLAUDE.md, PORT_PLAN.md, DEVLOG.md, README.md, LICENSE, .gitignore
├── obj-highsierra/                    ← Westmere build output (MOZ_OBJDIR), LOCAL, ~73 GB
├── obj-highsierra-nehalem/            ← Nehalem build output when built, LOCAL
├── obj-highsierra-penryn/             ← Penryn build output when built, LOCAL
└── toolchain-build/                   ← libc++19 source + local build, LOCAL, ~2 GB
```

The `sierra-fox-browser/` subdirectory IS the git repo and IS what you push to GitHub or transfer to another machine. Everything above its level (obj-highsierra*, toolchain-build) is local-only build state that stays on this Mac and is never committed, pushed, or copied anywhere.

The mozconfigs' `MOZ_OBJDIR` points at `@TOPSRCDIR@/../../obj-highsierra*` precisely so the obj dir lives outside the repo subdirectory, keeping the repo clean without requiring per-transfer exclusion lists.

### Distribution hygiene

Every distributed binary must be free of build-host paths. There are six sources of path leaks; each has a dedicated control, and the controls are mandatory for every build published via GitHub Releases:

| Leak source | Control | What to grep for after build |
|---|---|---|
| Runtime dylib lookup (LC_RPATH) | `LDFLAGS=... -Wl,-rpath,@executable_path` — NEVER `-rpath,$HOME/...` | `otool -l <binary> \| grep -A2 LC_RPATH` → must show only `@executable_path`, `@loader_path`, or project-relative `@executable_path/...` paths |
| C/C++ debug info and `__FILE__` macro | `CFLAGS=... -ffile-prefix-map=$PWD=/sierra-fox-build -ffile-prefix-map=$HOME=/home` (and same for `CXXFLAGS`) | `strings <binary> \| grep -E '/Users/\|/Volumes/\|/home/<name>'` → must be empty |
| Rust panic info / `file!()` / `line!()` | `RUSTFLAGS=... --remap-path-prefix=$PWD=/sierra-fox-build --remap-path-prefix=$HOME=/home` | same strings check as above |
| Bundled libraries loaded from build host | `LDFLAGS` keeps `-L$HOME/.mozbuild/clang/lib` for link-time symbol resolution, but post-build the actual dylibs (libc++.1.dylib, libc++abi.1.dylib, libunwind.1.dylib) must be copied into the `.app` Contents/MacOS/ so runtime lookup via `@executable_path` finds them without touching `$HOME` | `otool -L <binary>` + `find <Contents/MacOS> -name 'libc++*'` → required dylibs must be present inside the bundle |
| Toolchain-built libc++/libc++abi/libunwind assertion strings | The LLVM toolchain under `toolchain-build/` is a separate compile that does NOT inherit the Firefox mozconfig's `-ffile-prefix-map`. Source-file paths end up in `__TEXT,__const` as assertion / demangle message strings (not DWARF — `strip -S` cannot remove them). Permanent fix: rebuild the toolchain with its own `-ffile-prefix-map=$PWD=/libcxx-src`. Interim fix: binary-patch the three dylibs in `Contents/MacOS/` after copy, using equal-length replacement strings so the `__TEXT,__const` string pool layout and null terminators are preserved | `strings "$APP/Contents/MacOS/libc++.1.dylib" \| grep -E '/Users/\|/Volumes/'` and same for `libc++abi.1.dylib`, `libunwind.1.dylib` → must be empty |
| `omni.ja` generated content | `about:buildconfig` HTML (`chrome/toolkit/content/global/buildconfig.html` inside `Resources/omni.ja`) embeds the literal `CC`, `CXX`, `CFLAGS`, `CXXFLAGS`, `RUSTFLAGS` strings from configure, including any absolute paths that appear in them (`-ffile-prefix-map=/Volumes/.../Firefox-Legacy=/sierra-fox` is itself a literal string containing the build-host path). Generated `.sys.mjs` modules like `modules/RFPTargetConstants.sys.mjs` also carry informational comments with absolute paths to their generator inputs. Fix: `unzip -q` the two `omni.ja` files (`Resources/omni.ja` and `Resources/browser/omni.ja`), run `perl -pi -e 's{<host path>}{/sierra-fox}g'` across the extracted contents, and `zip -u` the patched files back into each archive. Do this after `./mach package` and before zipping the bundle | `unzip -p "$APP/Contents/Resources/omni.ja" \| strings \| grep -E '/Users/\|/Volumes/'` and same for `Resources/browser/omni.ja` → must be empty |

`HOST_LDFLAGS` can keep absolute `-rpath,$HOME/.mozbuild/clang/lib` because host tools (clang-plugin, bindgen, etc.) never ship — they run only during the build.

### Packaging the distributable bundle

**Always produce the distributable bundle via `./mach package`, never by zipping `obj-highsierra/dist/Sierra Fox Beta.app` directly.** The directly-staged `.app` under `obj-highsierra/dist/` is Mozilla's dev-stage tree, populated by `make install --symlink`: every resource (.sys.mjs module, Info.plist, application.ini, framework stub) is a symbolic link pointing back into the source tree and objdir on the build host. Zipping that tree produces an archive whose symlinks all point at `/Volumes/Data-RAID/Users/mdzura/...` and the resulting bundle is non-functional on any other machine.

`./mach package` runs the stage 2 packager: dereferences everything, applies `browser/installer/package-manifest.in` to exclude test binaries and dev-only tools, packs JS/XUL resources into `omni.ja` archives, and writes a clean distributable `.app` to `obj-highsierra/dist/firefox/Sierra Fox Beta.app`. This is the tree that gets zipped, after the post-package scrubs described above.

```bash
cd 161.0a1
export MOZCONFIG="$PWD/mozconfig.highsierra"
./mach package                                    # ~2 min, no compilation; produces obj-highsierra/dist/firefox/Sierra Fox Beta.app

APP="../../obj-highsierra/dist/firefox/Sierra Fox Beta.app"
cp -L ~/.mozbuild/clang/lib/libc++.1.dylib    "$APP/Contents/MacOS/"
cp -L ~/.mozbuild/clang/lib/libc++abi.1.dylib "$APP/Contents/MacOS/"
cp -L ~/.mozbuild/clang/lib/libunwind.1.dylib "$APP/Contents/MacOS/"
# post-package scrubs: binary-patch the libc++ dylibs and sed the two files inside omni.ja (see Distribution hygiene table)

ditto -c -k --sequesterRsrc --keepParent "$APP" ../../sierra-fox-browser/dist/Sierra-Fox-Beta-Westmere.zip
```

### Release-time checklist

Before uploading any zipped `.app` to a GitHub Release, run every check below against the packaged bundle (the one under `obj-highsierra/dist/firefox/`, not the dev-staged one). All must pass. No exceptions regardless of urgency.

```bash
APP="<path to obj-highsierra/dist/firefox/Sierra Fox Beta.app>"

# 1. No absolute paths in the main binaries and the bundled toolchain dylibs
strings "$APP/Contents/MacOS/XUL" | grep -E '/Users/|/Volumes/|/home/' | head
strings "$APP/Contents/MacOS/libmozglue.dylib" | grep -E '/Users/|/Volumes/|/home/' | head
strings "$APP/Contents/MacOS/libc++.1.dylib" | grep -E '/Users/|/Volumes/|/home/' | head
strings "$APP/Contents/MacOS/libc++abi.1.dylib" | grep -E '/Users/|/Volumes/|/home/' | head
strings "$APP/Contents/MacOS/libunwind.1.dylib" | grep -E '/Users/|/Volumes/|/home/' | head
# All must return nothing.

# 2. No absolute paths inside the packed omni.ja archives
unzip -p "$APP/Contents/Resources/omni.ja" | strings | grep -E '/Users/|/Volumes/|/home/' | head
unzip -p "$APP/Contents/Resources/browser/omni.ja" | strings | grep -E '/Users/|/Volumes/|/home/' | head
# Both must return nothing.

# 3. No absolute rpaths in XUL
otool -l "$APP/Contents/MacOS/XUL" | grep -A2 LC_RPATH
# Every "path" line must be @executable_path, @loader_path, or @executable_path/... — never an absolute path.

# 4. libc++ triplet is bundled inside the app
ls "$APP/Contents/MacOS/libc++.1.dylib" "$APP/Contents/MacOS/libc++abi.1.dylib" "$APP/Contents/MacOS/libunwind.1.dylib"
# All three must exist.

# 5. No symlinks in the bundle at all
find "$APP" -type l | head
# Must be empty. The release archive must contain materialised files, not dev-staged symlinks pointing back into the source tree or objdir.

# 6. `firefox --version` runs standalone
"$APP/Contents/MacOS/firefox" --version
# Must print "Mozilla Firefox <version>" and exit 0 without "Couldn't load XPCOM" or any dyld error.
```

If check 6 fails with "Couldn't load XPCOM" or a dyld error, the bundle is leaking paths or missing a bundled dependency — do not publish. Fix the mozconfig, the post-package copy step, or the omni.ja scrub step and repeat.
