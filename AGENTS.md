# AGENTS.md

Uzbek speech-to-text dictation app for **macOS and Windows**. Both products are named
**Kotib** as of 1.1.0 (was *Audio-Matnga*). The bundle id stays `com.rubaistt.dictation`
forever — TCC grants hang off it. Same model, same inference parameters, same design,
two native implementations.

- **macOS** — Swift app in `src/*.swift` (dictation core + Studio, see **Architecture**
  below) + C shim over whisper.cpp (Metal).
- **Windows** — C++20 / Win32 in `win/` (Vulkan). See **`win/README.md`** for its build,
  layout and conventions; **`docs/windows/WINDOWS-PORT-PLAN.md`** for the original porting decisions
  and the bugs found during testing; **`docs/windows/WINDOWS-PARITET.md`** for what is done, what
  the VM can and cannot verify, and how to drive the VM.

**The Windows build is cross-compiled from macOS.** `./win/build-mac.sh [x64|arm64|hammasi]`
builds whisper.cpp, CTranslate2, SentencePiece and the app with `llvm-mingw`. There is no
Windows machine in the loop for building — only for running. The MSVC path
(`win/build.ps1`) still works but is not the main road. Two load-bearing details:
`GGML_BACKEND_DL=ON` (a statically linked Vulkan backend makes the app refuse to start on
a machine with no Vulkan driver) and CTranslate2's **case-sensitive** architecture check,
which is why `win/cmake/toolchain-win-arm64-tarjima.cmake` exists. A third: whisper.cpp's
DLLs are built shared and import `libc++.dll`, `libunwind.dll` and `libomp.dll` from
llvm-mingw — without those three next to the exe the app does not start **anywhere**,
even though the app itself is `-static`.

**The installer is built inside the VM**: `iscc.exe` only runs on Windows.
`./win/tools/ornatuvchi-yasa.sh --model <path>` installs Inno Setup there if
needed, copies the sources over SSH and brings back **one** universal
`dist/Kotib-<v>-win-setup.exe`: it carries both the x64 and the ARM64 binaries
(`Check: IsArm64` picks at install time) plus the speech model, mirroring the
macOS `.pkg`. The translation model stays out — the app downloads it on demand.
The wizard is Uzbek (`win/installer/Uzbek.isl` — Inno ships no Uzbek
translation). Every run also produces `dist/Kotib-<v>-win-yangilash.exe` (`/DYangilash`,
no model, ~21 MB) — the auto-update package `reliz.sh win` signs.

