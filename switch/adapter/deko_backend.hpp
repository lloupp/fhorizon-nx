#pragma once
#include "commands.hpp"
#include <deko3d.h>
#include <vector>

namespace fhnx::gpu {
class DekoBackend final : public Commands {
    struct Resource { DkImage image{}; DkMemBlock memory{}; };
    struct Program { DkShader shader{}; DkMemBlock memory{}; DkStage stage{}; };
    Images images_;
    std::array<Resource, Images::capacity> resources_{};
    std::vector<Program> programs_;
    DkDevice device_{};
    DkQueue queue_{};
    DkCmdBuf commands_{};
    DkMemBlock command_memory_{}, descriptors_{};
    DkSwapchain swap_{};
    Image swap_images_[2]{};
    Image color_{}, depth_{}, sampled_{};
    bool recording_{};
    uint32_t command_count_{};
    void recording();
    DkImageView view(Image);
public:
    explicit DekoBackend(uint64_t image_budget);
    ~DekoBackend() override;
    DekoBackend(const DekoBackend&) = delete;
    DekoBackend& operator=(const DekoBackend&) = delete;
    Image create(ImageDesc) override;
    void destroy(Image) override;
    Shader shader(const void*, uint32_t) override;
    void begin() override;
    void transition(Image, Usage, Usage) override;
    void target(Image, Image = {}) override;
    void clear(float, float, float, float, bool = false, float = 1, uint8_t = 0) override;
    void sample(Image) override;
    void draw(const Draw&) override;
    void resolve(Image, Image) override;
    void submit() override;
    std::vector<uint32_t> readback(Image);
    DkSwapchain swapchain(Image, Image);
    int acquire(DkSwapchain);
    void present(DkSwapchain, int, Image);
    void destroySwapchain(DkSwapchain);
    uint64_t imageBytes() const { return images_.used; }
};
}
