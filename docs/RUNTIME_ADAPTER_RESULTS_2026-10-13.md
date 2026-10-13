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

## B — Sparse Horizon mapping prototype; desktop semantics blocked

`switch/runtime/horizon_memory.*` implements a bounded, sparse virtual reservation
using actual libnx `virtmemFindStack`/`virtmemAddReservation`, one 4096-byte backing
heap page per commit and `svcMapMemory`/`svcUnmapMemory`. Reservation is a libnx
address-manager bookkeeping operation, not a kernel PROT_NONE mmap. Reserved
addresses stay unmapped until commit. The abstraction is single-threaded, with
no signals/MMIO/exception integration, and cannot replace upstream `Memory` yet.

The upstream `src/system/xmemory.cpp` reserves `0x120000000` plus granularity,
uses virtual-base + 4 GiB for physical memory, and shares file-backed views of
the lower 512 MiB across multiple guest aliases. Those pointer arithmetic and
coherent aliases are **not provided** by this prototype. Horizon `svcMapMemory`
moves access from heap source into the stack region (source becomes inaccessible),
so exposing it as a simultaneous writable alias would be incorrect. `alias()`
returns unsupported rather than duplicating pages and losing coherence.

Final execution: official Eden 0.2.1 PGO, ARM64 NRO with explicitly bounded
32 MiB libnx heap, peak 8192 bytes of backing allocated by this adapter:

| Gate | Result |
| --- | --- |
| Reserve 1 MiB, zero guest backing committed | PASS |
| Commit two pages, zero initialization, unaligned BE value across page boundary | PASS |
| Out-of-budget third page, misaligned/duplicate commit, uncommitted access | Rejected without increasing committed bytes |
| Writable alias | Explicitly unsupported; tested rejection |
| Change mapped page to read-only | Kernel rejects with `0x0000D401`; queried permission remains `Perm_Rw` |
| Failed permission change | Return real Result code and retain prior metadata; never claim hardware protection |
| Change mapped page to no access | Kernel rejects; no-access capability remains blocked |
| Decommit, kernel query of unmapped page, zero-filled recommit | PASS |
| Cross-page write into an uncommitted page | Rejected before any byte changes |
| Explicit page reclamation, destructor reservation release, A x3 and Plus | PASS; process exit 0 |
| Desktop 4.5 GiB reservation | Rejected before allocation; this profile's stack region is 2 GiB |

The experiment queries process allowance 3,444,572,160 bytes and process-used
34,885,632 bytes after reclamation. These are emulated kernel accounting,
not host RSS or physical Switch measurements. The intermediate run with default
libnx heap showed about 3.44 GiB of process-used accounting: that was the library's
default heap sizing, not this adapter committing the guest arena. The final
32 MiB cap avoids this ambiguity; both outputs are preserved.

Evidence: [memory-2026-10-13](evidence/memory-2026-10-13/). The first test stopped
on denied protection; that failure is preserved. The final test succeeds only
as a **bounded prototype with explicit unsupported capabilities**, not as a
complete ShiftGlue virtual-memory implementation. Raw pointer access protection,
coherent physical aliases and a compatible contiguous full arena remain blockers.
Do not continue into title execution with these missing semantics.

Reproduce:

```sh
DEVKITPRO=/opt/devkitpro bash scripts/build_horizon_memory.sh
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json FHNX_SETTLE_SECONDS=10 \
  bash scripts/eden_input_smoke.sh .local/horizon-memory/fhnx_horizon_memory.nro \
  "$PWD/.local/tools/eden-cli" SMOKE
```

