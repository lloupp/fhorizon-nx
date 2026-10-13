// Adapted from public-domain switchbrew/switch-examples deko_basic.
// Independent FHNX GPU diagnostic; no game data or translated code.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>
#include <deko3d.h>

// Define the desired number of framebuffers
#define FB_NUM 2

// Define the desired framebuffer resolution (here we set it to 720p).
#define FB_WIDTH  960
#define FB_HEIGHT 540

// Remove above and uncomment below for 1080p
//#define FB_WIDTH  1920
//#define FB_HEIGHT 1080

// Define the size of the memory block that will hold code
#define CODEMEMSIZE (64*1024)

// Define the size of the memory block that will hold command lists
#define CMDMEMSIZE (16*1024)

static DkDevice g_device;
static DkMemBlock g_framebufferMemBlock;
static DkImage g_framebuffers[FB_NUM];
static DkSwapchain g_swapchain;

static DkMemBlock g_codeMemBlock;
static uint32_t g_codeMemOffset;
static DkShader g_vertexShader;
static DkShader g_fragmentShader;

static DkMemBlock g_cmdbufMemBlock;
static DkCmdBuf g_cmdbuf;
static DkCmdList g_cmdsBindFramebuffer[FB_NUM];
static DkCmdList g_cmdsRender[2];
static unsigned g_color;
#ifdef FHNX_GPU_TEXTURE_DEPTH
static DkImage g_depthImage, g_textureImage;
static DkMemBlock g_depthMemory, g_textureMemory, g_stagingMemory, g_descriptors;
static DkCmdList g_uploadTexture;
#endif

#ifdef FHNX_GPU_DURABLE_TRACE
static FILE *g_traceFile;
#endif
static void trace(const char *event) {
    char text[128];
    int length = snprintf(text, sizeof(text), "FHNX_GRAPHICS %s\n", event);
    svcOutputDebugString(text, length);
#ifdef FHNX_GPU_DURABLE_TRACE
    if (g_traceFile) { fwrite(text, 1, length, g_traceFile); fflush(g_traceFile); }
#endif
}

static DkQueue g_renderQueue;

// DKSH blobs are linked from independent UAM-generated data, avoiding RomFS.
#ifndef FHNX_GPU_CLEAR_ONLY
#include "triangle_vsh_bin.h"
#include "color_fsh_bin.h"
static void loadShader(DkShader *shader, const void *data, uint32_t size) {
    uint32_t offset = g_codeMemOffset;
    uint32_t padded = (size + DK_SHADER_CODE_ALIGNMENT - 1) & ~(DK_SHADER_CODE_ALIGNMENT - 1);
    if (!size || padded > CODEMEMSIZE - offset) { trace("shader_budget_failed"); exit(2); }
    memcpy((uint8_t *)dkMemBlockGetCpuAddr(g_codeMemBlock) + offset, data, size);
    g_codeMemOffset += padded;
    DkShaderMaker maker;
    dkShaderMakerDefaults(&maker, g_codeMemBlock, offset);
    dkShaderInitialize(shader, &maker);
}
#endif