**Per-user since 1.2 (S7).** `PrivilegesRequired=lowest`: the app lives in
`%LOCALAPPDATA%\Programs\Kotib`, the speech model in `%LOCALAPPDATA%\Kotib\models` (outside
`{app}`, so the update package swaps the app and never touches the model). No UAC — that
is what lets the auto-updater install silently. Verified in the VM (2026-10-08), as a
non-elevated user in the desktop session:
- **1.1.0 → 1.2 migration** (same AppId; 1.1.0's entry is in HKLM): the model is copied out
  of `C:\Program Files\Kotib\models` (the old uninstaller deletes it, so the copy comes
  first), the old Kotib is closed, the old uninstaller runs once via `ShellExec('runas')` —
  **the only UAC prompt** — the HKCU Run value is pointed at the new exe, Kotib reopens.
  All of it in `ssPostInstall`, after the new files are in place: if any step fails the user
  still has a working Kotib (declined UAC → an info box, two entries in «Apps»).
- While the HKLM entry exists Inno renames its own entry «Kotib X (Joriy foydalanuvchi)» to
  avoid a duplicate; after a successful removal the script rewrites `DisplayName`.
- **`/KOTIBYANGILASH`** (what `yangilovchi.cpp` passes): waits up to 30 s for the
  `Global\KotibSingleInstance` mutex to go (the app exits by itself), then force-closes;
  reopens Kotib on success **and** on failure (`DeinitializeSetup`). Before touching
  anything it opens every file in `{app}` for write+delete with no sharing: **Inno's
  rollback does not restore replaced files**, so a DLL locked halfway through left
  `ggml-base.dll`/`ggml-cpu*.dll` new and `ggml.dll` old (reproduced in the VM). Now a busy
  file aborts before the first write. Only in update mode — in the wizard this check would
  run before Inno's own «close the running app» offer and block it.
- **Uninstall (E8):** deletes the HKCU Run value if it points at this copy and the
  `yangilanish` folder; asks «Til modellari saqlab qolinsinmi?» *after* Inno's own
  confirmation. Phrased as «keep?» on purpose: `TaskDialogMsgBox` rejects `MB_DEFBUTTON2`
  («Invalid Buttons», a fatal runtime error mid-uninstall) and always defaults to the first
  button, so Enter keeps the ~4 GB. Silent uninstall keeps them.
- Cyrillic user name (`C:\Users\Аброр`, fresh profile, bundled setup): installs, the model
  loads (short-name path) and translation works; a Cyrillic folder **without** an 8.3 name
  goes through `rubai_load_w` (`_wfopen`) — log line «keng belgili ochish».

**Update end-to-end test stand (S8, 2026-10-08).** The updater only talks HTTPS to the two
hard-coded hosts and only trusts the key compiled in, so the stand fakes both without
touching repo code:
1. A test Ed25519 key (CryptoKit `Curve25519.Signing.PrivateKey`, raw 32-byte seed in
   base64 — `sign_update --ed-key-file` accepts it).
2. `git worktree add --detach` somewhere temporary; there `VERSION=1.2.0-sinov1` and the test
   public key in `scripts/yangilanish-kaliti.pub`, symlink the deps, `build-mac.sh hammasi
   --tez`, `ornatuvchi-yasa.sh`; repeat with `sinov2`. Never in the main tree: that build
   trusts the test key.
3. `reliz.sh imzola manifest.json -o javob.json --kalit-fayli <key> --ochiq <pub>`.
4. `win/tools/yangilanish_sinov_serveri.py` on the Mac (8443 — the sandbox refuses 443), with
   a throwaway CA + a cert for both names. In the VM: the CA into `Root` (`certutil
   -addstore`), both names → `127.0.0.2` in `hosts`, `netsh interface portproxy add v4tov4
   listenaddress=127.0.0.2 listenport=443 connectaddress=10.0.2.2 connectport=8443`. Undo
   all three afterwards (`certutil -delstore Root`, restore `hosts`, `portproxy delete`).
5. Per run: install `sinov1` in the desktop session, set `yangilanish.oxirgiMuvaffaqiyat=0`
   (else the 24-hour rule skips the check), start Kotib, read `dictation.log` + `server.log`.

Found with it: when the server closes the connection early, WinHTTP reports a clean end of
data (`WinHttpQueryDataAvailable` → 0 bytes), so `faylYukla` called the half file complete;
verification then failed on size and deleted it, and the next attempt started from zero.
It is shared by the speech- and translation-model downloads too. Now fewer bytes than
`Content-Length` is a dropped connection and the `.part` stays.

Inno traps met on the way: a `[Code]` line that **starts with `[`** (an array literal on a
continuation line) is parsed as a section header — «Invalid section tag»; `FileCopy` is
renamed `CopyFile` in 6.7; `ornatuvchi-yasa.sh` wipes `C:\kotib-orn` on every run, so keep
VM test scripts elsewhere. VM testing notes: run installers in the desktop session with
`schtasks /create … /rl LIMITED /it` (the SSH session is elevated — High integrity — and
would hide every UAC path); the UAC prompt is on the secure desktop, which VNC captures as
a black screen (Alt+Y answers it); `tasklist` truncates image names to 25 characters; a
silent uninstall run from SSH leaves `unins000.exe` behind, and the next install then
creates `unins001.*`. A second local user needs `SeBatchLogonRight` to run a scheduled
task, and `Start-Process -Credential` from SSH fails with 0xC0000142.

`win/core/whisper_bridge.c` is a near-verbatim copy of `src/whisper_bridge.c` — keep the
inference parameters (language `uz`, beam size 5, `no_speech_thold 0.25`) identical in both
so the two platforms produce the same output. **`./win/tests/mac/parametr-tekshir.sh`
enforces this**: it extracts every `p.<field> = …` line from both files and diffs them.
The files are *not* byte-for-byte identical — the Windows side additionally has
`rubai_load_ex` (CPU fallback) and `ggml_backend_load_all()` (DLL backends) — but the
parameters are, and that is what changes the output. Since 1.2 (S10/A5) **both** record an
error string (`set_err` → `rubai_last_error`), report the backend (`rubai_backend_name`;
note Metal registers as **`"MTL"`**, not `"Metal"` — the old comparison never matched and
macOS always said "CPU") and forward whisper/ggml messages to the app log
(`rubai_set_log`), dropping `GGML_LOG_LEVEL_DEBUG` — ggml writes one DEBUG line per
compiled pipeline, dozens to hundreds per model load. Keep those helpers identical too.
macOS additionally has
`rubai_transcribe_segments`, used by Studio to get per-segment timestamps; it shares
every inference parameter with `rubai_transcribe` except one deliberate difference,
`no_timestamps` (see S12 below). **Both platforms now have it** (`src/whisper_bridge.c` and
`win/core/whisper_bridge.c`) and it must stay identical too.

**S12 — Silero VAD chunking (1.2).** Measured, not guessed (`.stt-baho/s12`, FLEURS
10 × ~120 s + 50 short clips + 20 non-speech files, two runs each): this fine-tuned model
reports `no_speech_prob` ≈ 1e-10 on everything, so `no_speech_thold` never fires (it stays,
commented as inert) and silence/noise always came out as «musiqa»; and with
`no_timestamps=true` whisper advances exactly 30 s per window, so whatever the decoder
drops inside a window is gone — 14 of 96 sentences lost on long audio, WER 30 %. No
decoding parameter fixed either (whisper.cpp's built-in `p.vad` made it worse: it glues
speech back into one stream). The fix is in the bridge: both load `ggml-silero-v6.2.0.bin`
(pinned in `scripts/bogliqliklar.env`, 885 KB, shipped next to the app;
`rubai_set_vad_path` / Windows `rubai_set_vad_path_w`, set once — every model load reloads
VAD too), and `nutq_bolaklari.c` turns VAD segments into whisper calls: no speech → whisper
is not called (empty result); ≤ 30 s → the whole clip in one call (VAD is only a gate, so
short dictation is unchanged); longer → consecutive segments grouped up to 25 s, cut only at
VAD silences, one `whisper_full` each, texts joined with one space (the old code joined
segments with **no** separator and glued words at window joins). Studio uses the same
chunks with `no_timestamps = true`; a segment = one chunk with VAD times — whisper's own
timestamps on this model were degenerate (whole 0–30 s windows, a junk first word, results
that differed between runs). `temperature_inc` was 0 in S12; S23 raised it to **0.2**
(`docs/olchovlar/2026-10-08-takrorlanish-halqasi.md`): on every S12 set (long, long+noise,
short50, non-speech) the output is **byte-identical** to 0 — the fallback only fires on
broken audio, where it removed repetition loops («oʻzbekiston respublikasi» ×15) and cut WER
from 78 % to 67 %. The loops the fallback cannot break are collapsed in text by
`takrorniQisqartir` (inside `matnniTayyorla`, both platforms: a 1–6-word phrase repeated
≥ 4 times in a row → once; text without such a run is returned untouched). Caveat: whisper.cpp
seeds decoder 0's RNG only at state creation, so a fallback segment may depend on earlier
calls in the session. Without the VAD
file both bridges fall back to the 1.1 path. `parametr-tekshir.sh` now also diffs the `vp.`
lines and the whole «Ovozni boʻlaklash» section **word for word**, and `nutq_bolaklari.{c,h}`
byte for byte; the grouping is unit-tested in `win/tests/test_nutq.cpp`.
Windows confirmed in the VM (2026-10-08, `rubai-cli`, CPU under `GGML_BACKEND_DL`): the same
10 long files give WER 7.56 % vs macOS 7.83 % with the same normaliser
(`scripts/stt-baho/score.py`; 6 files differ by a few words — CPU vs Metal float
arithmetic), 0 of 10 non-speech files produce text.

The same rule applies to the translator: `src/tarjima_bridge.cpp` and
`win/core/tarjima_bridge.cpp` are the same file except for one Windows-only helper
(`yolniTayyorla`, which works around narrow-path encoding). Beam size, length limits and
the no-repeat n-gram must not drift.

No package manager on either side. Both platforms have a small pure-logic test runner —
see **Tests** below.

## Language

All comments, `NSLog` strings, and user-facing UI text are in **Uzbek**. Match that when editing.

**Apostrophe convention**: Uzbek Latin has two distinct modifier letters — `ʻ` (U+02BB,
MODIFIER LETTER TURNED COMMA, for `oʻ`/`gʻ`) and `ʼ` (U+02BC, MODIFIER LETTER APOSTROPHE,
for the glottal stop). **User-facing text and transcript output MUST use these proper
letters** — this is not a style nicety, it is the product's output quality
(`text_format.swift`'s `apostrofniBirxillashtir` is what normalizes whisper's mixed
output into them). **Every dictation result passes through `matnniTayyorla`**
(`text_format.swift` / `win/core/matn_format.cpp`) before it is logged, stored in history
or inserted — until 1.2 only Studio normalized, so dictation pasted ASCII `'` (515 of them
in one user's history, zero `ʻ`) and the "Oddiy apostrof" setting did nothing there. New
output rules go into that one function on both sides; the corpus diff
(`win/tests/mac/taqqoslash/`) compares both implementations' dictation output too. **Swift and C/C++ comments were swept once, from hand-reviewed
word lists (S20, S21):** every Uzbek word in a comment now uses `ʻ`/`ʼ`. The ASCII `'` that
remains in comments is deliberate — it separates a code or foreign name from an Uzbek
suffix (`Keychain'da`, `macOS'ning`, `log'ga`, `Chizgich'ini`) and is not a letter. Write
new comments the same way. Log strings may use either form. Never sweep with a bare regex: `tog'da` → `togʻda` is right, `log'ga` → `logʻga` is
wrong, and a pattern cannot tell them apart without a word list.

## Build & iterate

- `./setup.sh` — full one-time install (on Apple Silicon it also builds the **x86_64**
  whisper/CTranslate2/SentencePiece libs, so the app is universal; `src/build.sh` warns
  loudly when any of them is missing and falls back to arm64, and `scripts/make_pkg.sh`
  refuses to package anything `lipo -archs` doesn't report as arm64 + x86_64) (deps → whisper.cpp + CTranslate2 +
  SentencePiece static libs → model → app → login agent). **Every `cmake --build`
  here is capped at `nice -n 10 … -j4` on purpose**: an uncapped `-j` froze a
  16 GB machine for half an hour (CTranslate2 recompiles its sources once per CPU
  ISA, ~1–2 GB per job). Three flags are load-bearing and must not be dropped:
  `-DOPENMP_RUNTIME=NONE` (otherwise the app links `libomp.dylib`, which is absent
  without Homebrew), `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` (CTranslate2's vendored
  `clog` will not configure under CMake 4.x), and SentencePiece pinned to
  **v0.2.0** (`master` pulls external abseil-cpp — 94 extra static libs; v0.2.0
  vendors it and builds 2).
- **Every third-party source is pinned to a commit** in `scripts/bogliqliklar.env`,
  the one file read by `setup.sh` and `win/build-mac.sh` (through
  `scripts/bogliqliklar.sh`), `win/build.ps1` (`ConvertFrom-StringData`) and
  `win/cmake/llvm-mingw.cmake`. Until 1.2 whisper.cpp and CTranslate2 were cloned from
  `master` (a clean machine built different code than the one tested) and the MSVC path
  took a *different* whisper.cpp tag (v1.9.2) than the cross-compile path — two engines
  for one product. The pinned whisper.cpp is `167d225f`, the commit both 1.1.0 releases
  were actually built from. The helper clones a single commit (`--depth 1`, GitHub serves
  any SHA) into a temp folder and renames it only on success; an existing checkout at a
  different commit is a **hard stop** with the exact commands to fix it — the script does
  not move your checkout for you (it may hold experiments, and CTranslate2's rebuild takes
  25 minutes). The model download in `setup.sh` is verified against `MODEL_SHA256` before
  it gets the name the app looks for.
- **`VERSION` is the only place the version number is written.** `src/build.sh`,
  `scripts/make_pkg.sh`, `win/tools/zip-yasa.sh` source `scripts/versiya.sh`;
  `win/CMakeLists.txt` generates `kotib_versiya.h` for `win/res/app.rc`; the Inno script
  gets `/DAppVersion=` from `ornatuvchi-yasa.sh`/`build.ps1` and `#error`s without it.
  Format `X.Y.Z` with an optional `-suffix` for test channels; the Windows numeric
  version (what `joriyVersiya()` reads) takes only `X.Y.Z`. `win/res/app.manifest`'s
  `assemblyIdentity version` is deliberately frozen at `1.0.0.0` — it is the SxS identity,
  not the product version. **llvm-windres writes no depfile for `.rc` files**, so
  `res/app.rc` lists its generated header and the manifest in `OBJECT_DEPENDS`; without
  that, a changed `VERSION` regenerated the header but left the old number in the `.exe`
  (caught while testing this). `scripts/versiya-tekshir.sh` guards the rest — see
  `CLAUDE.md` → *Rules that bite immediately*.
  **Never put the repo root on a C/C++ include path (`-I.`, `-I$ROOT`).** APFS and NTFS
  are case-insensitive, so `#include <version>` (pulled in by libc++'s own headers)
  resolves to the root `VERSION` file and the build dies deep inside `<limits>` with
  "expected unqualified-id". No current build does this; use `-iquote` if a tool needs
  `#include "win/..."`.
- `src/build.sh` — rebuild only the app. Requires `whisper.cpp/build-static/` (created by `setup.sh`). Output: `~/Applications/Kotib.app` (override with `APP_OUT=`).
- The model is **never bundled into the `.app`** anymore (1.2, S4; `SKIP_MODEL` is obsolete).
  The app looks for it outside the bundle (`model_tanlov.swift`, every candidate checked
  for existence **and exact size**, next candidate tried if `rubai_load` fails): the user
  folder (in-app download), `/Library/Application Support/Kotib/models` (.pkg), the old
  1.1.0 bundle location (transition only), `~/rubai-stt/models` (setup.sh). The 1.1.0
  lookup returned the bundle path without checking the file, and CFBundle's cache kept
  returning it after a `SKIP_MODEL=1` rebuild — "Model yuklanmadi (kod 1)" 12 times in one
  user's log, each one losing a recording (spec A1). Known models (name, size, sha256)
  live in `ModelTanlov.malum`; a test pins size and sha256 to `bogliqliklar.env`.
- Swift sources are split across `src/*.swift`; top-level launch code must stay in `src/main.swift` (Swift only allows it there once a module has several files).
- After `src/build.sh`, relaunch the app to test (login autostart runs through the LaunchAgent bundled at `Contents/Library/LaunchAgents/`).
- `scripts/make_pkg.sh` — Developer ID sign + notarize + `.pkg` installer at `dist/Kotib-<VERSION>-mac.pkg`. Since 1.2 (S4) it has **two components**: the app (`/Applications`, ~15 MB) and the model (`com.rubaistt.dictation.pkg.model` → `/Library/Application Support/Kotib/models`, sha256-checked against `bogliqliklar.env` before packaging). One installer still works fully offline. `preinstall` removes `/Applications/Kotib.app` first (a 1.1.0 bundle carries the 800 MB model, which must not linger inside the new signed bundle); `postinstall` chowns the bundle to the console user so the auto-updater can replace it without a password (spec D2). `scripts/release.sh` forwards to it. Note: `lsbom` of a pkg built from a Claude Code session lists `._name` entries — that is how pkgbuild stores the undeletable `com.apple.provenance` xattr; `pkgutil --expand-full` (what Installer does) turns them back into xattrs, no `._` files land on disk and the bundle signature verifies. Not a bug.
- **Notarization is not optional**: macOS 15+ blocks unnotarized Developer ID apps outright, and "right-click → Open" no longer bypasses it. Requires a `notarytool` keychain profile (`rubai-notary`).

No linter or typechecker. `src/test.sh` is a small pure-logic test runner (see **Tests**
below) — it does not replace manual testing of anything that touches AppKit,
AVFoundation, whisper.cpp, the Keychain, or the filesystem.

## Tests

`src/test.sh` compiles the project's pure-Foundation files (currently `text_format.swift`,
`audio_util.swift`, `llm_client.swift`, `diktovka_tarixi.swift`, `yollar.swift`,
`vaqt_format.swift`, `matn_boluvchi.swift`, `tillar.swift`, `tarjima_model.swift`,
`yangilanish_siyosat.swift`, `imzo.swift` — CryptoKit is allowed there because it is
pure computation — `model_tanlov.swift`, `log_siyosati.swift` and `saqlanmagan.swift`) together with
`tests/*.swift` using `swiftc -O` and runs the resulting binary. No XCTest, no package
manager — just a script and plain top-level test functions called from `tests/main.swift`.
It currently reports **1494 checks** covering apostrophe normalization, sentence
capitalization, paragraph splitting, `jimlikNuqtasi`/`kuchaytir`, the two SSE parsers, LLM
chunking, the dictation history store (`diktovka_tarixi.swift`), the
Audio-Matnga → Kotib data-folder migration (`yollar.swift`), time formatting
(`vaqt_format.swift`), the provider model-list parser and the per-provider request
body (`modellarniAjrat` / `soravTanasi` in `llm_client.swift`), translation text
splitting (`matn_boluvchi.swift`), the 202-language table (`tillar.swift` — the
count is high because every language is checked individually) and the translation
model's folder-completeness check (`tarjima_model.swift`), and the auto-update logic
(version order, policy decision, URL allow-list, base64, Ed25519, signed manifest, the
check/retry/idle-install schedule and release-note shortening),
and the unsaved-audio store (`saqlanmagan.swift` — WAV round trip, naming, the 20-file /
7-day rule).

**Shared test table.** The auto-update cases live in **one** file,
`tests/umumiy/yangilanish_holatlari.def`, one macro call per line. The Windows test
`#include`s it as an X-macro (so it is compiled into `kotib-testlar.exe` — the VM needs
no extra file); the Swift test parses the same file at run time via `#filePath`. Both
platforms must reach the same decision on the same server reply, and two hand-kept copies
of the cases would drift silently — this is the same reason `parametr-tekshir.sh` exists.
Add a case there, never in one test file. The Swift parser understands the same C string
escapes the compiler does (`\"`, `\\`, `\n`, `\r`, `\t`, `\uXXXX`), so a release-note case
can carry a real newline or NBSP. `qisqaIzoh` (release note → one banner line) counts
**Unicode scalars, not graphemes**, on both sides — Swift's `String.count` and a C++ code
point loop would disagree on emoji and combining marks. The Ed25519 rows are the RFC 8032 §7.1 vectors
taken from the RFC text; malleability/small-order edge cases are deliberately absent
because CryptoKit and Monocypher may legitimately differ on them and Sparkle never
produces them.

After the Swift checks `test.sh` runs `scripts/versiya-tekshir.sh` (see the `VERSION`
bullet under **Build & iterate**), so a forgotten version number fails the same command.

### Windows tests

**`./win/tests/mac/hammasi.sh` runs every macOS-side check in one go** — the version
check (`scripts/versiya-tekshir.sh`), unit tests, whisper parameter parity, the
splitter/formatter corpus diff and the translator bridge diff. That is the command to
run after touching anything under `win/`. The heavy checks skip themselves when their
inputs (the translation model, CTranslate2) are missing.

Two unit-test runners, and you will normally use the first:

- **`./win/tests/mac/sinov.sh`** — compiles the pure-logic Windows sources with clang
  **on macOS** and runs them in about a second. Currently **506 checks** over
  `matn_format`, `vaqt_format`, `matn_boluvchi`, `tillar`, `json`, `llm_sof`,
  `versiya`, `yangilanish_siyosat`, `imzo`, `saqlanmagan` (the unsaved-audio store;
  it uses `std::filesystem`, not Win32, so even its file I/O runs here) and `sozlama`
  (`sozlamaOynasidan` — the settings window writes only its own 13 fields, and the
  updater's state in `settings.ini` must survive it — a reset `majburiyKorilgan` would
  restart the mandatory grace period; it used to
  write back a stale copy of the whole file and erase the install ID the stats thread had
  just created) and `nutq` (S12 VAD chunk grouping — `nutq_bolaklari.c`, which macOS uses
  byte for byte) (Monocypher and `nutq_bolaklari.c` are compiled as C, separately,
  because the `windows.h` stand-in is force-included only into C++). It works by force-including `win/tests/mac/windows.h`, a ~60-line stand-in
  for the handful of Win32 calls those files make, plus `util_shim.cpp` for the text
  encoders. This is the loop to use while writing code.
- **`kotib-testlar.exe`** — the same tests plus the ones that need Windows itself
  (**244 checks** before 1.2's S2; the S2 cases cross-compile cleanly but have not been run
  in the VM yet), built by `win/build-mac.sh` and run on Windows or in the UTM VM.
  It covers the two things the macOS pipe cannot see: ICU sentence boundaries and
  UTF-16 surrogate pairs (`wchar_t` is 32-bit on macOS, 16-bit on Windows). Tests
  that only make sense there are guarded with `#ifdef _WIN32`.

### Driving the Windows VM

The VM is reached over **SSH** and its screen over **VNC** — no disk swapping, no
Full Disk Access, no unlocked Mac needed:

```bash
./win/tools/win.sh --joyla              # copy the fresh build into the VM (20 s)
./win/tools/win.sh 'd: & testlar.cmd'   # run a command
python3 win/tools/vnc.py surat /tmp/a.png   # screenshot
python3 win/tools/vnc.py klik 400 300       # click (goes through UTM, absolute)
python3 win/tools/vnc.py sudra 100 200 500 300  # drag (goes through VNC)
./win/tools/ornatuvchi-yasa.sh --model <path>   # build the installer in the VM
```

Three things make that work, and each replaced a dead end:

- **virtio ARM64 drivers exist** in `virtio-win.iso` under `*/w11/ARM64/` — NetKVM
  gives the guest a network, which gives it OpenSSH, which removes the whole
  swap-the-test-disk dance. `viogpudo` also raises the screen from 800×600 to
  1280×800, which is what makes the UI inspectable at all.
- **QEMU's own VNC server**, enabled through UTM's `AdditionalArguments`
  (`["-vnc", "127.0.0.1:1"]`, plain strings — a dict makes UTM reject the whole
  config). UTM's `input scan code` never sends extended (0xE0) codes, so Win and
  the arrow keys are unreachable through it, and `screencapture` returns nothing
  while the Mac is locked.
- **Mouse goes through two paths.** Clicks use UTM's `input mouse click` (absolute,
  reliable). Drags have to use VNC because UTM has no button-hold; VNC coordinates
  arrive as *relative* deltas (both usb-tablet and usb-mouse are attached), so the
  cursor is first pinned into the corner and pointer acceleration must be off in the
  guest (`win/tools/vm-sichqoncha.ps1`).

**The VM has a microphone.** UTM's sound device gives the guest a «Line In» capture
endpoint fed by the Mac's default input. Playing a FLEURS clip through the Mac speaker
(`afplay -v 1 <wav>`) while Kotib records in the VM is a working — if very lossy, peak
≈ 0.02 — dictation test: start/stop with `vnc.py tugma ctrl+alt+d`, read the result from
the log or `%APPDATA%\Kotib\diktovka-tarixi.json`. `rubai-cli --record <s> --saqla <wav>`
works from SSH and keeps the recording for measuring on the Mac. A WAV dropped into
`%LOCALAPPDATA%\Kotib\saqlanmagan\` under a `2026-10-08_03-40-00-000.wav`-style name
(PCM16 mono 16 kHz, what `--saqla` writes) goes through the app's own retry path — the
same `matnniTayyorla` as a dictation. The clipboard is per session: read it by pasting in
the desktop, not with `Get-Clipboard` over SSH.

If the pointer stops responding entirely (keyboard still works, guest devices
still show as *Started*), the fault is QEMU's input state: **rebooting the guest
does not fix it — the VM has to be stopped and started**, so that a new QEMU
process comes up.

`.cmd` files are stored **CRLF** (`.gitattributes`): with LF only, cmd.exe cuts the
script mid-line (`jfk.wav` becomes `k.wav`). `installer/rubai.iss` and `Uzbek.isl`
need a **UTF-8 BOM** — without it Inno reads them in the system ANSI code page and
`ʻ` degrades to a plain `'`. When editing any `eol=crlf` file (`.iss`, `.rc`, `.ps1`,
`.cmd`, `.manifest`) with a script, preserve the endings — Python's `read_text`/
`write_text` silently turn CRLF into LF (use `newline=""`). Git still commits it
normalized, so nothing shows up in the diff; only the working copy that
`ornatuvchi-yasa.sh` ships to the VM is wrong. `git ls-files --eol | grep crlf` shows it.

