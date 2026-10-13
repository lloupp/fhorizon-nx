# Evidence ledger

Date: 2026-10-13. This scaffold was created without an FH1 game dump or Switch connected.

## External source observations (not reproduced here)

- `arcanite24/pinyon-shift` README and docs/ANDROID.md/ROADMAP.md: native Pinyon Shift, its own reports of playable PC/Steam Deck builds and experimental Android ARM64; Switch is on its mid-term roadmap. Vulkan is its chosen GPU path. These are developer claims, not FHNX runs.
- `NerunSmarts/ForzaRecomp` README/docs/status.md: a different ARM64 macOS recompiler project; its status document describes menu/world drawing, but gameplay and performance remain under investigation.
- `devkitPro/deko3d` README and `devkitPro/uam` README: Switch-specific GPU and offline GLSL→DKSH compiler.
- `HayatoG/switch-nvk` README: experimental libnx Vulkan implementation, unqualified for FH1 workload.

## Local checks

Record the output from `python3 -m unittest discover -s tests -v`, `ctest --test-dir .local/cmake-build --output-on-failure`, `python3 tools/fhnx.py doctor` and `python3 tools/fhnx.py audit .` in a local `.local/reports/` log. A `PASS` there is strictly about synthetic inputs.

## Not verified

- Xbox 360 Forza Horizon image availability or hashes in the user's possession.
- ReXGlue/ShiftGlue game translation or game boot in this environment.
- ARM64 toolchain build for Switch or Switch-NVK Vulkan extensions.
- Real Switch NRO build, Switch memory budgets, sound, input or graphics.
- Any Forza Horizon NX gameplay, FPS or installable NSP.

## Executed validation in this session (2026-10-13)

- Environment: Linux x86_64, GCC 14.2.0, CMake 3.31.6, Python 3.13.5.
- `python3 -m unittest discover -s tests -v`: **5 passed** (synthetic XEX magic, read-only metadata, blocked files, tool detection).
- `cmake -S . -B .local/cmake-build -DCMAKE_BUILD_TYPE=Release`: **configured successfully**.
- `cmake --build .local/cmake-build --parallel 2`: **both native executables built**.
- `ctest --test-dir .local/cmake-build --output-on-failure`: **2/2 passed**.
- `.local/cmake-build/fhnx_pc_smoke`: **PASS**, synthetic guest memory only.
- `python3 tools/fhnx.py audit .`: **safe_for_review: true**, no issues.
- `python3 tools/fhnx.py doctor`: devkitPro absent; `aarch64-none-elf-gcc`, `elf2nro`, `uam`, `spirv-cross` absent.
- No game data, runtime Pinyon Shift, GPU backend, Nintendo console or emulator was run.

## Added Eden emulator test infrastructure (2026-10-13)

- Eden official project `https://eden-emu.dev/`; command line documented at `https://github.com/eden-emulator/mirror/blob/master/docs/user/CommandLine.md` (`eden-cli --game <path>`, GUI `-hlaunch` with hbloader requirements).
- Added `scripts/test_eden.py` and synthetic `tests/test_eden.py` to run bounded subprocess smoke tests with private `.local/reports` output; `nro_boot_verified` and `physical_switch_verified` are always `false` unless separately validated by human observation/hardware.
- **No Eden execution, NRO boot or gameplay test is claimed by these changes.** The new unit tests exercise only the wrapper's command selection and result formatting.
- Next: run Python unit tests and GitHub CI; when devkitPro is available build a real homebrew NRO, then test Eden, then the physical Switch.
