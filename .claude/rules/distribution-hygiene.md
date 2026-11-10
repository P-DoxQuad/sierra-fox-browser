---
paths:
  - "161.0a1/mozconfig*"
  - "dist/**"
  - "161.0a1/browser/installer/**"
---

# Distribution hygiene

Every distributed binary must be free of build-host paths. Six leak sources, each with a dedicated control. All controls mandatory for every build published via GitHub Releases.

## Leak sources + controls

| Leak source | Control | Grep check |
|---|---|---|
| Runtime dylib lookup (LC_RPATH) | `LDFLAGS=... -Wl,-rpath,@executable_path` — NEVER `-rpath,$HOME/...` | `otool -l <bin> \| grep -A2 LC_RPATH` — only `@executable_path` / `@loader_path` / project-relative |
| C/C++ debug info + `__FILE__` | `CFLAGS=... -ffile-prefix-map=$PWD=/sierra-fox-build -ffile-prefix-map=$HOME=/home` (same on CXXFLAGS) | `strings <bin> \| grep -E '/Users/\|/Volumes/\|/home/<name>'` — empty |
| Rust panic / `file!()` / `line!()` | `RUSTFLAGS=... --remap-path-prefix=$PWD=/sierra-fox-build --remap-path-prefix=$HOME=/home` | same strings check |
| Bundled libraries loaded from build host | `LDFLAGS` keeps `-L$HOME/.mozbuild/clang/lib` for link time; post-build copy libc++.1.dylib / libc++abi.1.dylib / libunwind.1.dylib into `Contents/MacOS/` so runtime `@rpath` resolves inside the bundle | `find <Contents/MacOS> -name 'libc++*'` — all three exist |
| Toolchain libc++ assertion strings | Toolchain-built dylibs don't inherit the mozconfig `-ffile-prefix-map`. Paths land in `__TEXT,__const` (assertion/demangle strings, not DWARF — `strip -S` can't touch them). Permanent fix: rebuild the toolchain with its own `-ffile-prefix-map=$PWD=/libcxx-src`. Interim: binary-patch the three dylibs with equal-length replacements preserving null terminators | `strings "$APP/Contents/MacOS/libc++*.dylib" \| grep -E '/Users/\|/Volumes/'` — empty |
| omni.ja generated content | `about:buildconfig` HTML and `modules/RFPTargetConstants.sys.mjs` embed configure strings including path literals from `-ffile-prefix-map` flag values. Fix: `unzip -q` both omni.ja files, `perl -pi -e 's{<host>}{/sierra-fox}g'`, `zip -u` patched files back in | `unzip -p "$APP/Contents/Resources/omni.ja" \| strings \| grep -E '/Users/\|/Volumes/'` — empty; same for `Resources/browser/omni.ja` |

`HOST_LDFLAGS` may keep absolute `-rpath,$HOME/.mozbuild/clang/lib`. Host tools (clang-plugin, bindgen) never ship.

## Packaging

Use `./mach package`. Never zip `obj-highsierra/dist/Sierra Fox Beta.app` directly — that is the dev-stage tree populated via `make install --symlink`, so every resource symlinks back into the source tree and objdir on the build host.

`./mach package` dereferences everything, filters via `browser/installer/package-manifest.in`, packs resources into `omni.ja`, and writes a clean `.app` to `obj-highsierra/dist/firefox/Sierra Fox Beta.app`. That is the tree that gets zipped.

```bash
cd 161.0a1
export MOZCONFIG="$PWD/mozconfig.highsierra"
./mach package                                    # ~2 min, no compilation

APP="../../obj-highsierra/dist/firefox/Sierra Fox Beta.app"
cp -L ~/.mozbuild/clang/lib/libc++.1.dylib    "$APP/Contents/MacOS/"
cp -L ~/.mozbuild/clang/lib/libc++abi.1.dylib "$APP/Contents/MacOS/"
cp -L ~/.mozbuild/clang/lib/libunwind.1.dylib "$APP/Contents/MacOS/"
# Post-package scrubs: binary-patch libc++ dylibs + sed inside the two omni.ja files.

ditto -c -k --sequesterRsrc --keepParent "$APP" \
  ../../sierra-fox-browser/dist/Sierra-Fox-Beta-Westmere-SandyBridge+.zip
```

## Release-time checklist

Run against the packaged bundle (`obj-highsierra/dist/firefox/`, not the dev-staged one) before any GitHub Release upload. All must pass.

```bash
APP="<path to obj-highsierra/dist/firefox/Sierra Fox Beta.app>"

# 1. No build-host paths in main binaries or bundled toolchain dylibs
strings "$APP/Contents/MacOS/XUL"           | grep -E '/Users/|/Volumes/|/home/' | head
strings "$APP/Contents/MacOS/libmozglue.dylib" | grep -E '/Users/|/Volumes/|/home/' | head
strings "$APP/Contents/MacOS/libc++.1.dylib"   | grep -E '/Users/|/Volumes/|/home/' | head
strings "$APP/Contents/MacOS/libc++abi.1.dylib"| grep -E '/Users/|/Volumes/|/home/' | head
strings "$APP/Contents/MacOS/libunwind.1.dylib"| grep -E '/Users/|/Volumes/|/home/' | head

# 2. No build-host paths inside omni.ja
unzip -p "$APP/Contents/Resources/omni.ja"         | strings | grep -E '/Users/|/Volumes/|/home/' | head
unzip -p "$APP/Contents/Resources/browser/omni.ja" | strings | grep -E '/Users/|/Volumes/|/home/' | head

# 3. No absolute rpaths in XUL
otool -l "$APP/Contents/MacOS/XUL" | grep -A2 LC_RPATH

# 4. libc++ triplet bundled
ls "$APP/Contents/MacOS/libc++.1.dylib" "$APP/Contents/MacOS/libc++abi.1.dylib" "$APP/Contents/MacOS/libunwind.1.dylib"

# 5. No symlinks
find "$APP" -type l | head

# 6. firefox --version runs standalone
"$APP/Contents/MacOS/firefox" --version
```

Any check fails → do not publish. Fix mozconfig, post-package copy, or omni.ja scrub and repeat.
