# PM4 constants / EDRAM stability — incremental delivery

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

## Pending increments

B: controlled A/B/A indexed PM4 fixture with 64 frame readbacks, no depth test,
separate from the 3D demonstration. Compilation has completed; guest execution
is in progress. No speculative constant/cache correction is accepted.
C: real COPY-mode register / vertex-fetch derivation and bounded transfers are
not yet implemented in this checkpoint. Native resolve alone is not COPY.
D: retain timeouts, guest pixel failures and emulator signals separately.
E: deferred until the preceding contracts are sufficiently stable.

The existing be0c04b CI passed both jobs (38553900731); this evidence/runner
checkpoint's CI is pending. No game traces, commercial assets or hardware
claims. Main and previous PR history remain intact.
