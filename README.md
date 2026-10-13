# Forza Horizon NX — research and first reproducible smoke tests

**Status: feasibility scaffold, NOT a port of Forza Horizon.** This repository's original code does not boot or run Forza Horizon. It does not contain executable XEXs, copyrighted game assets, translated guest instructions, screenshots, save files, console firmware or encryption keys. Do not distribute any of these.

Goal: research a native **Nintendo Switch (original Tegra X1, libnx/homebrew)** build of **Forza Horizon 2012 for Xbox 360**, using the ongoing community AOT recompilations as references. Xbox 360 game Title ID: `4D5309C9` (identifier, not a verification hash).

## Upstream research (verified by reading public source docs on 2026-10-13)

- [Pinyon Shift](https://github.com/arcanite24/pinyon-shift) — most mature base: ShiftGlue/ReXGlue recompilation, Vulkan renderer, Windows/Linux/macOS, and experimental Android ARM64. Upstream already lists Nintendo Switch as mid-term feasibility work. Game files must come from a user's own verified copy.
- [ShiftGlue](https://github.com/arcanite24/shiftglue-sdk) — upstream's runtime and codegen fork.
- [ForzaRecomp](https://github.com/NerunSmarts/ForzaRecomp) — independent macOS ARM64 proof points; does not yet qualify sustained gameplay.
- [ForzaHorizonRecomp](https://github.com/Luiz3du/ForzaHorizonRecomp) — alternate XenonRecomp analysis; not a playable release.
- [deko3d](https://github.com/devkitPro/deko3d) and [UAM](https://github.com/devkitPro/uam) — established homebrew GPU API and offline shader compiler.
- [switch-nvk](https://github.com/HayatoG/switch-nvk) — experimental Mesa NVK on libnx; do not assume compatibility or feature completeness without testing it.
- [Xenia Canary](https://github.com/xenia-canary/xenia-canary) — PC behavior baseline when a strong PC and lawfully obtained game copy are available; **not** the Switch runtime.
- [Eden Emulator](https://eden-emu.dev/) — optional Nintendo Switch emulator, CLI/Qt available; useful to smoke-test built Switch NROs on PC before testing real hardware. Not an Xbox 360 emulator.

## What is already implemented HERE

1. `tools/fhnx.py doctor` — host and Switch SDK tool inventory.
2. `tools/fhnx.py inspect /path/to/extracted/FH1` — read-only XEX2 signature and SHA-256 of three expected XEX modules; **not** an official-disc signature verification. For ISO input, returns metadata only. Never decrypts, modifies, copies or publishes game data.
3. `tools/fhnx.py audit .` — conservative distribution blocker for likely copyrighted/generated files.
4. `src/common/guest_memory.hpp` — platform-neutral big-endian PowerPC guest memory access, with explicit bounds checking; used by host unit tests and a PC smoke binary.
5. `shaders/triangle.*` and `scripts/check_shaders.py` — independent example shaders to attempt offline UAM→DKSH translation if UAM is present; no game shaders.
6. `switch/source/main.c` and `scripts/build_switch_smoke.sh` — standard libnx input/console smoke, built only when devkitPro's official application template is available; **not** a racing game.
7. `scripts/test_eden.py` and `docs/EDEN_TESTING.md` — bounded local Eden Emulator CLI smoke workflow for a compiled NRO, with JSON report and private logs. The emulator does not execute an Xbox 360 XEX directly.
8. Python and CMake/CTest automated checks. All can run without a Switch and without a game image, except optional Switch/Emulator/UAM test targets.

## Validate on PC (no game required)

```sh
python3 tools/fhnx.py doctor
python3 -m unittest discover -s tests -v
cmake -S . -B .local/cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build .local/cmake-build --parallel 2
ctest --test-dir .local/cmake-build --output-on-failure
.local/cmake-build/fhnx_pc_smoke
python3 tools/fhnx.py audit .
```

### With your own game files (optional, not needed for this milestone)

```sh
python3 tools/fhnx.py inspect /path/to/your/extracted/FH1 --out .local/reports/disc-preflight.json
```

Expected files at the root: `default.xex`, `XMediaFacade_default.xex`, `SpeechFacade_default.xex`. An `XEX2` magic match proves only a likely file type. **Never infer a supported disc revision from the file name or title ID.** Verify the exact dump against the upstream project's trusted pinned manifest before using address-specific hooks or generating code.

### Upstream trial on a suitable PC

```sh
./scripts/clone_upstream.sh
# Inspect the upstream README and docs/BUILDING.md *at the recorded commit*.
# Use Pinyon Shift's own verified launcher/setup flow with your own disc.
```

Do not copy upstream generated C++ or binaries into a public repo. The upstream PC gameplay tests are independent evidence and **not** proof that Forza Horizon NX works.

### Optional Switch smoke build

```sh
# Requires an installed devkitPro switch-dev group and its official template:
export DEVKITPRO=/opt/devkitpro
./scripts/build_switch_smoke.sh
# Expected local (unvalidated here): .local/switch-smoke/fhnx_smoke.nro
```

Before testing on a real Switch, try the optional Eden Emulator PC flow:

```sh
python3 scripts/test_eden.py --nro .local/switch-smoke/fhnx_smoke.nro --dry-run
python3 scripts/test_eden.py --nro .local/switch-smoke/fhnx_smoke.nro --seconds 90
```

These require a built local NRO, and the second requires an independently installed Eden. See [Eden testing](docs/EDEN_TESTING.md) for setup, Qt hbloader caveats, and evidence. Emulator results do not establish a working real Switch. Only a real Switch with homebrew can validate that NRO's actual device behavior. It does not use any game content. Compile UAM shader example with `python3 scripts/check_shaders.py` when `uam` is installed. UAM is a host tool and shaders are offline-compiled.

## Feasibility conditions and roadmap

See [architecture](docs/ARCHITECTURE.md), [evidence and blockers](docs/EVIDENCE.md) and [prioritized milestones](docs/MILESTONES.md). The primary blocker is bridging the upstream Vulkan renderer to a suitable native Switch GPU backend; `deko3d` and experimental `switch-nvk` are alternatives requiring their own proofs. Runtime/threading, filesystem, audio, shader compilation and memory footprints are separate work. Switch performance is unknown; no FPS promise.

This is not affiliated with Microsoft, Nintendo, Playground Games, Turn 10 or the upstream teams. The original code written in this scaffold is MIT-licensed (`LICENSE`); third-party projects have their own licenses. All game rights remain with the respective owners.