**`./win/tests/mac/taqqoslash/taqqosla.sh`** runs the two implementations of the two
things users actually see side by side and diffs the output:

- the **sentence splitter** (`matn_boluvchi.swift` vs `matn_boluvchi.cpp`) over a corpus
  of real Uzbek/Russian/Chinese/Arabic lines (`korpus.txt`);
- the **text formatter** (`text_format.swift` vs `matn_format.cpp`) over a corpus of
  whisper-shaped segments with timings (`segmentlar.txt`) — paragraph splitting,
  apostrophe normalization, sentence capitalization, both apostrophe styles.

On macOS the C++ splitter falls back to its own rule (Windows ICU's export names are not
the ones macOS uses), so that half of the comparison measures the **worst case**: with
ICU present Windows can only do better. Add a line to the corpus files to add a case.

**`./win/tests/mac/tarjima-taqqosla.sh`** compiles *both* translator bridges
(`src/tarjima_bridge.cpp` and `win/core/tarjima_bridge.cpp`) on macOS against the same
CTranslate2 build and the same model, translates the same sentences with each and diffs.
They are the same file apart from one Windows-only path helper, and the failure mode if
they drift is invisible — the output still looks like a translation, just worse. Needs
`./setup.sh` and a downloaded translation model. `./win/tests/mac/tarjima-etalon.sh`
prints the macOS translation of one sentence, for comparing against
`rubai-cli.exe --tarjima` in the VM.

