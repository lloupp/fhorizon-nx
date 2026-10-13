# Architecture decisions — FHNX

## ADR 001: Reuse a verified existing title-specific AOT system

Prefer upstream **Pinyon Shift + ShiftGlue**, with a pinned public source commit recorded for each experiment. Upstream is already a substantially more complete Xbox 360 environment than an isolated XenonRecomp translation. Do not treat the AMD64 executable, Android APK, or macOS app as directly compatible with Nintendo Switch Horizon OS.

Separate three independent artifacts:

- **Game code translation:** generated on the owner's PC, per supported XEX hash and title revision; private.
- **Portable host runtime:** title emulation services, guest memory, scheduling, audio, graphics frontend; may need libc, OS, ARM64 and memory fixes.
- **Switch platform backend:** libnx app lifecycle, input, storage, graphics, audio, thread/clock services. Must avoid desktop-only assumptions such as `dlopen`, Windows APIs, POSIX APIs not supplied by the Switch toolchain, and 8-GB physical memory.

## ADR 002: GPU alternatives (no selected production backend)

A. **deko3d / DKSH**: Switch homebrew-native. Need render-pass, resource, texture, fetch, shader/pipeline translation from the upstream Vulkan path to deko3d; offline UAM generates DKSH. SPIR-V != DKSH, and Vulkan calls cannot be forwarded to deko3d without an adapter.

B. **switch-nvk**: existing experimental Vulkan implementation over libnx. Start with standalone triangle, required Vk features/extensions, format queries, presentation, memory budgets, frame times. Do **not** assume the requested Vulkan 1.3 GPU feature set exists on Tegra X1. Driver state/licensing/integration may block viability.

Stop/go gate: create original triangle and depth/texture tests on host and target before any Forza renderer integration. Compare actual GPU capabilities and timings. An NRO that prints text proves nothing about either graphics path.

## ADR 003: Runtime and performance

Hardware budget to measure rather than assume: Switch original shares 4 GiB physical RAM with the OS and GPU; homebrew application-mode memory allowance is below the raw 4 GiB and depends on runtime. Homebrew applet mode is much more constrained. Start with **application-mode** title takeover for eventual gameplay testing (not yet tested). CPU Cortex-A57 and Maxwell GPU differ considerably from tested PC/Adreno platforms. Require memory telemetry, thread scheduling, texture stream stress, audio underrun logs, and bounded GPU captures before targeting frame rates.

Initial *experiment* targets (NOT promises): 960x540 internal, 30 fps cap, minimum effects, portable saves. Move to 1280x720 only if measurements permit. Gameplay, AI/physics correctness, and save compatibility must be separately verified.

## ADR 004: Publication policy

No original game files, original XEX, translated proprietary machine-code-derived output, encrypted images, copyrighted screenshots/media, dumps, build packages containing recompiled game code, saves, profiles, passwords, or license keys in the public tree. Use hashes only in local ignored reports. Keep independent scaffolding and tools public; make personal builds only from content legitimately held by the user. Run audit prior to commit and review manually.