// This function creates all the necessary graphical resources.
static void graphicsInitialize(void)
{
    // Create the device, which is the root object
    DkDeviceMaker deviceMaker;
    dkDeviceMakerDefaults(&deviceMaker);
    trace("device_create_begin");
    g_device = dkDeviceCreate(&deviceMaker);
    trace("device_create_end");



    // Calculate layout for the framebuffers
    DkImageLayoutMaker imageLayoutMaker;
    dkImageLayoutMakerDefaults(&imageLayoutMaker, g_device);
    imageLayoutMaker.flags = DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_HwCompression;
    imageLayoutMaker.format = DkImageFormat_RGBA8_Unorm;
    imageLayoutMaker.dimensions[0] = FB_WIDTH;
    imageLayoutMaker.dimensions[1] = FB_HEIGHT;

    // Calculate layout for the framebuffers
    DkImageLayout framebufferLayout;
    trace("image_layout_begin");
    dkImageLayoutInitialize(&framebufferLayout, &imageLayoutMaker);
    trace("image_layout_end");

    // Retrieve necessary size and alignment for the framebuffers
    uint32_t framebufferSize  = dkImageLayoutGetSize(&framebufferLayout);
    uint32_t framebufferAlign = dkImageLayoutGetAlignment(&framebufferLayout);
    framebufferSize = (framebufferSize + framebufferAlign - 1) &~ (framebufferAlign - 1);

    // Create a memory block that will host the framebuffers
    DkMemBlockMaker memBlockMaker;
    dkMemBlockMakerDefaults(&memBlockMaker, g_device, FB_NUM*framebufferSize);
    memBlockMaker.flags = DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
    trace("framebuffer_block_create_begin");
    g_framebufferMemBlock = dkMemBlockCreate(&memBlockMaker);
    trace("framebuffer_block_create_end");

    // Initialize the framebuffers with the layout and backing memory we've just created
    DkImage const* swapchainImages[FB_NUM];
    for (unsigned i = 0; i < FB_NUM; i ++)
    {
        swapchainImages[i] = &g_framebuffers[i];
        dkImageInitialize(&g_framebuffers[i], &framebufferLayout, g_framebufferMemBlock, i*framebufferSize);
    }

    // Create a swapchain out of the framebuffers we've just initialized
    DkSwapchainMaker swapchainMaker;
    dkSwapchainMakerDefaults(&swapchainMaker, g_device, nwindowGetDefault(), swapchainImages, FB_NUM);
    trace("swapchain_create_begin");
    g_swapchain = dkSwapchainCreate(&swapchainMaker);
    trace("swapchain_create_end");
#ifdef FHNX_GPU_TEXTURE_DEPTH
    trace("depth_texture_create_begin");
    DkImageLayoutMaker extraMaker;
    DkImageLayout extraLayout;
    dkImageLayoutMakerDefaults(&extraMaker, g_device);
    extraMaker.flags = DkImageFlags_UsageRender;
    extraMaker.format = DkImageFormat_ZF32;
    extraMaker.dimensions[0] = FB_WIDTH;
    extraMaker.dimensions[1] = FB_HEIGHT;
    dkImageLayoutInitialize(&extraLayout, &extraMaker);
    dkMemBlockMakerDefaults(&memBlockMaker, g_device, (dkImageLayoutGetSize(&extraLayout) + DK_MEMBLOCK_ALIGNMENT - 1) & ~(DK_MEMBLOCK_ALIGNMENT - 1));
    memBlockMaker.flags = DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
    g_depthMemory = dkMemBlockCreate(&memBlockMaker);
    dkImageInitialize(&g_depthImage, &extraLayout, g_depthMemory, 0);
    dkImageLayoutMakerDefaults(&extraMaker, g_device);
    extraMaker.flags = DkImageFlags_Usage2DEngine;
    extraMaker.format = DkImageFormat_RGBA8_Unorm;
    extraMaker.dimensions[0] = 16;
    extraMaker.dimensions[1] = 16;
    dkImageLayoutInitialize(&extraLayout, &extraMaker);
    dkMemBlockMakerDefaults(&memBlockMaker, g_device, (dkImageLayoutGetSize(&extraLayout) + DK_MEMBLOCK_ALIGNMENT - 1) & ~(DK_MEMBLOCK_ALIGNMENT - 1));
    memBlockMaker.flags = DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
    g_textureMemory = dkMemBlockCreate(&memBlockMaker);
    dkImageInitialize(&g_textureImage, &extraLayout, g_textureMemory, 0);
    dkMemBlockMakerDefaults(&memBlockMaker, g_device, DK_MEMBLOCK_ALIGNMENT);
    memBlockMaker.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    g_stagingMemory = dkMemBlockCreate(&memBlockMaker);
    uint32_t *checker = dkMemBlockGetCpuAddr(g_stagingMemory);
    for (unsigned y = 0; y < 16; ++y)
        for (unsigned x = 0; x < 16; ++x)
            checker[y*16+x] = ((x/2+y/2)&1) ? 0xff30c0ff : 0xff202020;
    g_descriptors = dkMemBlockCreate(&memBlockMaker);
    DkImageView textureView;
    dkImageViewDefaults(&textureView, &g_textureImage);
    dkImageDescriptorInitialize(dkMemBlockGetCpuAddr(g_descriptors), &textureView, false, false);
    DkSampler sampler;
    dkSamplerDefaults(&sampler);
    dkSamplerDescriptorInitialize((DkSamplerDescriptor *)((uint8_t *)dkMemBlockGetCpuAddr(g_descriptors)+128), &sampler);
    trace("depth_texture_create_end");
#endif

    // Create a memory block onto which we will load shader code
    dkMemBlockMakerDefaults(&memBlockMaker, g_device, CODEMEMSIZE);
    memBlockMaker.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code;
#ifndef FHNX_GPU_CLEAR_ONLY
    trace("shader_block_create_begin");
    g_codeMemBlock = dkMemBlockCreate(&memBlockMaker);
    trace("shader_block_create_end");
    g_codeMemOffset = 0;

    // Load our shaders (both vertex and fragment)
    trace("shader_init_begin");
    loadShader(&g_vertexShader, triangle_vsh_bin, triangle_vsh_bin_size);
    loadShader(&g_fragmentShader, color_fsh_bin, color_fsh_bin_size);
    trace("shader_init_end");
#endif

    // Create a memory block which will be used for recording command lists using a command buffer
    dkMemBlockMakerDefaults(&memBlockMaker, g_device, CMDMEMSIZE);
    memBlockMaker.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    trace("cmd_memory_create_begin");
    g_cmdbufMemBlock = dkMemBlockCreate(&memBlockMaker);
    trace("cmd_memory_create_end");

    // Create a command buffer object
    DkCmdBufMaker cmdbufMaker;
    dkCmdBufMakerDefaults(&cmdbufMaker, g_device);
    trace("cmd_buffer_create_begin");
    g_cmdbuf = dkCmdBufCreate(&cmdbufMaker);
    trace("cmd_buffer_create_end");

    // Feed our memory to the command buffer so that we can start recording commands
    dkCmdBufAddMemory(g_cmdbuf, g_cmdbufMemBlock, 0, CMDMEMSIZE);
#ifdef FHNX_GPU_TEXTURE_DEPTH
    DkImageView textureViewCopy;
    dkImageViewDefaults(&textureViewCopy, &g_textureImage);
    DkCopyBuf source = { dkMemBlockGetGpuAddr(g_stagingMemory), 0, 0 };
    DkImageRect destination = { 0, 0, 0, 16, 16, 1 };
    dkCmdBufCopyBufferToImage(g_cmdbuf, &source, &textureViewCopy, &destination, 0);
    dkCmdBufBarrier(g_cmdbuf, DkBarrier_Full, DkInvalidateFlags_Image);
    g_uploadTexture = dkCmdBufFinishList(g_cmdbuf);
#endif

    // Generate a command list for each framebuffer, which will bind each of them as a render target
    trace("render_command_record_begin");
    for (unsigned i = 0; i < FB_NUM; i ++)
    {
        DkImageView imageView;
        dkImageViewDefaults(&imageView, &g_framebuffers[i]);
#ifdef FHNX_GPU_TEXTURE_DEPTH
        DkImageView depthView;
        dkImageViewDefaults(&depthView, &g_depthImage);
        dkCmdBufBindRenderTarget(g_cmdbuf, &imageView, &depthView);
#else
        dkCmdBufBindRenderTarget(g_cmdbuf, &imageView, NULL);
#endif
        g_cmdsBindFramebuffer[i] = dkCmdBufFinishList(g_cmdbuf);
    }

    // Declare structs that will be used for binding state
    DkViewport viewport = { 0.0f, 0.0f, (float)FB_WIDTH, (float)FB_HEIGHT, 0.0f, 1.0f };
    DkScissor scissor = { 0, 0, FB_WIDTH, FB_HEIGHT };
#ifndef FHNX_GPU_CLEAR_ONLY
    DkShader const* shaders[] = { &g_vertexShader, &g_fragmentShader };
#endif
    DkRasterizerState rasterizerState;
    DkColorState colorState;
    DkColorWriteState colorWriteState;

    // Initialize state structs with the deko3d defaults
    dkRasterizerStateDefaults(&rasterizerState);
    dkColorStateDefaults(&colorState);
    dkColorWriteStateDefaults(&colorWriteState);

    // Pre-record two palettes; A changes background without rebuilding resources.
    for (unsigned palette = 0; palette < 2; ++palette) {
    // Generate the main rendering command list
    dkCmdBufSetViewports(g_cmdbuf, 0, &viewport, 1);
    dkCmdBufSetScissors(g_cmdbuf, 0, &scissor, 1);
    dkCmdBufClearColorFloat(g_cmdbuf, 0, DkColorMask_RGBA, palette ? 0.45f : 0.025f, palette ? 0.08f : 0.06f, 0.12f, 1.0f);
#ifndef FHNX_GPU_CLEAR_ONLY
    dkCmdBufBindShaders(g_cmdbuf, DkStageFlag_GraphicsMask, shaders, sizeof(shaders)/sizeof(shaders[0]));
#endif
    dkCmdBufBindRasterizerState(g_cmdbuf, &rasterizerState);
    dkCmdBufBindColorState(g_cmdbuf, &colorState);
    dkCmdBufBindColorWriteState(g_cmdbuf, &colorWriteState);
#ifdef FHNX_GPU_TEXTURE_DEPTH
    DkDepthStencilState depthState;
    dkDepthStencilStateDefaults(&depthState);
#ifdef FHNX_GPU_DISABLE_DEPTH
    depthState.depthTestEnable = false;
#endif
    dkCmdBufBindDepthStencilState(g_cmdbuf, &depthState);
    dkCmdBufClearDepthStencil(g_cmdbuf, true, 1.0f, 0, 0);
    dkCmdBufBindImageDescriptorSet(g_cmdbuf, dkMemBlockGetGpuAddr(g_descriptors), 1);
    dkCmdBufBindSamplerDescriptorSet(g_cmdbuf, dkMemBlockGetGpuAddr(g_descriptors)+128, 1);
    DkResHandle texture = dkMakeTextureHandle(0, 0);
    dkCmdBufBindTextures(g_cmdbuf, DkStage_Fragment, 0, &texture, 1);
    // Near triangle first, far triangle second: the latter must not overwrite it.
    dkCmdBufDraw(g_cmdbuf, DkPrimitive_Triangles, 3, 1, 0, 0);
    dkCmdBufDraw(g_cmdbuf, DkPrimitive_Triangles, 3, 1, 3, 0);
#elif !defined(FHNX_GPU_CLEAR_ONLY)
    dkCmdBufDraw(g_cmdbuf, DkPrimitive_Triangles, 3, 1, 0, 0);
#endif
    g_cmdsRender[palette] = dkCmdBufFinishList(g_cmdbuf);
    }

    trace("render_command_record_end");


    // Create a queue, to which we will submit our command lists
    DkQueueMaker queueMaker;
    dkQueueMakerDefaults(&queueMaker, g_device);
    queueMaker.flags = DkQueueFlags_Graphics;
    trace("queue_create_begin");
    g_renderQueue = dkQueueCreate(&queueMaker);
    trace("queue_create_end");
#ifdef FHNX_GPU_TEXTURE_DEPTH
    trace("texture_upload_begin");
    dkQueueSubmitCommands(g_renderQueue, g_uploadTexture);
    dkQueueWaitIdle(g_renderQueue);
    trace("texture_upload_end");
#endif
}

