# deko3d GPU initialization crash — reproducible isolation plan

Date: 2026-10-13. **Historical initial isolation plan. The investigation has now produced passing GPU tests; see [executed results and correction](GPU_RESULTS_2026-10-13.md).**

Baseline: Forza Horizon NX PR #1 at `4ae77a3ddba94eb324cfa6c681c74ef653f63aca` compiles an independent `fhnx_graphics.nro`, but Eden 0.2.1 aborts before first visible deko3d frame with `std::length_error: vector::_M_default_append` or `std::bad_alloc`. CPU framebuffer, console/input, and ShiftGlue header/arena probes pass. No Switch hardware has been used.

## Code change

Added low-volume `svcOutputDebugString` markers surrounding all large deko3d init calls, resource creation, queue creation, and each operation of the first frame. All markers start with `FHNX_GRAPHICS` and use distinct begin/end pairs, so a captured emulator log can identify the last completed phase. This does **not** fix the abort.

## Capture sequence

1. Build this branch with a valid devkitPro toolchain using `./scripts/build_switch_graphics.sh`.
2. Confirm output `.local/switch-graphics/fhnx_graphics.nro` exists and record SHA256.
3. Run Eden using the existing `scripts/eden_input_smoke.sh` or `scripts/test_eden.py` instrumentation with at least `Service.NVDRV` debug logging. Do not mark a boot pass based solely on a process starting.
4. Preserve the last 100 log lines and stack/backtrace of the **host emulator** crash privately under `.local/reports`. Do not publish a raw log containing private paths, game files or keys. Create a sanitized excerpt for review.
5. Identify the *earliest* `FHNX_GRAPHICS *_begin` that has no corresponding `*_end`. If all `*_end` markers appear, continue to the first-frame acquire/submit/present markers; check asynchronous GPU worker exceptions via host stack.
6. Repeat with the same NRO SHA on the official PGO and GCC builds and log emulator build SHA and Vulkan driver. A change between emulators suggests host implementation differences, not guaranteed guest correctness.
7. When Switch hardware is available, test the identical NRO there. Do not infer target feasibility from host llvmpipe.

## Interpreting ioctl 0xC010480B

Switchbrew's NV service table identifies `0xC010480B` as `NVGPU_IOCTL_CHANNEL_ZCULL_BIND`, not general queue initialization or submit. In the current Eden source `nvhost_gpu::ZCullBind` simply stores `zcull_params` and reports success. The original crash's last **fully printed** ioctl can precede an exception from an asynchronous GPU worker or a later call. No root cause is confirmed.

## Suggested next minimal experiments

- **Device-only NRO**: `dkDeviceCreate` and `dkDeviceDestroy`, no framebuffers, queues or shaders.
- **Queue-only NRO**: add `dkQueueCreate` / `dkQueueDestroy`, no swapchain, shaders or cmd buffers. Record `FHNX_GRAPHICS queue_create_begin/end`.
- **Swapchain-only NRO**: add images and swapchain without shaders or draw.
- **Triangle NRO**: existing sample, same shader blobs across versions.

Keep each NRO in its own program and only change one subsystem between runs. Do not use `return 0` as a substitute for a visible graphics confirmation.

CI compilation is useful for catching libnx API/compiler regressions; CI cannot validate Eden unless the emulator is actually installed and launched under suitable conditions. Preserve PR #1 and do not merge automatically.
