// Independent deko3d isolation probes. No shaders, swapchain or game data.
#include <switch.h>
#include <deko3d.h>
#include <stdio.h>
#include <string.h>

#ifndef FHNX_GPU_PROBE
#error Select a GPU probe stage
#endif

static void trace(const char *event) {
    char text[128];
    int length = snprintf(text, sizeof(text), "FHNX_GRAPHICS %s\n", event);
    svcOutputDebugString(text, length);
}

int main(void) {
    trace("boot");
    DkDeviceMaker maker;
    dkDeviceMakerDefaults(&maker);
    trace("device_create_begin");
    DkDevice device = dkDeviceCreate(&maker);
    trace("device_create_end");
    if (!device) { trace("device_null"); return 2; }
#if FHNX_GPU_PROBE == 2
    DkQueueMaker queue_maker;
    dkQueueMakerDefaults(&queue_maker, device);
    queue_maker.flags = DkQueueFlags_Graphics;
    trace("queue_create_begin");
    DkQueue queue = dkQueueCreate(&queue_maker);
    trace("queue_create_end");
    if (!queue) { dkDeviceDestroy(device); return 3; }
    trace("queue_wait_begin");
    dkQueueWaitIdle(queue);
    trace("queue_wait_end");
    dkQueueDestroy(queue);
#elif FHNX_GPU_PROBE == 4
    // Bounded GPU DMA/readback test: 16 x 64 KiB, no images or shaders.
    DkQueueMaker queue_maker;
    dkQueueMakerDefaults(&queue_maker, device);
    queue_maker.flags = DkQueueFlags_Graphics;
    DkQueue queue = dkQueueCreate(&queue_maker);
    if (!queue) { dkDeviceDestroy(device); return 3; }
    const uint32_t bytes = 64 * 1024;
    DkMemBlockMaker memory_maker;
    dkMemBlockMakerDefaults(&memory_maker, device, bytes);
    memory_maker.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    DkMemBlock source = dkMemBlockCreate(&memory_maker);
    DkMemBlock target = dkMemBlockCreate(&memory_maker);
    DkMemBlock commands = dkMemBlockCreate(&memory_maker);
    if (!source || !target || !commands) { trace("allocation_failed"); return 4; }
    DkCmdBufMaker command_maker;
    dkCmdBufMakerDefaults(&command_maker, device);
    DkCmdBuf command = dkCmdBufCreate(&command_maker);
    if (!command) { trace("command_failed"); return 4; }
    dkCmdBufAddMemory(command, commands, 0, bytes);
    dkCmdBufCopyBuffer(command, dkMemBlockGetGpuAddr(source), dkMemBlockGetGpuAddr(target), bytes);
    dkCmdBufBarrier(command, DkBarrier_Full, 0);
    DkCmdList list = dkCmdBufFinishList(command);
    uint8_t *input = dkMemBlockGetCpuAddr(source);
    void *output = dkMemBlockGetCpuAddr(target);
    uint64_t start = armGetSystemTick();
    int failed = 0;
    trace("gpu_copy_begin");
    for (unsigned cycle = 0; cycle < 16; ++cycle) {
        for (unsigned i = 0; i < bytes; ++i) input[i] = (uint8_t)(i ^ (i >> 8) ^ cycle);
        memset(output, 0, bytes);
        dkQueueSubmitCommands(queue, list);
        dkQueueWaitIdle(queue);
        if (memcmp(input, output, bytes)) { failed = 1; break; }
    }
    char metrics[160];
    int length = snprintf(metrics, sizeof(metrics),
        "FHNX_GRAPHICS gpu_copy_metrics bytes=%u iterations=16 elapsed_ticks=%llu\n",
        bytes, (unsigned long long)(armGetSystemTick() - start));
    svcOutputDebugString(metrics, length);
    trace(failed ? "gpu_copy_failed" : "gpu_copy_passed");
    dkQueueDestroy(queue);
    dkCmdBufDestroy(command);
    dkMemBlockDestroy(commands);
    dkMemBlockDestroy(target);
    dkMemBlockDestroy(source);
    if (failed) { dkDeviceDestroy(device); return 5; }
#elif FHNX_GPU_PROBE == 3
    const uint32_t flags[] = {
        DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached,
        DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image,
        DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code
    };
    for (unsigned i = 0; i < sizeof(flags)/sizeof(flags[0]); ++i) {
        DkMemBlockMaker memory_maker;
        dkMemBlockMakerDefaults(&memory_maker, device, 64 * 1024);
        memory_maker.flags = flags[i];
        trace("memory_create_begin");
        DkMemBlock memory = dkMemBlockCreate(&memory_maker);
        trace("memory_create_end");
        if (!memory) { dkDeviceDestroy(device); return 4; }
        dkMemBlockDestroy(memory);
        trace("memory_destroy_end");
    }
#endif
    trace("device_destroy_begin");
    dkDeviceDestroy(device);
    trace("device_destroy_end");
    trace("probe_passed");
    trace("shutdown");
    return 0;
}
