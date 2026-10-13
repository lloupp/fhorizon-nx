# Testing Forza Horizon NX homebrew with Eden Emulator

**Name:** Eden (not "Elden"). Official site: https://eden-emu.dev/ . Source: https://git.eden-emu.dev/eden-emu/eden ; GitHub mirror: https://github.com/eden-emulator/mirror

Eden emulates the **Nintendo Switch**, NOT the Xbox 360. It can test a native Switch `.nro` built by this project, but it cannot run `default.xex` directly. It cannot replace hardware validation, especially of deko3d GPU features, timing, memory, audio, Vulkan compatibility, and homebrew app-mode behavior.

Eden's public [command-line handbook](https://github.com/eden-emulator/mirror/blob/master/docs/user/CommandLine.md) documents `eden-cli --game <path>` (or `eden --game <path>` on Qt), `--debug`, `--null-render`, and Qt's `-hlaunch`. **Do not assume all builds run standalone NRO files successfully.** Homebrew requiring hbloader may need the emulator's supported homebrew-launcher flow, and compatible user-provided system components. Do not download or distribute firmware/keys/Atmosphere/NSPs through this repository.

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
- `emulator_process_started`: the operating system started Eden, not the NRO.
- 0 return code: Eden exited successfully; does **not** mean the game started.
- Timeout: an emulator process remained alive until a bounded stop; **neither a pass nor a failure**.
- Black screen, unsupported services or missing keys: record the precise block. Never invent a functioning boot.
- This project must never publish original Forza game content, generated translated C++, encrypted Nintendo content, keys, system files, or copyrighted runtime assets.

## Useful alternatives

Use Pinyon Shift on PC as an **Xbox 360 recompilation behavior baseline**, and Eden for **Switch application-layer smoke tests**. These cover different parts of the development problem.
