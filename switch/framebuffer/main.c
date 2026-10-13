// Original CPU raster baseline. This is NOT a deko3d/Vulkan GPU test.
#include <switch.h>
#include <stdio.h>
#include <stdint.h>

enum { Width = 320, Height = 180 };
static void trace(const char *event) {
    char text[128];
    int size = snprintf(text, sizeof(text), "FHNX_GRAPHICS %s\n", event);
    svcOutputDebugString(text, size);
}

static void draw(uint32_t *pixels, unsigned pitch, int palette) {
    for (unsigned y = 0; y < Height; ++y) {
        for (unsigned x = 0; x < Width; ++x) {
            uint32_t color = palette ? 0xff203070 : 0xff201008;
            // Integer barycentric gradient within a synthetic triangle.
            int dy = (int)y - 24;
            int dx = (int)x - Width / 2;
            if (dy >= 0 && dy <= 132 && dx >= -dy && dx <= dy) {
                unsigned red = (132 - dy) * 255 / 132;
                unsigned green = (dy - dx) * 255 / 264;
                unsigned blue = (dy + dx) * 255 / 264;
                color = 0xff000000 | (blue << 16) | (green << 8) | red;
            }
            pixels[y * pitch + x] = color;
        }
    }
}

int main(void) {
    Framebuffer fb;
    Result result = framebufferCreate(&fb, nwindowGetDefault(), Width, Height,
                                      PIXEL_FORMAT_RGBA_8888, 2);
    if (R_FAILED(result)) { trace("framebuffer_failed"); return 2; }
    framebufferMakeLinear(&fb);
    trace("boot");
    trace("cpu_framebuffer_backend");
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    unsigned frames = 0;
    int palette = 0;
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 pressed = padGetButtonsDown(&pad);
        if (pressed & HidNpadButton_A) { palette ^= 1; trace("a_pressed"); }
        if (pressed & HidNpadButton_Plus) { trace("plus_exit"); break; }
        u32 stride;
        uint32_t *pixels = framebufferBegin(&fb, &stride);
        draw(pixels, stride / sizeof(*pixels), palette);
        framebufferEnd(&fb);
        if (++frames == 2) trace("frames_submitted");
        svcSleepThread(16000000);
    }
    framebufferClose(&fb);
    trace("shutdown");
    return 0;
}
