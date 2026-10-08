# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Read AGENTS.md first

**`AGENTS.md` is the authoritative engineering guide for this repo** — architecture,
every hard-won gotcha, and the reasoning behind decisions that must not be re-litigated.
This file is a short orientation and command sheet; it deliberately does **not** repeat
AGENTS.md. Before changing anything non-trivial, read the relevant section there.

Documentation map:

| File | What it covers |
|---|---|
| `AGENTS.md` | Architecture, conventions, gotchas, why-decisions. **Start here.** |
| `README.md` | User-facing macOS docs (Uzbek) |
| `win/README.md` | Windows build, file layout, VM tooling, user docs (Uzbek) |
| `docs/windows/WINDOWS-PORT-PLAN.md` | Original porting decisions + bugs found in testing |
| `docs/windows/WINDOWS-PARITET.md` | Windows/macOS parity status, what the VM can and cannot verify |
| `CONTRIBUTING.md` | Build (also without signing keys), checks, code rules — for outside contributors |
| `CHANGELOG.md` | User-visible changes per version |
| `THIRD_PARTY_NOTICES.md` | Third-party software and model licenses (full texts: `scripts/litsenziyalar.sh`) |
| `SECURITY.md` | Vulnerability reporting, signing-key policy |
| `docs/audit/` | Open-source readiness audit of the current tree |
| `KEYINGI-ISH.md` | Internal repo only (not in the public `kotib` repo). Open work only. Finished items are deleted from it; durable knowledge moves into `AGENTS.md` |
| `statistika/README.md` | Telemetry/update Worker, privacy contract, release-time KV update |
| `docs/superpowers/` | Design specs and plans (e.g. the translation-quality measurements) |

## What this is

**Kotib** — offline Uzbek speech-to-text dictation, two *native* implementations of the
same product:

- **macOS** — Swift/AppKit in `src/*.swift` + a C shim over whisper.cpp (Metal).
- **Windows** — C++20/Win32 in `win/` (Vulkan), **cross-compiled from macOS** with llvm-mingw.

Same model (`rubaiSTT v2 medium`, ggml q8_0), same inference parameters, same design.
Three sections in both: **Yozish** (dictation), **Fayl** (file → transcript, optional LLM
actions), **Tarjima** (offline NLLB-200 3.3B translation, 202 languages).

Bundle id is `com.rubaistt.dictation` **forever** — macOS TCC grants hang off it.

## Language

All comments, log strings and user-facing text are **Uzbek**. Match that when editing.

Uzbek Latin has two distinct modifier letters: `ʻ` (U+02BB, for `oʻ`/`gʻ`) and `ʼ`
(U+02BC, glottal stop). User-facing text and transcript output **must** use them — this
is product output quality, not style. Never substitute ASCII `'`. In old comments the
codebase is inconsistent; leave that alone rather than running a mechanical sweep.

## Commands

### macOS

```bash
./setup.sh                     # one-time: deps → whisper.cpp/CTranslate2/SentencePiece → model → app
./src/build.sh                 # rebuild the app only → ~/Applications/Kotib.app
./src/test.sh                  # pure-logic Swift tests (~1377 checks, seconds) + version check
./scripts/versiya-tekshir.sh   # VERSION vs. everything that announces a version (--reliz before a release)
./scripts/release.sh           # Developer ID sign + notarize + dist/Kotib-<v>-mac.pkg
```

### Windows (built on macOS, run on Windows)

```bash
./win/build-mac.sh [x64|arm64|hammasi]   # cross-compile; --tez skips libs, --tarjimasiz skips CTranslate2
./win/tests/mac/hammasi.sh               # THE command after touching win/ — all macOS-side checks
./win/tests/mac/sinov.sh                 # unit tests only (~376 checks, ~1 s) — the writing loop
./win/tools/win.sh --joyla               # push the fresh build into the UTM VM over SSH
./win/tools/win.sh 'd: & testlar.cmd'    # run a command in the VM
python3 win/tools/vnc.py surat /tmp/a.png    # VM screenshot
./win/tools/ornatuvchi-yasa.sh --model <path>  # build the installer inside the VM (iscc is Windows-only)
```

`hammasi.sh` bundles five checks: the version check, unit tests, whisper-parameter parity
between the two platforms, the splitter/formatter corpus diff, and the translator-bridge
diff. The heavy ones skip themselves when their inputs are missing.

`build-mac.sh` reads two optional environment variables: `TOOLCHAIN` (llvm-mingw folder;
default `~/Developer/.toolchains/llvm-mingw-<LLVM_MINGW_VERSIYA>-ucrt-macos-universal`)
and `BREW_PREFIX` (Vulkan headers and shader compilers; default `brew --prefix`).

### Service and site

```bash
cd statistika && npm test                       # Worker tests (Miniflare; npm install once)
cd statistika && npx wrangler deploy            # telemetry + update Worker (stat.mirqobilov.com)
./scripts/reliz.sh kv-yoz manifest.json --foiz 10   # sign + verify + publish an update policy (1.2+)
./scripts/reliz.sh qaytar win 1.2.0                 # kill switch: back to a previous policy
./scripts/versiya-tekshir.sh --reliz            # ALWAYS before the two commands below
npx wrangler kv key put --namespace-id=8570442129a44d89ab486d90b4c471fd \
    joriy --path=statistika/versiya.json --remote    # announce a new release (no redeploy needed)
npx wrangler pages deploy web --project-name rubaistt-dictation   # download page
```

