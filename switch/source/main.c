// FHNX Switch bring-up smoke. No original game code/data or Vulkan usage.
#include <switch.h>
#include <stdio.h>
#include <string.h>

static void trace(const char *event) {
    char message[128];
    int length = snprintf(message, sizeof(message), "FHNX_SMOKE %s\n", event);
    svcOutputDebugString(message, (u64)length);
}

#ifdef FHNX_RUNTIME_PROBE
extern int fhnx_runtime_probe(void);
#endif

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    consoleInit(NULL);
    trace("boot");
#ifdef FHNX_RUNTIME_PROBE
    int result = fhnx_runtime_probe();
    printf("ShiftGlue endian + 1 MiB guest arena: %s (%d)\n", result ? "FAIL" : "PASS", result);
    trace(result ? "runtime_probe_failed" : "runtime_probe_passed");
    if (result) { consoleExit(NULL); return result; }
#endif
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    printf("Forza Horizon NX - platform smoke\n");
    printf("libnx + display + input; no Forza gameplay\n");
    printf("Press A to count, + to exit.\n");
    int count = 0;
    unsigned frames = 0;
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 pressed = padGetButtonsDown(&pad);
        if (pressed & HidNpadButton_Plus) { trace("plus_exit"); break; }
        if (pressed & HidNpadButton_A) {
            printf("A input %d\n", ++count);
            trace("a_pressed");
        }
        consoleUpdate(NULL);
        if (++frames == 2) trace("frames_submitted");
    }
    consoleExit(NULL);
    trace("shutdown");
    return 0;
}