Milestone A was published as `318bf4b6c3d4026e9b5c15ae034a920f5cd8d660` in
[PR #3](https://github.com/lloupp/fhorizon-nx/pull/3). Its two CI jobs passed
(PR workflow run 38448049087). Publication used the connected GitHub Git-object
API after local `git push` reported no HTTPS credentials. The published tree
matches local tree `035a0b5b49ab1598d6514140fa92e0c431444be5` exactly; only
commit author/time metadata differs. Subsequent publication preserves that parent.

## C — Additional pinned ShiftGlue VMX/SIMDe operations executed on ARM64

`switch/runtime/vmx_probe.cpp` includes the actual pinned `rex/ppc/context.h`
and `rex/ppc/intrinsics.h`. Volatile input arrays are copied at runtime into
vector operands. Named non-inlined operation wrappers prevent substituting
constant answers. Fixed independent reference arrays encode expected bytes,
saturated lanes, compare masks and float bit patterns. This adds component tests,
not a title VMX interpreter or complete instruction coverage.

| Executed operation | Reference / boundary |
| --- | --- |
| Upstream two-vector byte permutation | Guest-to-host reversed byte order, both source vectors, low-five-bit controls including 255/32/47/63 |
| Unsigned byte saturation | Overflow to 255 and non-overflow lanes |
| Signed halfword saturation | Both signed limits, normal positive/negative lanes |
| Upstream unsigned byte comparison | Lane masks 0/255 including values above 127 |
| Unaligned vector load/store | Actual non-inlined 128-bit transfer at displacements 1 through 15 |
| Byte order | Four-byte word reversal via runtime vector shuffle |
| Upstream signed/unsigned saturating conversions | Fractional truncation, signed overflow/underflow, negative-to-unsigned, UINT_MAX saturation, unsigned NaN-to-zero |
| Upstream unsigned-int-to-float | Exact reference IEEE bits for 0, 1, 2^31 and UINT_MAX |
| Upstream guest-order dot3/dot4 | Reversed guest lanes and independently computed broadcast values 170/184 |
| Context/FPCR | Four rounding mode round trips, vector v14/v31 and GPR/FPR nonvolatile preservation |

Official Eden PGO final execution passed all eight logged groups, booted,
registered three A events and exited via Plus with code 0. Local Switch-only FPCR
overlay remains in the build script; upstream headers are unchanged.

Saved object disassembly shows `tbl`, `uqadd`, `sqadd`, `cmgt`, `fcvtzs`, `scvtf`,
floating arithmetic, `ldr/str q` and FPCR MRS/MSR in the relevant operations.
`tools/check_vmx_object.py` automatically verifies thirteen object-code gates,
also added to CI. Object inspection is supplementary to runtime pass events;
the test does not claim hardware execution or guest exception/saturation flags.
The first object inspection correctly rejected the identity alignment copy:
GCC had lowered it to paired scalar `ldp/stp`. A SIMD register constraint now
forces vector `ldr/str q` without supplying any computed answer. The intermediate
disassembly and failed instruction gate are preserved; the final NRO was rebuilt
and executed again after this change.

Evidence: [vmx-2026-10-13](evidence/vmx-2026-10-13/), with NRO/object hashes,
build/run logs, capture, object disassembly and instruction gate results.
Run `bash scripts/build_shiftglue_probe.sh vmx`, then execute the resulting
`.local/switch-vmx/fhnx_vmx.nro` with `eden_input_smoke.sh ... SMOKE`.

## D and next real blockers

The 3D track/vehicle demonstration is deferred. B exposes actual memory semantic
blockers, so these components cannot yet be called a solid complete runtime.
No placeholder 3D demo or complete-renderer claim was added to conceal that gap.

1. Select a genuine Horizon address-space/alias/protection strategy preserving
   ShiftGlue's translated-code pointer ABI; test it on hardware in application mode.
2. Connect a **bounded subset** of Pinyon's frontend draw/resolve operations to
   `Commands`, derive surface identity from guest state, and reject unsupported
   EDRAM/format/resolve semantics. The actual Pinyon executor still is not connected.
3. Add reusable vertex/index/uniform buffer and descriptor arenas, DKSH shader
   conversion and asynchronous fenced resource reuse before attempting a 3D client.
4. Expand VMX tests to edge cases, status flags and guest exception behavior.
   Current tests prove only the enumerated representative operations.
5. Validate these new adapters with stock deko on modified Eden, then physical
   Switch; this execution used official Eden plus the isolated workaround.