**Keep pure logic in files that the macOS pipe can compile.** That is why `llm.cpp` is
split into `llm_sof.cpp` (providers, request body, SSE, chunking — no Win32) and
`llm.cpp` (Credential Manager, WinHTTP), and why version comparison and every update
decision live in `versiya.cpp`/`yangilanish_siyosat.cpp` rather than inside
`yangilovchi.cpp`. macOS draws the same line:
`llm_client.swift` is testable, `llm_providers.swift` is not.

A check that fails is printed with its name; a check that *crashes* prints the name of
the last check that started (`std::set_terminate` in `win/tests/main.cpp`) — added
because the first macOS run died with a bare `Abort trap` and bisecting was slow.

### What tests cannot cover

What they deliberately cannot cover on macOS: anything that imports AppKit, AVFoundation, whisper.cpp,
Security (Keychain), or touches the filesystem/network — i.e. `ilova.swift`,
`ilova_saqlanmagan.swift`, `whisper.swift`, `yozuvchi.swift`, `overlay.swift`,
`kirituvchi.swift`, `hotkey.swift`, `avtostart.swift`, `log.swift`, `statistika.swift`,
`yangilovchi.swift`, `yangilash_haydovchi.swift`,
`media_decode.swift`, `hujjat.swift`, `transcribe_job.swift`, `asosiy_oyna.swift`,
`asosiy_oyna_qismlar.swift`, `diktovka_view.swift`, `yozish_kartasi.swift`,
`studiya_view.swift`, `studiya_amallar.swift`, `studiya_vidjetlar.swift`,
`sozlamalar_view.swift`, `sozlamalar_qoshimcha.swift`, `sozlamalar_llm.swift`, `kotib_uslub.swift`,
`llm_providers.swift`, `settings.swift`, `audio_devices.swift`, `model_download.swift`,
`tarjimon.swift`, `tarjima_view.swift`, `tarjima_yuklovchi.swift`, `clipboard.swift`.
Those are verified by a successful build (`src/build.sh`) plus manual testing. Keep new
pure-logic code (no AppKit/AVFoundation/whisper/Keychain/filesystem imports) in a file
`test.sh`'s `UNDER_TEST` array can pick up, and add cases to `tests/`, so it stays
testable this way.

## Key conventions & gotchas

- **All user-data paths live in `src/yollar.swift`** — the app support folder, the
  dictation-history file, the Studio document root, the downloaded model and the log
  (`~/Library/Logs/Kotib.log`). It also performs the one-time
  `Application Support/Audio-Matnga` → `Kotib` move on first access, so a 1.0 user keeps
  their history and documents after the rename. Never rebuild these paths inline again:
  before `yollar.swift` the folder name was spelled out in three separate files, and a
  rename that missed one would silently orphan user data.
- **Model quantization is measured, not guessed** — `scripts/stt-baho/` runs every
  variant through the app's own code path on FLEURS uz (345 sentences with human
  transcripts). 2026-10-07: q4 rejected (+0.36…+0.48 pp WER, meaning-changing errors, not
  faster); q5_k equal quality, 284 MB smaller, but 15–30 % slower on ARM CPU and x86 is
  unmeasured — q8_0 stays until a real x86 Windows run decides
  (`docs/olchovlar/2026-10-07-model-kvantlash.md`). Both platforms must ship the same model.
