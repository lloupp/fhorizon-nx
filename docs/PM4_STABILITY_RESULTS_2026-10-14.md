# PM4 constants / EDRAM stability — incremental delivery

**Follow-up:** [GPU accuracy comparison](READBACK_ACCURACY_RESULTS_2026-10-14.md) verifies 96/96 changing-constant frames with explicit Eden High. The failures below are historical evidence and remain preserved.

Starting commit: be0c04b0a25166805d5e22b62ff9e8e4b8e1d6f4, PR #5.
Pinned ShiftGlue 1e8968f712824b97d4f4330c5a22389c4c8be1b5 and Pinyon
a72a92a5f215a32875386b824b8b6f0f34e1caf9. SDK/compiler and official Eden
packages were restored; no old screenshots used to approve a new binary.

## A — exact corrected binary

Rebuilt be0c04b sources, explicit macro-workaround deko archive, original SDK
versions. NRO SHA256:
`f3d1ab09eb4b5b547e707e0be6cc7d377c872d49fd85ddd57660c2ba790f4969`.
The fixture builds RB_DEPTHCONTROL=0x16 through the real upstream union/LESS
enum. Executor requests DepthCompare::less and backend assigns
DkCompareOp_Less. Host ASan+UBSan includes an independent 0x16 / 0x26 check
and rejects EQUAL. No EQUAL shader/rendering support is implied.

| Attempt | Result | Pixel evidence |
|---|---|---|
| GCC 01 | 90-second timeout, not approved | Last durable event inflight_cpu_update_destroy_rejected; readback not confirmed |
| PGO 01 | Complete, process exit 0, A x3, Plus, zero resource counters, shutdown | Native indexed PM4 draw, MSAA 4x resolve and readback; uniform half-red; four capture checks passed |
| GCC 02 | Guest pixel assertion failed, process exit 0, not approved | First red/green readback passed; update expected left=ff000080 but got ff0000ff |

PGO observed process memory 68,550,656 bytes; images 7,217,152; buffers 28,672;
780 presented frames. Driver llvmpipe 25.2.8. These short executions are not a
hardware or long-duration stability measurement. Official GCC remains blocked
by timeout / constant correctness failures. Host process exit 0 is not a guest
success criterion. Historical gate exception messages can resemble pass labels;
`pm4_probe_failed` and missing shutdown make the whole run a failure.

Official package SHA256: GCC
2fae658397daf13c118082a3eb65d61a6519967b5e22e6667756baecf6000c5a;
PGO 7a28bf988b0648831989722bdbaa90ab31371b403808199813e0ea7c8b25ba6d.
All attempts are [preserved individually](evidence/pm4-stability-2026-10-14/validation-a/).

## B — controlled constants and reduced readback reproducer (unresolved)

The indexed PM4 fixture binds A / B / A, changes B each frame, and reads all
three positions. The same baseline NRO (86a58e7bf10721b85b07be9b2d3638f50f0e8370ee55883063cbd9d6a07a5c0c)
failed its second update on both official GCC and PGO: expected green 128,
read green 64. A CPU slow-memory configuration also failed. The asynchronous
GPU-disabled experiment copied an absolute SDMC path: it is retained but
excluded from an isolated-profile cohort.

Distinct UBO snapshots, command cache flush, readback lanes, uncached staging,
process cache flush, fresh readback allocation and command-memory rotation did
not eliminate the mismatch. Failed patches are archived as experiments, not
accepted runtime fixes. Process cache invalidation is unimplemented in the
inspected official Eden service implementation. There is no proven fix.

The smaller clear-only fixture alternates GPU clear and readback with no draw
or uniform bind in its GPU loop; it also diverged. Therefore this failure cannot
be attributed exclusively to dkCmdBufPushConstants. The 96-frame observation
run recorded **4 correct and 92 incorrect frames**, NRO
c9055635988102a7abac400314053977fad438c878f28506206dbe7d6a2d29c6.
Guest failure with Eden exit 0 remains a failed test. Resource queue-error guards
make queue failures explicit, but did not fix stale pixels. Root cause remains
between guest command/readback lifetime, emulator memory visibility and host
GPU behavior; no physical Switch comparison is available.

[Individual logs, summaries, failures and patches](evidence/pm4-stability-2026-10-14/constants/)
are preserved. Older attempts without RAM telemetry report null, not an estimate.

## C — bounded genuine COPY frontend

