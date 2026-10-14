# Repeated PM4 constants/readback: controlled GPU accuracy comparison

Continues PR #5 from published 5dd23081bd6f3e52ee090c190fe83eb1c6bac475.
No production backend, compiler ABI, guest memory layout or game content changed.
Pinned SDK 1e8968f712824b97d4f4330c5a22389c4c8be1b5; Pinyon a72a92a5f215a32875386b824b8b6f0f34e1caf9.

## Result

Changing only the official Eden GPU accuracy from **Medium (1)** to **High (2)**
corrected the changing-constant/readback failure for this bounded fixture.
Default config records gpu_accuracy=1 and default=true. Explicit High config
contains only a Renderer section with gpu_accuracy=2 and default=false, so no
absolute SDMC paths or CPU/GPU settings are copied between profiles.

| Binary SHA256 | Setting/build | Observed result |
|---|---|---|
| c9055635988102a7abac400314053977fad438c878f28506206dbe7d6a2d29c6 | Previous Medium GCC | 4/96 correct, 92 mismatches; preserved in previous report |
| Same c9055635 | High GCC 01 / 02, PGO 02 | Each 96/96, A x3, Plus, zero resources, shutdown, process 0 |
| 0651a867cad1c79c694695ae3ec908a139e68bf1ede9f80bd1340a6a05dd71c1 | Current 1x, High GCC twice / PGO once | Each 96/96, clean input/exit/resource checks |
| Same 0651a867 | Explicit Medium GCC control | Second update expected green 64, received 32; guest failure, host exit 0 |
| d6e6d3dd401b9180d360d46e801dc8ca2e3e4a853a218774935442a07d0ee154 | Current MSAA4, High GCC | 96/96 indexed PM4 draws/resolves/readbacks, clean input/exit |
| f3d1ab09eb4b5b547e707e0be6cc7d377c872d49fd85ddd57660c2ba790f4969 | Exact be0c04b, High GCC | LESS, indexed PM4, MSAA4 resolve, correct initial and half-red updated pixels, clean exit |

RAM observed in the current constants probes: 68,546,560 bytes. Tracked GPU image
bytes: 5,574,656 (1x) / 7,213,056 (4x); buffers 36,864 before explicit destruction.
Exact be probe: 68,550,656 bytes. These are emulator observations, not hardware
performance or a claim of long-term stability.

## Separate GPU output from CPU transfers

New optional constants atlas mode stores 12 consecutive GPU outputs in distinct
320x180 cells of a 1280x540 image with native image copies before a single final
CPU readback. It does not supply expected colors as the actual GPU result.

Medium GCC clear-only and A/B/A atlas modes each verified all 12 cell references,
with fresh screenshots. In the Medium intermediate-read variant, CPU reads
lagged while all 12 GPU cells in the final atlas remained correct. That attempt
then suffered SIGSEGV (process -11) during presentation, before controls; it is
**not approved**. The crash log/report/durable GPU result are retained.

With explicit High, the assertion-enabled diagnostic variant verified every
intermediate CPU result and all 12 final GPU cells. Independent host color
checks use literal green bytes 32/64/96/159/223 and red A, sample 36 positions,
and reject black or unchanging images. A diagnostic PPM from the first High
variant was truncated despite its correct GPU readback/screenshot; it is retained
as an invalid export and is not used for pixel approval. The exporter now writes
a checked RGB block rather than ignoring many tiny fwrite results. The final
atlas run completed with fresh controls, a complete 2,073,616-byte PPM, and both
PPM/screenshot independent color checks passed. Raw PPMs are stored losslessly
as .ppm.gz with original byte counts and SHA256; the truncated file remains
explicitly invalid.

2D-engine (FlipX with CPU spatial restoration) and L2-cache barrier experiments
both still failed the second update at Medium. They are archived as failed
patches and snapshots. Neither is retained in DekoBackend. High is an explicit
emulator-test setting, not a hidden guest workaround.

## Source inspection and interpretation

Eden mirror pin 58c1e20ee58efa3900ba616207d460886214480b was inspected, without
patching or rebuilding the official executables:
- src/common/settings_enums.h: GpuAccuracy = Low, Medium, High (0/1/2).
- src/common/settings.h: default Medium and current_gpu_accuracy Medium.
- src/video_core/renderer_vulkan/vk_rasterizer.cpp: MustFlushRegion excludes
  texture-cache checks below High; FlushAndInvalidateRegion calls FlushRegion
  only at High before invalidation.
- src/video_core/engines/maxwell_dma.cpp: block-linear-to-pitch readback can use
  accelerated ImageToBuffer, with a software unswizzle fallback.
- src/video_core/buffer_cache/buffer_cache.h: download/async buffer tracking.

The same-binary comparison proves sensitivity to this precision setting for
these fixtures. The source provides a plausible cache/flush mechanism; no
instrumented call trace proves which exact flush branch caused each stale read.
No uniform buffer lifetime fix or general emulator crash fix is claimed.

## Reproduction

```sh
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
FHNX_EDEN_GPU_ACCURACY=high FHNX_TEST_PROFILE=unique-profile \
FHNX_SETTLE_SECONDS=15 scripts/eden_input_smoke.sh path/to/probe.nro .local/tools/eden-gcc-cli GRAPHICS

VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
python3 scripts/eden_stability.py path/to/probe.nro .local/tools/eden-gcc-cli \
  --label constants-high --repeats 2 --settle 15 --gpu-accuracy high
```

A fresh profile is required. The runner refuses existing report/trace evidence
instead of mixing attempts. Precision is recorded in each summary. Medium stays
available as a negative comparison; omitted precision keeps Eden's own default.
New tests do not reinterpret old failed attempts as passed.

33 Python tests and PM4/COPY ASan+UBSan passed; ARM64 NROs built using GCC16.1,
libnx4.12, deko3d0.5/UAM1.1 and the existing pinned MME workaround archive.
CI includes the atlas/intermediate and MSAA4 variants; final CI status belongs in
the PR. [Individual evidence, hashes, failures and tested source snapshots](evidence/readback-accuracy-2026-10-14/).

Remaining limits: short synthetic sequences, llvmpipe25.2.8, no physical Switch
run or gameplay. Medium coherence remains incorrect for these readbacks. One
new Medium SIGSEGV and prior 3D instability remain unresolved. Next priority:
limited Xenos shader translation / resource bindings with the verified High
validation profile, then a physical Switch coherence comparison when available.
