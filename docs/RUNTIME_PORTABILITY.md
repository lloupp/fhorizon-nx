# Pinyon Shift / ShiftGlue ARM64 integration boundary

Pinned references: Pinyon Shift `a72a92a5f215a32875386b824b8b6f0f34e1caf9`, its ShiftGlue submodule `1e8968f712824b97d4f4330c5a22389c4c8be1b5`. `scripts/clone_upstream.sh` fetches those public sources under ignored `.local/` and refuses to silently replace an existing different revision.

## Implemented and executed

`scripts/build_shiftglue_probe.sh` builds `fhnx_runtime.nro` with GCC 16.1.0 C++23. `switch/runtime/runtime_probe.cpp` includes the real upstream `rex/types.h`, checks 16/32/64-bit byte swaps and the byte layout of `rex::be_u32`, then uses FHNX bounded guest memory to compare values on ARM64. A synthetic 1 MiB allocation, unaligned access and out-of-range exception passed in Eden.

Upstream `rex/platform.h` supports Windows, macOS and GNU/Linux/Android; its default branch rejects Horizon. Our script generates a **local** overlay that adds Switch OS detection without setting Linux. The original header retains its BSD copyright/license text. No upstream repository file is edited and no vendor headers or proprietary translated game code are committed here.

This is a header/component integration proof only. It does not initialize ShiftGlue's kernel, translator, VMX executor, scheduler, audio or graphics runtime.

## Remaining concrete adapters

| Boundary | Observed upstream assumption | Required Horizon work / gate |
|---|---|---|
| Guest address space | `src/system/xmemory.cpp`: reserve `0x120000000` plus granularity; physical base at virtual base + `0x100000000`; file-backed views and Linux/macOS `mmap` | Design reserve/commit/alias behavior with libnx SVCs and real memory allowance; do not allocate 4.5 GiB physically or replace pointer arithmetic with an incompatible vector |
| OS and build | OS detection excludes Switch; desktop/Android CMake configuration and libraries | Explicit Horizon toolchain configuration; audit every platform conditional rather than pretending Switch is Linux |
| CPU / VMX | ARM64 source paths exist; title-generated code remains separate | Cross-build actual representative operations, compare synthetic golden vectors and scheduling behavior |
| Input | Host input abstractions and SDL pathways | Translate normalized libnx PadState inputs to runtime state, with reconnect/disconnect tests |
| GPU | Vulkan path and desktop assumptions | Basic deko3d queue/triangle first; shader/resource translation is independent of host Vulkan support |
| Audio | SDL audio/XMA processing code | libnx sink, generated PCM fixture, completion/underrun counters; then private game decoding tests |
| Storage | Host-backed files, mapped file behavior and save/profile trees | Read-only private game root, bounded loading, path normalization, isolated writable save root |
| Dynamic libraries | Desktop plugin loading assumptions | Static linkage for Horizon; no forwarding a desktop plugin into an NRO |

## switch-nvk assessment

Inspected `HayatoG/switch-nvk` commit `6eec707da3ad5f86c64f748226583202801bfd03` README, `BUILD_AND_RUN.md`, `REPRODUCE.md` and status notes. Upstream describes a custom libnx `nwindow` WSI, hardware bring-up and zero-copy presentation. Those are **upstream reports**, not reproduced FHNX results.

The documented build needs Mesa 25.0.7, patches, Rust nightly with a Horizon sysroot, LLVM/CLC tools and static packaging; around 15 GB for the documented container workflow. Docker is unavailable in this environment. Building that stack was deferred in favor of the lightweight, compiled homebrew tests. Nothing links switch-nvk into FHNX yet.

Next target probe must record Vulkan instance/device versions, actual required features/extensions, RGBA/depth/texture formats, surface/present modes, memory types/budgets, buffer fill/readback and a visible triangle. Run it in application mode on hardware, then compare Eden. Do not equate the host's Vulkan 1.3 software driver with target support. Review the upstream GPL/Mesa component licenses before a distribution design; no game-derived binaries belong in the public repo.