Implemented real pinned SDK register unions/enums and PM4 DRAW_INDX auto-index
rectangle-list COPY mode: RB_COPY_CONTROL, DEST_BASE/INFO/PITCH, SURFACE_INFO,
COLOR_INFO, VF0 with 8-in-32 vertex endianness, and the scissor/pixel-center
contract. Derives Fh1SurfaceKey and a native image-copy region, validates actual
backend destination metadata, rejects feedback/overlapping EDRAM ownership,
uninitialized states, unsupported formats, MSAA/conversion/clear flags, invalid
pitch/address, fractional or unaligned vertices and out-of-bounds regions.

The supported fixture copies [8,40) x [8,40) from an existing RGBA8 128x96
surface into a 64x64 RGBA8 destination. Source is a GPU clear (64,128,191,255),
not a shader gradient or a Forza command stream. Native dkCmdBufCopyImage copies
1024 pixels; readback verifies all 4096 destination pixels including untouched
(64,32,128,255). GPU readback is then CPU-packed into a bounded synthetic guest
span using the actual upstream GetTiledOffset2D function; all untouched bytes
remain 0x5a. This is not GPU tiled DMA or a mapped ShiftGlue arena.

Final COPY NRO SHA256:
33f95861312ddb23e02722bf21602e3cfea1773256f3da602e0f37effc47016d.
GCC and PGO independent executions both completed with fresh captures, every
pixel/tiled assertion, A x3, Plus, zero resource counters, shutdown and exit 0.
Observed process memory 68,554,752 bytes; images 5,062,656; buffers 12,288.
The earlier shader-gradient source failed before COPY and is retained separately.
The preceding clear variant also completed on GCC/PGO, but its captures do not
approve the final binary. EQUAL 0x26 rejection is exercised on ARM64 in this probe.

[Fresh COPY captures, GPU PPM, synthetic tiled bytes (hex), hashes and logs](evidence/pm4-stability-2026-10-14/copy/).
This subset does not implement arbitrary clamping, tiling conversions, MSAA COPY,
EDRAM ownership transfers, general formats or general game shader execution.

## D — stability classification

Attempts were sequential; each summary carries NRO hash, Eden build, driver,
guest boot, last durable event, pixel/readback result, shutdown, exit, timeout,
signal, RAM and screenshot hashes. The summary rejects partial pass markers,
black captures and guest failures even when the host process exits 0.
No new SIGSEGV/SIGABRT was observed in these completed probes; one exact-be
GCC attempt timed out. No crash root cause or long-duration stability is claimed.
The prior 3D one-in-five result is unchanged and separate from this PM4 cohort.

Two additional sequential GCC runs used the exact final COPY hash and isolated
profiles. Repeat 01 passed completely. Repeat 02 has fresh durable boot, every
pixel/tiled pass, A x3, Plus and shutdown, but eden.log has no diagnostic events;
the runner returned 5 / input_exit_verified=false. It is preserved as a collection
classification failure, not an automatic approval. Final COPY cohort: 3/4 runner
approvals, 4/4 durable guest pixel passes, 0 observed signals/timeouts. This is a
short cohort, not proof of general stability. Legacy scene-specific false fields
in the battery summary do not describe the COPY workload.

Upstream files actually inspected (under pinned shiftglue-sdk):
- src/graphics/command_processor.cpp: PM4 register/draw dispatch.
- src/graphics/fh1_edram_resolve.cpp: Fh1ResolveRectangle/Fh1PlanResolve.
- src/graphics/util/draw.cpp: general resolve helper (where applicable).
- src/graphics/vulkan/command_processor.cpp and render_target_cache.cpp.
- include/rex/graphics/fh1_edram_surfaces.h: key/pitch identity.
- src/graphics/pipeline/texture/util.cpp: actual tiled addressing.
The COPY adapter implements a strict representable subset rather than linking
the desktop memory-dependent planner or copying the Vulkan executor.

## Validation and remaining blockers

28 Python tests passed. PM4 and COPY contracts passed ASan+UBSan. Constants and
COPY NROs compiled with devkitA64/libnx/deko3d/UAM. CI is recorded in the PR when
both jobs complete; it does not stand in for Eden. SDK BSD notices are preserved
in the generated tiling overlay. Pinyon/SDK pins remain unchanged.

E is deferred because changing-constant/readback correctness is still blocked.
The next technical priority is an independent readback visibility/lifetime
reproducer and a physical Switch comparison, followed by a limited verified
Xenos-to-UAM shader path. No automatic general Xenos/SPIR-V-to-DKSH conversion,
commercial game commands, gameplay or Switch hardware execution is claimed.