### Tests: no filtering

Neither runner takes a filter — both run everything in about a second, so there is no
`--only`. To add a macOS case: write `tests/test_*.swift` and register it in
`tests/test_ruyxat.swift`'s `barchaTestlarniRoyxatgaQosh()`. Windows: add the function to
the `testlar[]` table in `win/tests/main.cpp`. Only files importing **nothing but
Foundation** can go into `src/test.sh`'s `UNDER_TEST` array — keep new pure logic in such
a file so it stays testable.

There is no linter and no typechecker. Anything touching AppKit, AVFoundation,
whisper.cpp, Keychain, the filesystem or the network is verified by a successful build
plus manual testing (see AGENTS.md → *What tests cannot cover*).

## Rules that bite immediately

- **Never run an uncapped `cmake --build -j`.** Every build here is `nice -n 10 … -j4` on
  purpose: an uncapped `-j` froze a 16 GB machine for half an hour (CTranslate2 recompiles
  its sources once per CPU ISA, ~1–2 GB per job). Warn before starting heavy builds.
- **The two platforms' shared files must not drift.** `win/core/whisper_bridge.c` vs
  `src/whisper_bridge.c`, and `win/core/tarjima_bridge.cpp` vs `src/tarjima_bridge.cpp`,
  differ only by named Windows-only helpers — inference parameters, beam sizes, length
  limits are identical, and `win/tests/mac/parametr-tekshir.sh` enforces it.
- **Dev builds are signed with Developer ID, not ad-hoc.** TCC keys an ad-hoc signature to
  the cdhash, so every rebuild silently drops the Accessibility grant.
- **All user-data paths live in `src/yollar.swift`** (Windows: `core/util.cpp`). Never
  rebuild a path inline — a missed rename orphans user data, and the
  `Audio-Matnga` → `Kotib` migration runs from there.
- **The version number lives in one place: `VERSION`** (repo root). Everything that is
  *built* derives from it — `src/build.sh` (Info.plist), `scripts/make_pkg.sh`,
  `win/CMakeLists.txt` → `kotib_versiya.h` → `win/res/app.rc`, and `rubai.iss` via
  `iscc /DAppVersion=` from `ornatuvchi-yasa.sh`/`build.ps1`. Never type a version number
  into those files again. What *announces* a version — `statistika/versiya.json`,
  `web/index.html`, the README download links — points at CDN keys that cannot be derived
  (`dl/v1.1b/…`), so it is edited by hand and checked by `scripts/versiya-tekshir.sh`
  (runs inside `test.sh` and `hammasi.sh`): an announced version newer than `VERSION`
  always fails (the app would offer itself an update); an older one fails only with
  `--reliz`. Bump `VERSION`, `versiya.json`, the site and README links in one commit.
- **Third-party code is pinned** in `scripts/bogliqliklar.env` (whisper.cpp, CTranslate2,
  SentencePiece commits; llvm-mingw version; model URL/sha256/size). `setup.sh` and
  `win/build-mac.sh` clone exactly those commits and **stop** if an existing checkout is
  elsewhere; `win/build.ps1` reads the same file. To upgrade a dependency, change the pin
  there and rebuild its build folders — never `git pull` inside `whisper.cpp/`.
- **The update signing key is irreplaceable** (login Keychain, Sparkle item, account
  `kotib`; public half in `scripts/yangilanish-kaliti.pub`). Use `scripts/imzo-kaliti.sh`;
  never commit the private key; `sign_update` against the Keychain hangs on a GUI prompt —
  sign via an exported temp key file (see AGENTS.md → *Update signing key*).
- **Auto-update test cases live in `tests/umumiy/yangilanish_holatlari.def`** and are run by
  both platforms' tests. Add cases there, not in one test file.
- **CDN objects carry `Cache-Control: immutable`** — never overwrite an existing R2 key;
  Cloudflare would serve the stale copy for a year. New build → new prefix.
- `rubai_transcribe` mallocs its result (call `rubai_free_str`); `rubai_transcribe_segments`
  does not — `rubai_segment_text` points into the bridge's own segment store (valid until
  the next call or `rubai_unload`), must be copied immediately and must **never** be freed.
- **Both bridges chunk audio with Silero VAD (S12)** — `nutq_bolaklari.c` (byte-identical
  on both platforms) and the «Ovozni boʻlaklash» section (word-for-word identical; checked
  by `parametr-tekshir.sh`). The VAD model is pinned in `scripts/bogliqliklar.env` and
  ships next to the app. Don't "fix" hallucinations with `no_speech_thold`: on this model
  `no_speech_prob` is ~1e-10 everywhere (measured), see AGENTS.md → S12.
- Uploading anything over 300 MiB to R2 needs the throwaway-Worker multipart route —
  `wrangler r2 object put` refuses it and the OAuth token has no `r2` scope.
- `.cmd` files are stored CRLF and `installer/*.iss`/`*.isl` need a UTF-8 BOM
  (`.gitattributes`) — otherwise cmd.exe truncates lines and Inno mangles `ʻ`.

## Gitignored, never committed

`whisper.cpp/`, `ctranslate2/`, `sentencepiece/`, `sparkle/`, `*.bin`, `dist/`, `.model-cache/`,
`.venv/`, `.wrangler/`, `tests/.build/`, `win/build-*/` — all fetched or built.