- **Model**: `ggml-rubaistt.bin` (q8_0, 823 369 796 bytes). Lookup order and checks: see the S4 bullet under **Build & iterate** (`model_tanlov.swift`).
- **Info.plist**, the bundled **LaunchAgent** plist and **codesign** are all generated inside `src/build.sh` — edit them there, not as separate files. `scripts/release.sh` re-signs with Developer ID + hardened runtime.
- **Dev builds are signed with Developer ID, not ad-hoc — this is not optional.** TCC binds the Accessibility grant to the code identity; an ad-hoc signature has no Team ID and no stable designated requirement, so tccd keys the grant to the binary's **cdhash**. Every rebuild changes the cdhash, the old grant stops matching, and the toggle in System Settings stays ON while `AXIsProcessTrusted()` returns false — the app re-prompts and dead rows pile up in the permission list. `build.sh` picks the `Developer ID Application` identity automatically (override with `CODESIGN_IDENTITY=`) and falls back to ad-hoc with a warning when no cert is present. `SMAppService.agent` also refuses to register an ad-hoc-signed bundle.
- **Entitlements** (`src/entitlements.plist`): only `audio-input` + `allow-jit`.
- **whisper_context is a process-global C singleton** (`whisper_bridge.c:6`), shared by
  dictation and Studio — there is only ever one loaded model. `rubai_transcribe` mallocs
  the result string — the Swift caller **must** call `rubai_free_str` on it.
  `rubai_transcribe_segments` is different: it returns an `int` status code and mallocs
  nothing itself; per-segment text comes from `rubai_segment_text`, which returns a
  pointer **into the bridge's own segment store** (since S12; it lives until the next
  `rubai_transcribe_segments` or `rubai_unload`) — the caller must copy it immediately (as
  `whisper.swift`'s `transcribeSegments` does with `String(cString:)`) and must **never**
  call `rubai_free_str` (or `free`) on it. Access is serialized by `TranskripsiyaIshi.ishlayapti` (a single
  in-flight job at a time — dictation checks the same flag before starting) and by
  `Whisper.shared.band`. `band` is *owned* by `TranskripsiyaIshi` for the duration of a
  Studio job and must stay `true` across the whole job, including the decode gaps between
  individual whisper calls when a file is chunked — not just during the whisper calls
  themselves. While `band` is true, the idle-unload timer re-arms itself instead of
  freeing the model (see `AppDelegate.scheduleIdleUnload`/`studiyaIshTugadi` in
  `ilova.swift`); if a caller sets `band = true` and doesn't reliably clear it, the model
  never gets unloaded. The reverse lock is `DiktovkaBand.faol` (mic open or a dictation
  waiting for whisper): Studio refuses to start a file while it is true, because the
  whisper queue is serial and the dictation would otherwise wait minutes behind the file
  and paste into whatever window the user had moved on to.
- **Recorder** creates a **new `AVAudioEngine`** on every `start()` to avoid a stuck-state bug after login/device change.
  Opening the mic runs on a **background queue with a 3s timeout** (`Recorder.ochishTimeout`), never on the main thread:
  `AVAudioEngine.inputNode` makes a *synchronous* HAL call into `coreaudiod` (`GetHWFormat` → `GetSubDevices`) that can
  wedge — on 2026-09-04 it blocked the main thread for an hour and froze the whole app. `stop()` uses the input node
  reference cached at open time for the same reason. If the timeout wins and CoreAudio answers later, the late engine
  is closed so the mic is never left open — and only *that* engine: `och()` returns its engine instead of storing it,
  and the tap writes to the buffer only while its attempt owns it (`buferEgasi`). The cleanup used to call `stop()`,
  which stopped the *retry's* newer engine and left the UI saying "recording" with the mic closed.
- **The user's clipboard survives dictation** (macOS, `src/clipboard.swift`). Until 1.2
  `finishTranscription` wrote the transcript to the pasteboard *before* `Inserter` saved
  the "old" value, so the restore put the transcript back and the user's clipboard was
  lost on every dictation — even in the slow mode that never uses the clipboard — and
  only `.string` was ever restored (images, files, rich text vanished). Now the fast mode
  snapshots every item and every type, writes the transcript marked
  `org.nspasteboard.TransientType` + `ConcealedType` (clipboard managers do not record it),
  sends ⌘V, and after 1 s restores the snapshot **only if `changeCount` is still ours** —
  a user who copied something meanwhile keeps it. The transcript is left on the clipboard
  only when inserting failed (the overlay says so). Verified on a private named
  pasteboard (text + RTF + PNG + file URL round-trip, empty clipboard, concurrent copy);
  the ⌘V path itself needs a manual check. Windows (C2) is still open — it must handle
  non-HGLOBAL formats and needs the VM to test.
- **Toggle guards** (`AppDelegate`): a 0.4s debounce (`toggleOynasi`) drops duplicate hotkey events, `boshlanyapti` blocks
  a second start while the mic is opening (force-cleared after `boshlashChegarasi` = 10s so a stuck permission dialog
  cannot kill the hotkey), and `maxYozish` (600s) auto-stops a recording that was left running — an unnoticed one ran
  32 minutes and pasted 7952 characters of room noise.
- **Hotkey** is persisted in `UserDefaults` under keys `hk.keyCode`, `hk.mods`, `hk.label`. Default: ⌃⌥D.
- **Login item / launch behaviour**: autostart goes through a LaunchAgent bundled at `Contents/Library/LaunchAgents/com.rubaistt.dictation.autostart.plist`, registered via `SMAppService.agent(plistName:)`. It starts the app with the argument **`--autostart`**, and that argument is the *only* way the app can tell a login launch from a user launch — macOS has no documented API for this (`NSApplicationLaunchIsDefaultLaunchKey` is also NO when saved state is restored; `XPC_SERVICE_NAME` is identical in both cases). The plist uses `BundleProgram` with a path relative to the bundle, because the app can live in `/Applications` or `~/Applications`. The rules: `--autostart` → menu bar only, no window; anything else → the window opens. `applicationShouldHandleReopen` opens the window when a running app's `.app` is double-clicked. `LoginItem.eskiRoyxatdanKochir()` migrates users off the old `SMAppService.mainApp` registration.
- **Single instance**: `AppDelegate.ortiqchaNusxaChiqsin()` terminates the app at launch if an older instance of the same bundle id is already running. This is required because launchd bypasses LaunchServices — registering the agent while the app is running starts a *second* copy, which means two menu-bar icons, a double-firing hotkey and the model loaded twice.
- **Idle unload**: model is freed from RAM after 180s of inactivity (see the `band` gotcha above for when this is deferred).
- **Permissions** required: Microphone (on first record) + Accessibility (for synthetic ⌘V). Accessibility is checked via `AXIsProcessTrusted()`.
- **LSUIElement window trap**: this is a menu-bar-only (`LSUIElement`) app with no Dock
  icon by default. Show any window with `orderFrontRegardless()`, never
  `makeKeyAndOrderFront(_:)` — the latter is unreliable for an `LSUIElement` app. Switch
  `NSApp.setActivationPolicy` to `.regular` while the main window is open, and back to
  `.accessory` when it closes, so Dock/⌘Tab visibility tracks whether the window is
  showing. See `asosiy_oyna.swift`'s header comment.
- **Long audio is chunked** above 90 minutes (`bolaklashChegarasi` in
  `transcribe_job.swift` — roughly 230 MB of float32 samples per hour of audio), into
  30-minute pieces. The cut point inside each 30-second search window is the quietest
  point (`jimlikNuqtasi` in `audio_util.swift`), so a chunk boundary never lands mid-word.
  Segment timestamps from later chunks are shifted by that chunk's time offset before
  being merged into one segment list.
- **AVFoundation asset properties are read with async `load(...)`**, not the synchronous
  `tracks(withMediaType:)` / `duration` deprecated since macOS 13. Studio's decode runs
  synchronously on its own background thread, so `media_decode.swift`'s `kutib` waits on
  the async call with a semaphore — never call it from the main thread. The remaining
  `AVAssetReader` warnings (`add`, `startReading`, `copyNextSampleBuffer`) appear only
  when targeting macOS 27; their replacements do not exist on the 13.0 deployment target.
- **Fonts: each platform uses its own system font** — SF Pro on macOS, Segoe UI on
  Windows. The design asks for IBM Plex Sans; bundling it was considered and declined
  (2026-09-25) because Mac-only Plex would break parity and change every text width.
- **Two repos since 1.2.0.** The full history stays in the private
  `MuhammadMirrr/uzbek-dictation`; the public `MuhammadMirrr/kotib` gets the tree as it
  is, **without `KEYINGI-ISH.md`** (internal to-do list), via `scripts/ochiq-repoga.sh`
  (one commit per sync, gitleaks first). Old GitHub release links
  (`/releases/download/v1.0/…`) are 404; downloads go through the CDN. Telemetry is
  hard-wired to `stat.mirqobilov.com` and not tied to the official signature, so
  self-built copies report to the same panel too (owner's requirement, 2026-10-08).
- **Update signing key (1.2, S2).** One Ed25519 key signs every update and every policy
  manifest on both platforms (spec D4). It lives in the **login Keychain** as Sparkle's
  item (service `https://sparkle-project.org`, account **`kotib`** — not Sparkle's default
  `ed25519`, so other Sparkle projects on the machine cannot collide). The public half is
  `scripts/yangilanish-kaliti.pub` (not secret; S5/S8 compile it into the apps). Everything
  goes through `scripts/imzo-kaliti.sh` (`yarat`, `ochiq`, `zaxira <file>`, `tikla <file>`,
  `tikla-sinov`). **If the key is lost no update can ever be shipped again** — installed
  apps accept only this key — so it must exist as two offline, encrypted copies
  (`zaxira`, then move the file off the machine). `tikla-sinov` proves a backup restores
  without touching the real key (export → import under a throwaway account → compare →
  delete). Two traps found while doing this: `generate_keys -p` reports a missing key on
  **stdout** with exit 0 (the script checks the 44-char base64 shape instead), and
  **`sign_update` reading the Keychain item blocks on a GUI permission dialog** — the
  item's ACL trusts only `generate_keys`, which created it. Release scripts therefore sign
  with `generate_keys -x` → temp file (0700 dir) → `sign_update --ed-key-file` → delete.
  A real `sign_update` signature (94-byte manifest and a 3 MB file) was verified by both
  CryptoKit and Monocypher, and a one-bit change was rejected by both.
- **Update logic is pure and shared** (`src/yangilanish_siyosat.swift` + `src/imzo.swift`,
  `win/core/versiya.cpp` + `yangilanish_siyosat.cpp` + `imzo.cpp`, Monocypher 4.0.3 vendored
  unmodified in `win/third_party/monocypher/` with its sha512). The server reply is
  `{"m": base64(manifest), "s": base64(signature)}` and the signature is checked over the
  exact bytes of `m` **before** `m` is parsed. Any invalid field rejects the whole
  manifest — a half-valid manifest is a guess. URLs pass a deliberately dumb allow-list
  (exact `https://cdn.mirqobilov.com/` or `https://stat.mirqobilov.com/` prefix, then only
  `[A-Za-z0-9._~/-]`, no empty/`.`/`..` segments) instead of a URL parser, so both
  platforms answer identically. `min_versiya > versiya` makes the manifest invalid rather
  than blocking the app forever on a condition it cannot meet; a clock moved backwards never
  blocks. The old ping-response banner path was removed in S5 (macOS) — see below.
- **Update client, macOS (1.2, S5) — Sparkle 2.10.0.** Pinned in
  `scripts/bogliqliklar.env`; `src/build.sh` fetches it with `sparkle_tayyorla` (sha256),
  links `-F sparkle -framework Sparkle` with rpath `@executable_path/../Frameworks`, copies
  the framework **without its XPC services** (they are only for sandboxed apps; Kotib is not
  sandboxed) and signs **inside-out without `--deep`** — `--deep` would stamp our
  `audio-input`/`allow-jit` entitlements onto Sparkle's `Autoupdate` and `Updater.app`.
  `scripts/make_pkg.sh` does the same with hardened runtime + timestamp. Info.plist (in
  `build.sh`): signed feed required, EdDSA public key from `scripts/yangilanish-kaliti.pub`,
  automatic checks/downloads, no system profiling. `src/yangilovchi.swift` is the policy:
  first check 60 s after launch only if the last **successful** feed load is > 24 h old,
  same on wake, retry 15 min → 1 h → 4 h on real errors (`SUNoUpdateError` is not one),
  and install through `immediateInstallationBlock` only when idle — no recording, no
  Studio job, no translation, ≥ 2 min since the last dictation (1 min once the update has
  waited 24 h; `YangilanishSiyosati.ornatishMumkinmi`, tested). A menu-bar app almost never
  quits, so Sparkle's "install on quit" alone would never fire. `src/yangilash_haydovchi.swift`
  is the Uzbek `SPUUserDriver`: **silent** for background checks, a small window only when
  the user pressed «Hozir tekshirish» (Settings); it refuses to relaunch mid-dictation.
  After the relaunch, `Yangilovchi` shows «Kotib X ga yangilandi» (overlay + window banner,
  `BannerXabar`). Statistics moved to `src/statistika.swift` (`Statistika`, renamed from
  `yangilanish.swift`) and no longer reads the ping response — spec G2: update and stats
  must not share a failure path. Acceptance (Gatekeeper, a real feed, silent update) needs
  a signed build and a person at the Mac — S5/S6.
- **macOS release pipeline (1.2, S6).** `scripts/make_pkg.sh` now also writes
  `dist/Kotib-<v>-mac.zip` of the **stapled** app (notarizing the `.pkg` covers the app inside,
  so `stapler staple` on the app normally just works; otherwise the zip is submitted on its
  own) — Sparkle cannot update from a `.pkg`. `./scripts/reliz.sh mac <v> [--bosqich-soat N]`
  copies the zip into `dist/sparkle-arxiv/` (keep it: old versions there are what deltas are
  built from), runs `generate_appcast` (signed feed, deltas, release notes = the version's
  `CHANGELOG.md` section via `scripts/changelog-bolimi.py`, phased rollout), uploads new files
  to `cdn.mirqobilov.com/dl/mac/` and writes KV `appcast-mac` + `appcast-mac@<v>`;
  `reliz.sh qaytar mac <v>` restores both policy and feed. Three things found by testing it
  with a throwaway key: (1) **one prefix, not one per version** — `generate_appcast` rewrites
  the URLs of the *old* items to the current `--download-url-prefix`, so per-version prefixes
  break every older link; immutability is kept by versioned file names plus a size check
  that refuses to overwrite an existing CDN key; (2) **no deltas for versions with a suffix**
  (`1.2.0-sinov1` → `-sinov2` gets only full updates) — test deltas with plain versions;
  (3) it refuses archives whose app fails code-signing checks. `curl -f` returns 22 on 404,
  and under `pipefail` + `set -e` that silently killed the script — `cdn_hajm` swallows it.
- **Update client, Windows (1.2, S8) — our own, `win/core/yangilovchi.{h,cpp}`.** Every
  decision is a shared pure function (`uygonishdaTekshirish`, `qaytaUrinishKechikishi`,
  `siyosatQarori`, `ornatishMumkinmi`, `qisqaIzoh`, `faylniTekshir`); the class only wires
  them to timers (ids **20–22** on the app's hidden window — the app's own are 1–9),
  `WM_POWERBROADCAST`/`PBT_APMRESUMEAUTOMATIC` and WinHTTP. First check 60 s after launch,
  then an **hourly** timer asks "24 h since the last *successful* check?" — a single 24 h
  `SetTimer` drifts across sleep. 404 from the Worker means "no release yet" and counts as
  success; a bad signature counts as a failure (retry schedule). The update downloads to
  `%LOCALAPPDATA%\Kotib\yangilanish\` through the resumable `faylYukla`, is renamed to a
  random `Kotib-<v>-yangilash-<hex>.exe`, then **opened with `FILE_SHARE_READ` only** and
  verified size → sha256 (`baytlarSha256`, CNG) → Ed25519 over the whole file. That handle
  stays open until `CreateProcessW`, so nobody can swap the file during the hours it may wait
  for an idle moment. Install = `… /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /KOTIBYANGILASH
  /LOG="…\ornatish.log"`, then `DestroyWindow` → normal shutdown. A mandatory update
  (`min_versiya`) waits only for dictation; the blocking card is S9. The architecture comes
  from `IsWow64Process2` (an x64 Kotib emulated on ARM64 still gets the ARM64 entry). The
  current version is **`KOTIB_VER_STR`** from the CMake-generated `kotib_versiya.h` (now on
  `rubai_core`'s include path), not the `.exe` resource: the numeric resource drops the
  suffix, so `1.2.0-sinov1` would never be offered `-sinov2`. The public key reaches C++ the
  same way (`KOTIB_YANGILANISH_KALITI`; CMake regex has no `{n}`, so the length is checked
  with `string(LENGTH)`). Stats moved to `win/core/statistika.{h,cpp}` (`pingYubor`, renamed
  from `yangilanish.*`) and ignore the ping response; the old banner + `ShellExecuteW(url)`
  path is gone. UI: a «Yangilanishlar» row in Settings (status + «Hozir tekshirish» /
  «Oʻrnatish») and a tray item. `settings.ini` keys: `yangilanish.oxirgiMuvaffaqiyat`,
  `.chelak`, `.majburiyKorilgan`, `.oxirgiIshlagan`, `.kutilgan`, `.kutilganIzoh`, `.kanal`.
  **Test channel:** `yangilanish.kanal=sinov` reads `/v1/yangilanish/win-sinov.json` — same
  server, same key, separate KV entry, so a VM can run the whole flow on real infrastructure
  without touching users. What the S7 installer must do for this to work: wait for the
  `Global\KotibSingleInstance` mutex to disappear before replacing files, relaunch Kotib on
  `/KOTIBYANGILASH` (and relaunch the old one if setup fails).
- **Mandatory update (1.2, S9), both platforms.** `min_versiya` comes only from the
  signed policy manifest (macOS now fetches `/v1/yangilanish/mac.json` too — Sparkle does
  not know about it). The requirement is **persisted** (`majburiyMin`, `majburiyMuhlat`,
  `majburiyKorilgan` — UserDefaults `yangilanish.*` / `settings.ini`) and the decision is
  `majburiyQaror` (shared table, `MAJBURIY` rows): a restarted app knows it is blocked
  without the network, and the grace period never restarts — except for a **different**
  `min`. A 404 or a manifest without `min` clears it (kill switch); an app that never got a
  signed manifest never blocks. Grace period → a non-dismissable chrome banner («Muhim
  yangilanish: Kotib X — <status>. N soatdan keyin diktovka toʻxtaydi»), an immediate
  check that ignores the 24 h rule (hourly until ready), and install as soon as no
  dictation is running (the idle rule is skipped). Expired → «Yangilanish majburiy —
  diktovka toʻxtatildi» + «Qayta urinish»/«Hozir oʻrnatish» + a **hard-coded** «Saytdan
  yuklab olish» (`uzb.mirqobilov.com`, never a server-supplied URL); only *starting* a
  recording is refused — stopping one always works. The design's separate block card became
  this banner (two buttons, no ✕: `BannerXabar.kalit == nil` / empty `kalit` on Windows) —
  the visual simplified, the capability stayed. On macOS Sparkle still does the download:
  `reliz.sh mac <v> --min V` marks the feed item critical (`--critical-update-version`),
  because Sparkle exempts critical items from phased rollout — without it a user could be
  blocked before their rollout group was due. `reliz.sh mac` now also publishes
  `siyosat-mac` on every release so a stale `min` cannot linger. The 1.1.0 bridge reply is
  prepared in `statistika/versiya.json` (direct installer URLs) and is written
  to KV `joriy` only at release (S24).
- **Update server side (1.2, S3).** The Worker serves `GET /v1/yangilanish/{mac,win}.json`
  (KV `siyosat-<p>`) and `GET /v1/appcast/mac.xml` (KV `appcast-mac`) verbatim, with
  `max-age=300`; it never signs or rewrites them, so a compromised Worker still cannot ship
  an update. `/v1/ping` and `/v1/amal` now answer independently of D1 — the writes run in
  `ctx.waitUntil` and only log on failure (spec H1: a D1 error or exhausted free quota used
  to return 500 and silently kill the update banner too). `scripts/reliz.sh`
  (`imzola`/`tekshir`/`kv-yoz [--foiz N]`/`qaytar <p> <v>`, plus `--quruq`, `--mahalliy`)
  is the only writer of those keys (S8 added `win <v> [--min V] [--muhlat N]`, which signs
  `dist/Kotib-<v>-win-yangilash.exe`, uploads it to `dl/win/` and publishes the manifest with
  both architectures pointing at that one universal installer, and `--kanal sinov`, which
  writes the `…-sinov` keys the Worker serves at `/v1/yangilanish/<p>-sinov.json` and
  `/v1/appcast/mac-sinov.xml`): it signs via an exported temp key (see the
  `sign_update` trap above), then verifies with `scripts/manifest-tekshir` — a tiny tool
  compiled from the app's **own** `yangilanish_siyosat.swift` + `imzo.swift`, so "the
  script accepted it" and "the app accepts it" cannot diverge — and, unless `--mahalliy`,
  HEADs every CDN URL and compares the size. Every publish also writes
  `siyosat-<p>@<versiya>`, which is what `qaytar` restores (after re-verifying it). Worker
  tests: `cd statistika && npm test` (Vitest inside workerd via `@cloudflare/vitest-plugin`
  — the old `@cloudflare/vitest-pool-workers` name is deprecated). `statistika/` now pins
  its own wrangler in `package.json`; the global 4.105 bundled a workerd too old for the
  Worker's `compatibility_date` (2026-09-01) to run `wrangler dev`. Note: `scripts/reliz.sh`
  (update policy) and the older `scripts/release.sh` (→ `make_pkg.sh`) are different
  things until S6 folds the `.pkg` pipeline into `reliz.sh`.
- **The app ships no API keys.** Local transcription needs no account and no network
  access; the LLM layer (Studio's "actions") is entirely optional, and when used, the API
  key is the user's own, entered by them, stored only in the Keychain
  (`llm_providers.swift`'s `Kalitlar`), and never logged — `Kalitlar.saqla` logs only an
  error *code* on failure, never the key itself.

## Architecture

### Single window

The app shows one window, `AsosiyOyna` (`src/asosiy_oyna.swift`). Its layout comes from the
Claude Design project **"Kotib - yangi.dc.html"**: an `NSToolbar` with a centred
two-item segmented control (**Yozish** / **Fayl**) and a trailing ⚙ button, above a
`KonteynerVC` that swaps in the selected tab's view controller. There is **no sidebar** —
the app has only two real modes, and a four-item source list added hierarchy over them
that carried no information.

Settings are **not a tab**: ⚙ opens `SozlamalarVC` as a modal sheet wrapped in
`SozlamalarSheetVC` (which supplies the design's footer: ♥ Donat qilish + Tayyor). There is
no onboarding section either — the permission banner inside the Yozish tab replaced it, so
the prompt lives where the permission is actually needed.

The window is pinned to the light appearance (`appearance = .aqua`): the design was drawn
in light only and its colours are the brand itself, so following the system's dark mode
would break it. All design tokens — colours, radii, fonts, the shared button/box builders —
live in **`src/kotib_uslub.swift`** (`enum U`). Add a colour there, never in a view file.

The two tab VCs are created lazily on first display and cached (`AsosiyOyna` holds them in
optional properties) so their state — an in-flight transcription job, Keychain-backed
settings — isn't lost when switching tabs:

- **`src/diktovka_view.swift`** (`DiktovkaVC`) — the Yozish tab: permission banner
  (`RuxsatBanneri`), the big press-to-talk card (`YozishKartasi` in `yozish_kartasi.swift`, which holds *both* the
  idle and recording states in one view so the two never jump relative to each other), and
  the history list backed by **`src/diktovka_tarixi.swift`** (`DiktovkaTarixi`), a
  pure-Foundation store persisting the last 200 dictations as JSON at
  `~/Library/Application Support/Kotib/diktovka-tarixi.json` (path from `src/yollar.swift`).
  Unit-tested.
- **`src/studiya_view.swift`** (`StudiyaVC`) — the Audio tab, two mutually exclusive views:
  the drop zone + recent-files list, and the transcript view (‹ Audio, text, and the
  Nusxa olish / Saqlash… / Matnni yaxshilash ⌄ footer). The old three-pane `NSSplitView`
  is gone.
- **`src/sozlamalar_view.swift`** (`SozlamalarVC`) — the settings sheet: hotkey,
  microphone, autostart, permission status, and a collapsible "Qoʻshimcha sozlamalar"
  section (input method, plain apostrophe, LLM, log, uninstall).

**«Tarjima qilish ⌄» in the Audio tab** hands the transcript to the Tarjima tab
(`StudiyaVC.onTarjima` → `AsosiyOyna.tarjimagaMatn` → `TarjimaVC.matnniQabulQil`). Three
choices worth keeping:

- It is a **separate footer button**, not an entry in "Matnni yaxshilash ⌄": that menu is
  hidden entirely when no LLM API key is configured, and translation is offline and needs
  no key.
- The result lands in the **Tarjima tab, not in the document**. A translation is not the
  transcript, so it is deliberately not stored under `natijalar/`; leaving the tab drops it.
- The menu offers the last-used target language (`tr.maqsad`, shared with the Tarjima tab),
  then Russian / English / Turkish, then "Boshqa til…", which hands over the text *without*
  starting so the user picks from the searchable 202-language box that already exists there.
  Uzbek is filtered out — the source is always Uzbek.

`matnniQabulQil` will not auto-start while a translation is already running: `Tarjimon`
takes one job at a time and does not reject a second call, so the old job's callback would
land on the new one's state. It cancels and waits for the user to press Tarjima.

- **`src/menyu.swift`** (`Menyu.qur()`) — the app's main menu bar, built once from
  `applicationDidFinishLaunching`. It exists because standard editing shortcuts are not
  magic on macOS: ⌘C/⌘V/⌘X/⌘A/⌘Z reach a text view only through an `NSMenuItem`'s
  `keyEquivalent`. Kotib had no `NSApp.mainMenu` at all (it started life as a pure
  menu-bar app), so none of them worked inside its windows. Menus: Kotib (⌘, settings,
  ⌘H, ⌘Q), Fayl (⌘O, ⌘W), Tahrir (the standard editing set), Koʻrinish (⌘1/⌘2/⌘3 tab
  switching), Oyna. Items use `target = nil` on purpose so AppKit routes the action down
  the responder chain to whatever text view is focused.

**Three things must stay true or the shortcuts silently die again:**

1. `NSApp.mainMenu` must be set even though the app is `LSUIElement` — the status-bar
   menu in `ilova.swift` is *not* a substitute (its `keyEquivalent`s only fire while
   that menu is open).
2. The window must be **key**, not merely main. `orderFrontRegardless()` does not make it
   key, and with no key window the responder chain menu validation walks is empty, so
   ⌘C/⌘W show up greyed out. `AsosiyOyna.keyQil()` calls `makeKey()` on the next runloop
   tick (activation is async) to fix this.
3. `NSTextView.allowsUndo` defaults to **false** — without it ⌘Z does nothing. Set on
   every editable text view (`tarjima_view.swift`, `studiya_view.swift`).

The undo/redo items use `QatiyNomliBand`, a tiny `NSMenuItem` subclass that refuses title
changes: `NSUndoManager` rewrites those two titles on every validation, and with no
localization in the app they would drop to English in an otherwise Uzbek menu.

**Settings apply immediately.** There is no Save/Cancel — the design has only "Tayyor",
which just closes the sheet. This matches macOS System Settings; do not reintroduce a
draft/commit model.

**Deliberate deviations from the design**, all following one rule — the visual can simplify,
the capability may not disappear:
- history row delete and "clear history" moved to the row's context menu;
- the formatted/raw transcript switch moved to the text view's context menu (a user with no
  LLM configured still needs the raw text);
- LLM base URL / model fields appear only for the `custom` provider.

`AsosiyOyna` also owns the `.regular`/`.accessory` activation-policy dance described in the
LSUIElement gotcha above, and pushes state into whichever VC is currently live (e.g.
`diktovkaHolati(yozilyapti:)` when a dictation starts/stops) — it caches the last-pushed
state so a VC created *after* the state changed (e.g. the user opens the window
mid-dictation) picks it up immediately instead of waiting for the next push.

**Auto Layout gotcha, learned the hard way twice.** An `NSScrollView` whose document is an
Auto Layout stack has *no intrinsic height* — left alone it collapses to zero (the settings
sheet rendered as nothing but its footer). And when a height is capped, Auto Layout takes
the missing space from whatever has the lowest vertical compression resistance: labels
were squashed from 22pt to **2pt**. `U.yozuv` therefore sets `.required` vertical
compression resistance, and wrapping labels need an explicit `preferredMaxLayoutWidth`.

### Dictation

Three layers, ObjC-C-bridged:
1. **`src/whisper_bridge.c` / `.h`** — C shim. Exposes `rubai_load`, `rubai_unload`, `rubai_transcribe`, `rubai_transcribe_segments`, `rubai_free_str`. Params: language `uz`, beam search size 5, GPU + flash attention; `rubai_transcribe` has no timestamps, `rubai_transcribe_segments` does (see the whisper_bridge gotcha above).
2. **`src/Bridging.h`** — `#include "whisper_bridge.h"` imported via `-import-objc-header`.
3. **The dictation core, one type per file** (split out of the old 1 300-line `dictate.swift` in S20): `whisper.swift` (`Whisper`), `yozuvchi.swift` (`Recorder`), `overlay.swift` (`Overlay`), `kirituvchi.swift` (`Inserter`), `hotkey.swift` (`HotKeyConfig`/`HotKeyStore`/`HotKey`), `avtostart.swift` (`LoginItem`), `log.swift` (`RubaiLog`), and `ilova.swift` (`AppDelegate` + `DiktovkaBand`) with `ilova_saqlanmagan.swift` (its unsaved-audio extension). The status menu is just Diktovka / (Saqlangan ovoz) / Oynani ochish / Chiqish — Settings/Welcome/Studio UI moved into `AsosiyOyna`'s sections. Settings/donate UI (`settings.swift`), microphone enumeration (`audio_devices.swift`), and the online-installer model downloader (`model_download.swift`) are split into their own files; top-level launch code lives in `src/main.swift` (Swift requires that once a module has several files).

Data flow per dictation: ⌃⌥D → `Recorder.start` (mic → 16 kHz float32) → ⌃⌥D → `Recorder.stop` → `Whisper.transcribe` (off main thread, Metal) → `Inserter.insert` (clipboard + ⌘V).

**A dictation's audio is never thrown away** (S10). If `Whisper.transcribe` fails (model
missing/corrupt, `rubai_transcribe` returned NULL — which is now distinct from an empty
string, "no speech"), `ovozniSaqla` writes the samples as 16 kHz mono 16-bit WAV to
`Yollar.saqlanmagan` (`saqlanmagan.swift`; newest 20, ≤ 7 days). The status menu shows
«Saqlangan ovozni matnga oʻgirish (N)» while any exist. They are retried once
automatically after the next successful dictation or a finished model download, only
while no dictation/Studio job is running (the whisper queue is serial). A retried text goes
to the **history** with the original date — and to the clipboard when the user clicked the
menu item — but is **never** pasted into the focused app: it would land in whatever window
happens to be in front at that moment.

### Studio

Studio turns an arbitrary audio/video file into a readable transcript, optionally
post-processed by an LLM. New files, one responsibility each:

- **`src/media_decode.swift`** — decodes any audio/video file to 16 kHz mono float32 using
  AVFoundation only (no ffmpeg); supports reading a time range and mid-decode cancellation.
- **`src/audio_util.swift`** — pure signal helpers: `jimlikNuqtasi` (quietest point in a
  window, used to choose chunk-cut points) and `kuchaytir` (normalizes quiet input up
  toward whisper's expected level). Foundation-only, unit-tested.
- **`src/text_format.swift`** — turns whisper segments into readable text: paragraph
  splitting by pause length, apostrophe normalization (see the apostrophe convention
  above), sentence capitalization. Foundation-only, unit-tested.
- **`src/hujjat.swift`** — transcript persistence (`HujjatOmbori`) under
  `~/Library/Application Support/Kotib/hujjatlar/<uuid>/` (path from `src/yollar.swift`): raw segments as JSON,
  formatted/raw text, and per-action LLM results, with path-traversal-safe component
  names and metadata written last so a document is only "complete" once `hujjat.json`
  exists.
- **`src/transcribe_job.swift`** — `TranskripsiyaIshi`, the job orchestrator: decode →
  chunk if needed (see the long-audio gotcha above) → whisper (via
  `rubai_transcribe_segments`) → `HujjatOmbori.saqla`. Owns the single-job lock and the
  `Whisper.shared.band` flag described above.
- **`src/studiya_view.swift`** — the Studio section: document library, drag & drop, text
  view (raw/formatted tabs), action panel, and progress UI (see **Single window** above).
- **`src/llm_client.swift`** — optional LLM layer, Foundation-only: the `provayderlar`
  preset list, two adapters (an OpenAI-compatible `/chat/completions` client and an
  Anthropic-native `/messages` client), SSE stream parsing for both, and text chunking
  (`paragrafBolaklari`) for long transcripts. Unit-tested.
- **`src/llm_providers.swift`** — `Kalitlar`, which stores the user's API key in the
  macOS Keychain keyed by provider id and never logs the key itself (only an error code
  on failure); and `LLMSozlama`, which persists the user's chosen provider/model/base URL
  in `UserDefaults` and reports whether LLM use is configured (`sozlanganmi`).
- **`src/amallar.swift`** — the fixed list of Studio actions (`amallar`) and their Uzbek
  system prompts (cleanup, summary, etc.), each marked whether its output is
  map-reduced across chunks (`yiguvchi`) or applied chunk-by-chunk and concatenated.

Data flow per Studio job: drop a file on `studiya_view.swift` → `TranskripsiyaIshi.boshla`
→ `mediaMalumot`/`media_decode.swift` (decode, chunked above 90 min) → whisper via
`rubai_transcribe_segments` per chunk → segments merged with shifted timestamps →
`text_format.swift` formats raw + readable text → `HujjatOmbori.saqla` persists both →
the Studio section displays it; an optional action from `amallar.swift` then sends the text
through `llm_client.swift` to the user's configured provider and saves the result via
`HujjatOmbori.natijaSaqla`.

### Tarjimon

Offline translation between the model's **202 languages**, any direction. Third tab,
«Tarjima».

- **Model**: NLLB-200 **3.3B**, converted to CTranslate2 and quantized to **int8**
  (3.36 GB on disk, 3.11 GB compressed). Lives in
  `~/Library/Application Support/Kotib/tarjima-model-33b/` (path from
  `src/yollar.swift`). It is **not bundled in the `.app`** — the user downloads it
  on first use, so the installer stays the size it is today.
  The folder name carries `-33b` deliberately: without a rename `TarjimaModel.tayyormi`
  reads a leftover 1.3B folder as complete and the upgrade never starts. The old
  folder is deleted **after** the new model is in place (`tarjima_yuklovchi.swift`'s
  `ochish`), freeing 1.38 GB — never before, or an interrupted download would leave
  the user with no working model.
- **Why NLLB** (2026-08-30 measurement, FLORES-200, **50** sentences — not comparable
  with the 200-sentence numbers below): NLLB-1.3B scored
  **51.2** against MADLAD-400 3B (43.6) and TranslateGemma 4B (38.6). The decisive
  reason is narrower than the score: **only NLLB has a `uzn_Latn` code**. Both Google
  models write Uzbek in Cyrillic, and even after transliteration they stay well
  behind. TranslateGemma also hallucinates on Uzbek — it invented a phrase that was
  not in the source.
- **The model archive is live on the CDN** at
  `https://cdn.mirqobilov.com/dl/tarjima/v2/nllb-200-3.3B-int8.tar.gz` (3 114 748 195
  bytes, uploaded 2026-08-31, sha256 verified against the local copy). The v1 1.3B
  archive stays where it is — the key is immutable and old builds still point at it. It holds the
  four files `TarjimaModel.kerakliFayllar` requires inside one folder, because
  `tarjima_yuklovchi.swift` extracts with `--strip-components 1`. `tokenizer.json` is
  deliberately absent (17 MB, unused). Objects carry `Cache-Control: immutable`, so
  **never overwrite this key** — a new build needs a new prefix.
- **Uploading anything over 300 MiB to R2**: `wrangler r2 object put` refuses it, and
  the wrangler OAuth token has no `r2` scope for the S3 API either. Use
  **`scripts/r2-katta-yukla.sh <file> <key>`**: it deploys a throwaway Worker with an R2
  binding (`createMultipartUpload` / `uploadPart` / `complete`, random per-run token),
  pushes 50 MiB parts over HTTPS, checks the CDN size and deletes the Worker (also on
  failure); it refuses an existing key. Wait for the token to work, not for a 403 — a
  Worker whose secret has not propagated yet also answers 403. The 1.2.0 `.pkg` and
  `setup.exe` went up this way and their CDN copies match by sha256.
- **Sparkle ignores version suffixes.** `SUStandardVersionComparator` says
  `1.2.0-sinov2` = `1.2.0-sinov1` = `1.2.0` (checked against the shipped Sparkle 2.10.0).
  A suffixed item updates nobody, and a machine running `1.2.0-sinovN` never sees the
  real 1.2.0 as newer. `reliz.sh mac` therefore accepts only `X.Y.Z`; test the update
  channel with numbers **below** the release (`1.1.90` → `1.1.91`, which also exercises
  deltas). Windows uses our own comparator and is not affected.
- **An open sheet or modal window cancels the install.** Sparkle's immediate install
  sends a quit Apple event; AppKit answers «App termination blocked by modal sheet» and
  Sparkle does not retry — the forced-update banner sat on «yuklanmoqda…» forever (S9
  test). `Yangilovchi.modalOchiq` counts an attached sheet or `NSApp.modalWindow` as busy.
- **Why int8**: CTranslate2 supports only `float32`, `int8` and `int8_float32` on the
  CPU — `float16`/`bfloat16` are GPU-only and `int16` does not exist on ARM. For 3.3B
  that is 13.4 GB versus 3.36 GB, and int8 is faster. It runs **on the CPU only** — no
  GPU required, which is what makes it usable on the weak Windows machines that are
  ~76% of the install base.
- **Batch size stays 1.** Measured on an M5: peak RSS 2.65 GB at batch 1, **6.00 GB at
  batch 16**. Batching is the obvious speed optimisation and it is the wrong one here —
  it pushes an 8 GB machine into swap. `tarjimon.swift` feeds sentences one at a time.
- **Decoding length is derived from the source**, `2 * source_tokens + 30`
  (`tarjima_bridge.cpp`). It used to be a fixed 256 and long sentences were **silently
  truncated** — verified: a 380-token input stopped at exactly 256. FLORES never caught
  this because its sentences are ~50 tokens.
- **`no_repeat_ngram_size = 4`.** Lifting the length cap exposed a degeneration loop on
  punctuation-free input (dictation output can look like that): «мышление В дальнейшем
  мышление В дальнейшем…». The 256 cap had been hiding it. Measured on FLORES-200
  (200 sentences, uzn_Latn → rus_Cyrl): chrF++ 47.90 → 47.87, i.e. noise, while word
  diversity on degenerate input goes from 0.08 to 0.62.
- **Quality is measured, not guessed** — `scripts/eval/`. Current numbers (M5, int8,
  FLORES-200 devtest, 200 sentences, uzn_Latn → rus_Cyrl):
  1.3B **47.90** chrF++ at 0.75 sentences/s, 3.3B **49.10** at 0.33. Read that number
  next to `scripts/eval/tibbiy.txt` output, never alone: chrF++ under-rewards grammar
  fixes, and the same jump that moved the score by 1.2 fixed five of eight real defects
  (gender agreement, dangling pronoun, modality, superlative, lexical choice).
- **Three ideas were measured and rejected** — do not re-propose them without new
  evidence; see `docs/superpowers/specs/2026-08-31-tarjima-sifati-design.md`.
  Round-trip reranking: the signal inverted, ranking the wrong sentence highest.
  N-best disagreement: all five beams share the model's one wrong belief.
  Splitting long sentences: output got worse. Terminology errors (`issiq bosishi` →
  `перегрев`) cannot be fixed by decoding — the model does not know the word.
- **`llama.cpp` does NOT work here**: `llama-server` has no encoder-decoder (T5 /
  M2M100) support. It loads a MADLAD GGUF and emits garbage. That is why the engine
  is CTranslate2. Don't re-litigate this without re-testing.
- **Tokenization must match HuggingFace exactly**: `[source_lang] + sentencepiece
  pieces + "</s>"`, with `[target_lang]` as the decoder prefix. Get this wrong and
  the model still produces output — quality just degrades silently. The reference
  is in `tarjima_bridge.cpp`'s header comment.
- **Language names come from macOS**, not a hand-written table: `Locale(identifier:
  "uz")`. Its Uzbek data is incomplete — 13 languages have no name and 11 come back
  in **Russian** (a CLDR fallback), so those 24 are hardcoded in `tillar.swift`'s
  `qoshimchaNomlar`. Tests assert no name is Cyrillic and none is missing.
- **Emoji and em dashes** become `<unk>`. `matn_boluvchi.swift` swaps `—`/`–` for a
  plain `-` and lifts emoji out of the sentence, re-appending them after translation.
  `tozala` restores the em dash on the way out, but only when the hyphen has a space on
  both sides — otherwise `ijtimoiy-iqtisodiy` would be mangled. It also pulls in the
  space the model likes to leave before punctuation («У женщин , принимающих»).
- **Sentence boundaries come from ICU**, `enumerateSubstrings(options: .bySentences)`.
  Splitting on `.` by hand is what broke `t.me/dr_azamoff`: it became `t.` + the rest,
  and `t.` came back from the model as `п.`. ICU also knows `3.14`, `v1.0`,
  abbreviations, and Chinese `。` / Arabic `؟` — 202 languages for free.
- **Links, emails and `@handles` never reach the model.** `NSDataDetector` finds them
  and they become `Bolak.xom`, restored in place. It deliberately does not fire on
  `3.14` or `v1.0`. Unlike emoji, a link is *not* pushed to the end of the sentence —
  that would turn «Manba: link» into «link Manba:».
- **Memory**: the translation model unloads after 180 s idle, exactly like whisper
  (`Tarjimon.bosatishniRejalashtir`). Together the two models now take ~3.7 GB — on a
  16 GB machine that means swap, which is why neither stays resident.
- **Disk space is checked before the download starts** (`TarjimaModel.yetarliJoyBormi`).
  It needs archive + extracted copy, 6.5 GB, and any `.part` already on disk counts
  toward it. Without the check a full disk failed 3 GB in with no usable message.
- **Cancellation happens between sentences**: CTranslate2 cannot abort mid-translation.
- **`NSTextView.string` is a lazy view over the internal buffer.** Assigning it to a
  local and then writing to that text view mutates the local too. The ⇄ swap silently
  put the same text in both boxes until `String(view.string[...])` forced a copy.

**Scope is closed (2026-09-25).** Wiring translation into dictation (speak Uzbek, type
Russian), Uzbek Cyrillic output and a `beam_size = 1` "fast mode" were all considered
and declined — do not reopen them without the owner asking.

Files: `tarjima_bridge.cpp/.h` (C bridge) → `tarjimon.swift` (Swift layer) →
`tarjima_view.swift` (UI); `matn_boluvchi.swift`, `tillar.swift` and
`tarjima_model.swift` are pure Foundation and unit-tested; `tarjima_yuklovchi.swift`
downloads and unpacks the model.

## Gitignored artifacts

`whisper.cpp/`, `ctranslate2/`, `sentencepiece/`, `sparkle/`, `*.bin`, `.venv/`, `.assets/`,
`dist/`, `tests/.build/`, `__pycache__/` — fetched/built, never committed.
