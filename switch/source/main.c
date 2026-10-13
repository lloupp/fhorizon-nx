// FHNX Switch bring-up smoke. No original game code/data or Vulkan usage.
#include <switch.h>
#include <stdio.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    printf("Forza Horizon NX - platform smoke\n");
    printf("libnx + display + input; no Forza gameplay\n");
    printf("Press A to count, + to exit.\n");
    int count = 0;
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 pressed = padGetButtonsDown(&pad);
        if (pressed & HidNpadButton_Plus) break;
        if (pressed & HidNpadButton_A) printf("A input %d\n", ++count);
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return 0;
}
