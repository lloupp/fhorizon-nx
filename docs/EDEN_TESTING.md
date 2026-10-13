# Testing Forza Horizon NX homebrew with Eden Emulator

**Name:** Eden (not "Elden"). Official site: https://eden-emu.dev/ . Source: https://git.eden-emu.dev/eden-emu/eden ; GitHub mirror: https://github.com/eden-emulator/mirror

Eden emulates the **Nintendo Switch**, NOT the Xbox 360. It can test a native Switch `.nro` built by this project, but it cannot run `default.xex` directly. It cannot replace hardware validation, especially of deko3d GPU features, timing, memory, audio, Vulkan compatibility, and homebrew app-mode behavior.

Eden's public [command-line handbook](https://github.com/eden-emulator/mirror/blob/master/docs/user/CommandLine.md) documents `eden-cli --game <path>` (or `eden --game <path>` on Qt), `--debug`, `--null-render`, and Qt's `-hlaunch`. **Do not assume all builds run standalone NRO files successfully.** Homebrew requiring hbloader may need the emulator's supported homebrew-launcher flow, and compatible user-provided system components. Do not download or distribute firmware/keys/Atmosphere/NSPs through this repository.

## Tested Linux setup

See [actual results](EDEN_RESULTS_2026-10-13.md). Install devkitPro `switch-dev` using its official tooling, then set `DEVKITPRO`. The Linux Eden setup script downloads the pinned official stable AppImage, checks its recorded SHA256, extracts it without FUSE and creates `.local/tools/eden-cli`:

```sh
./scripts/setup_eden_linux.sh
export PATH="$PWD/.local/tools:$PATH"
export DEVKITPRO=/opt/devkitpro
./scripts/build_switch_smoke.sh
python3 scripts/test_eden.py --nro .local/switch-smoke/fhnx_smoke.nro --dry-run
./scripts/eden_input_smoke.sh .local/switch-smoke/fhnx_smoke.nro .local/tools/eden-cli SMOKE
```

The Linux integration script needs Xvfb, xwininfo, xdotool, ImageMagick (`import`) and a working Vulkan driver. On this CPU-only host we used Mesa lavapipe, selected with `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json`. The script enables the AppImage's `SHARUN_ALLOW_SYS_VKICD=1`, creates isolated config/data directories, starts a local virtual display, captures the screen, injects three A (`a`) presses and Plus (`m`) and requires ordered guest events. It assumes a fresh SDL input config; it does not overwrite a user's existing Eden profile.

A 90-second timeout is an upper bound; successful input tests exit much earlier. Capture output resides under `.local/eden-tests/<NRO name>/captures/`; inspect it visually. Full logs stay private. The script launches Xvfb and Eden within the same environment, using loopback TCP for environments without Unix socket support. `FHNX_DISPLAY_NUMBER` can select a display explicitly, and `FHNX_SETTLE_SECONDS` can allow slower boots.

`setup_eden_linux.sh` selects the official amd64 PGO package. Older CPUs or other architectures should choose their official compatible release manually and pass `--binary` instead.

## Procedure

1. Install Eden from its official site and record version/hash. Confirm `eden-cli --version` or `eden --version`; document OS and GPU driver.
2. Build the independent libnx input/console NRO with devkitPro:
   `./scripts/build_switch_smoke.sh`. This is **not** the Forza game.
3. Check the NRO path; by default the build is expected at `.local/switch-smoke/fhnx_smoke.nro` (the build itself is unverified until devkitPro is present).
4. Dry-run the invocation without starting the emulator:
   ```sh
   python3 scripts/test_eden.py --nro .local/switch-smoke/fhnx_smoke.nro --dry-run
   ```
5. Execute a bounded smoke test (opens GUI or SDL frontend and logs locally; **does not** claim boot):
   ```sh
   python3 scripts/test_eden.py --binary eden-cli --nro .local/switch-smoke/fhnx_smoke.nro --seconds 90
   ```
   You may omit `--binary` to auto-detect `eden-cli` / `eden`. Qt is sometimes required for homebrew launcher workflows; see upstream's `-hlaunch` documentation.
6. Inspect `.local/reports/eden-smoke.json`, `.local/reports/eden-smoke.log`, and the emulator's screen and own logs (private). Record manual results:
   - App launches / renders text?
   - A button increments counter?
   - Plus button exits?
   - FPS / memory / renderer warnings?
   - Does direct NRO loading require hbloader?
7. Compare again on an actual Switch. No emulator-only pass may mark a real hardware gate complete.

## Interpretation

- Script dry run: verifies the **command shape only**, not compatibility.
- With `--expect-events SMOKE` or `--expect-events GRAPHICS`, absent or unordered boot/frame/A/Plus/shutdown markers cause exit status 5 even if the emulator exits 0. `nro_boot_verified` and `input_exit_verified` reflect those guest markers; correct pixels still require capture review.
- `emulator_process_started`: the operating system started Eden, not the NRO.
- 0 return code: Eden exited successfully; does **not** mean the game started.
- Timeout: an emulator process remained alive until a bounded stop; **neither a pass nor a failure**.
- Black screen, unsupported services or missing keys: record the precise block. Never invent a functioning boot.
- This project must never publish original Forza game content, generated translated C++, encrypted Nintendo content, keys, system files, or copyrighted runtime assets. Original diagnostic screenshots and sanitized event reports can be published.

## Useful alternatives

Use Pinyon Shift on PC as an **Xbox 360 recompilation behavior baseline**, and Eden for **Switch application-layer smoke tests**. These cover different parts of the development problem.

## Debugging and graphics

The upstream handbook describes `--config` for a separate configuration, `--debug` for the GDB stub (default port 1234), and input profiles. The installed 0.2.1 SDL `--help` lists its actual supported options; do not assume every option from the current handbook exists in older stable builds. `--null-render` cannot validate a screen. Qt `-hlaunch` is a separate launcher flow; the independent NROs tested here loaded directly without it.

Our diagnostics send `FHNX_SMOKE` or `FHNX_GRAPHICS` messages with `svcOutputDebugString`, which Eden records as `Debug.Emulated`. The deko3d failure investigation used an isolated config with debug logging and single-core mode. No keys/firmware or changes to Eden source were needed.

`./scripts/build_switch_graphics.sh` compiles UAM vertex/fragment shaders offline and links their DKSH blobs into an independent deko3d NRO. Its Eden execution remains blocked; preserve the failure. `./scripts/build_switch_framebuffer.sh` builds a separate CPU triangle that has passed, and does not bypass the GPU milestone.
