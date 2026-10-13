# Runtime continuation: reusable deko3d command backend

Base: `0a60de4e3601e9d05f9ace52f9345fab1347389a`, continuation branch
`feat/runtime-deko-adapter-20261013`. PR #2 and all its evidence remain unchanged.
No ISO, XEX, translated title code, firmware or keys were used.

## A — Implemented boundary and executed evidence

`src/graphics/commands.hpp` defines a target-neutral `Commands` interface,
image descriptions, generation-checked handles, explicit usage states, a bounded
image registry and draw state. `switch/adapter/deko_backend.*` implements it with
real deko3d calls, not a CPU rasterizer. `switch/adapter/main.cpp` is a synthetic
executor client using that interface. The backend can be used by a future Pinyon
frontend, but **is not wired into Pinyon's command processor**.

Implemented: create/destroy RGBA8 and Z24S8 images; 1x/4x matching attachments;
clear; triangle draw with DKSH shaders, depth test/write, stencil replace/equal,
color mask; one sampled image per batch; explicit 4x color resolve; conservative
full barriers with image invalidation; submit/wait; GPU image-to-buffer readback;
swapchain presentation. Resource handles reject stale/double destruction and
budget overflow. Swapchain-owned image destruction is rejected. Cleanup waits
for the queue and reclaims image allocations. Shader objects live until backend
destruction; no shader eviction or replacement is implemented.

New executions on official Eden 0.2.1 PGO, using the previously pinned separate
explicit-macro-upload deko3d archive and host Mesa llvmpipe:

| Execution | Observed result |
| --- | --- |
| Positive synthetic command flow | Depth preserves the near white triangle; stencil masks the far blue triangle; 4x resolve and GPU readback verify orange/blue pixels; resolved texture sampled in second pass |
| Omitted resolve control | Destination remains green in GPU readback and presentation |
| Disabled stencil control | Blue appears outside the mask in readback; captured blue area grows and orange area shrinks |
| All three final executions | Boot, A x3, frame submission, Plus shutdown, emulator exit 0, no timeout |
| Resource accounting | 2,293,760 image bytes during MSAA fixture; 5,242,880 with destination and two presentation images; 0 after explicit destruction |
| Host graphics contract | Budget, unsupported samples, state transitions, attachment checks, resolve checks and stale handles passed in Release build |
| Captured pixel comparison | Ten checks passed; checker rejects positive captures supplied as their own negative controls |

These byte counts are **owned image allocations**, not whole-process RAM or a
hardware performance budget. Frame counts/ticks are emulator observations; no
Switch frame-rate claim is made. CPU/GPU synchronization deliberately waits
between batches and uses full barriers. This is a correctness-first boundary.

Evidence: [adapter-2026-10-13](evidence/adapter-2026-10-13/), including build logs,
process reports, raw emulator logs, captures, pixel comparison and hashes.

![Adapter resolved target sampled in the presentation pass](evidence/adapter-2026-10-13/positive-boot.png)

The first attempt expected white instead of the far shader's blue tint; its
failed readback check is preserved in `failed-first`. An early concurrently
launched control did not reach boot before automation targeted a closed SDL
window; it is preserved in `failed-concurrent-control`. Final runs execute
sequentially with a ten-second startup settle. Intermediate successful output
before the additional depth and ownership gates is preserved separately.

## Inspected upstream and mapping

Pinyon pin `a72a92a5f215a32875386b824b8b6f0f34e1caf9`, ShiftGlue pin
`1e8968f712824b97d4f4330c5a22389c4c8be1b5` (fetched with the existing script).
Within the SDK, `include/rex/graphics/vulkan/fh1_native_executor.h` and
`src/graphics/vulkan/fh1_native_executor.cpp` expose the concrete boundary:

| Pinyon/ShiftGlue executor concept | Initial FHNX counterpart | Still missing |
| --- | --- | --- |
| Surface image/memory/views, dimensions, format, sample count and byte accounting | `ImageDesc`, image registry and owned deko image/memblock | Xenos format conversion, view reinterpretation, surface cache and EDRAM ownership |
| Surface layout/stage/access and `Transition` | `Usage` plus full deko barrier/image invalidation | Per-stage fine-grained hazard tracking and dependency batching |
| `PrepareTargets`, `BindTargets`, `BeginDrawRendering` | Validated color/depth handles, sample-count matching, target binding | Deriving attachments from actual guest registers and `Fh1SurfaceKey` |
| Shader modules, transfer pipeline keys and SPIR-V | UAM-produced original DKSH, vertex/fragment stage checks, bounded program storage | Guest shader translation, constants, vertex/index buffers, pipeline cache and SPIR-V-to-DKSH path |
| `NativeResolve`, copy plans, tile sources, readback queues | Whole-image RGBA8 4x-to-1x resolve and blocking GPU readback | Guest sample selection, endian/format conversion, partial regions, depth resolve, EDRAM tiling and guest-memory writes |

`include/rex/graphics/fh1_edram_surfaces.h` provides API-independent
`Fh1SurfaceKey` (base tiles, pitch tiles, MSAA, depth flag, storage format).
That identity can eventually drive a cache above this backend. It is not
currently reproduced or imported into a synthetic surrogate claiming EDRAM
compatibility. Arbitrary Xbox traffic is rejected by scope, not silently emulated.

## Reproduce A

```sh
export DEVKITPRO=/opt/devkitpro
export PATH="$HOME/.local/bin:$PATH"
./scripts/setup_eden_linux.sh
./scripts/prepare_deko_eden_workaround.sh
export FHNX_DEKO_LIBRARY="$PWD/.local/deko-eden/deko3d/lib/libdeko3dd.a"
bash scripts/build_deko_adapter.sh
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json FHNX_SETTLE_SECONDS=10 \
  bash scripts/eden_input_smoke.sh .local/adapter-positive/fhnx_adapter_positive.nro \
  "$PWD/.local/tools/eden-cli" GRAPHICS
# Separate variants: FHNX_ADAPTER_VARIANT=no-resolve / no-stencil,
# FHNX_ADAPTER_DEFINES=-DFHNX_ADAPTER_NO_RESOLVE / -DFHNX_ADAPTER_NO_STENCIL.
python3 tools/check_adapter_pixels.py positive-boot.png positive-after-a.png \
  no-resolve-boot.png no-stencil-boot.png
cmake -S . -B .local/adapter-host -DCMAKE_BUILD_TYPE=Release
cmake --build .local/adapter-host --target fhnx_graphics_contract
ctest --test-dir .local/adapter-host -R graphics_contract --output-on-failure
```

The new CI step compiles the adapter and validates the NRO header; it does not
execute Eden. Modified Eden and stock deko were not available in this fresh
checkout; the official/workaround profile was sufficient for this milestone.
Previously demonstrated modified-Eden behavior is not claimed as a new run.
Physical Switch and Forza gameplay remain unvalidated.