// This function is to be called at each frame, and it is in charge of rendering.
static void graphicsUpdate(void)
{
    // Acquire a framebuffer from the swapchain (and wait for it to be available)
    static int first_frame = 1;
    if (first_frame) trace("first_acquire_begin");
    int slot = dkQueueAcquireImage(g_renderQueue, g_swapchain);
    if (first_frame) trace("first_acquire_end");

    // Run the command list that binds said framebuffer as a render target
    if (first_frame) trace("first_bind_submit_begin");
    dkQueueSubmitCommands(g_renderQueue, g_cmdsBindFramebuffer[slot]);
    if (first_frame) trace("first_bind_submit_end");

    // Run the main rendering command list
    if (first_frame) trace("first_render_submit_begin");
    dkQueueSubmitCommands(g_renderQueue, g_cmdsRender[g_color]);
    if (first_frame) trace("first_render_submit_end");

    // Now that we are done rendering, present it to the screen
    if (first_frame) trace("first_present_begin");
    dkQueuePresentImage(g_renderQueue, g_swapchain, slot);
    if (first_frame) trace("first_present_end");
    first_frame = 0;
}

// This function destroys the graphical resources created by graphicsInitialize.
static void graphicsExit(void)
{
    // Make sure the rendering queue is idle before destroying anything
    dkQueueWaitIdle(g_renderQueue);

    // Destroy all the resources we've created
    dkQueueDestroy(g_renderQueue);
    dkCmdBufDestroy(g_cmdbuf);
    dkMemBlockDestroy(g_cmdbufMemBlock);
#ifndef FHNX_GPU_CLEAR_ONLY
    dkMemBlockDestroy(g_codeMemBlock);
#endif
    dkSwapchainDestroy(g_swapchain);
    dkMemBlockDestroy(g_framebufferMemBlock);
#ifdef FHNX_GPU_TEXTURE_DEPTH
    dkMemBlockDestroy(g_descriptors);
    dkMemBlockDestroy(g_stagingMemory);
    dkMemBlockDestroy(g_textureMemory);
    dkMemBlockDestroy(g_depthMemory);
#endif
    dkDeviceDestroy(g_device);
}

// Main entrypoint
int main(int argc, char* argv[])
{
#ifdef FHNX_GPU_DURABLE_TRACE
    // Optional diagnostic I/O survives a host abort that drops async log messages.
    g_traceFile = fopen("sdmc:/fhnx_gpu_checkpoints.log", "w");
#endif
    trace("boot");
    graphicsInitialize();
    trace("resources_ready");
    unsigned frames = 0;

    // Configure our supported input layout: a single player with standard controller styles
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    // Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
    PadState pad;
    padInitializeDefault(&pad);

    while (appletMainLoop())
    {
        padUpdate(&pad);

        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_A) { g_color ^= 1; trace("a_pressed"); }
        if (kDown & HidNpadButton_Plus) { trace("plus_exit"); break; }

        graphicsUpdate();
        if (++frames == 2) trace("frames_submitted");
    }

    graphicsExit();
    trace("shutdown");
    return 0;
}
