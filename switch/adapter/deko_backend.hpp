#pragma once
#include "commands.hpp"
#include <deko3d.h>
#include <vector>

namespace fhnx::gpu {
class DekoBackend final : public Commands {
    struct Resource { DkImage image{}; DkMemBlock memory{}; };
    struct Program { DkShader shader{}; DkMemBlock memory{}; DkStage stage{}; uint32_t generation{1}; };
    struct Storage {DkMemBlock memory{};uint32_t size{},generation{1};BufferKind kind{};};
    Images images_;
    std::array<Resource, Images::capacity> resources_{};
    std::vector<Program> programs_;
    std::array<Storage,32> buffers_{};
    uint64_t buffer_bytes_{};
    Buffer vertex_buffer_{};
    uint32_t vertex_count_{},descriptor_cursor_{};
    DkDevice device_{};
    DkQueue queue_{};
    DkCmdBuf commands_{};
    DkMemBlock command_memory_{}, descriptors_{};
    DkMemBlock readback_memory_{};
    uint32_t readback_capacity_{};
    DkSwapchain swap_{};
    Image swap_images_[2]{};
    Image color_{}, depth_{}, sampled_{};
    bool recording_{};
    uint32_t command_count_{};
    void recording();
    Storage& storage(Buffer);
    DkImageView view(Image);
public:
    explicit DekoBackend(uint64_t image_budget);
    ~DekoBackend() override;
    DekoBackend(const DekoBackend&) = delete;
    DekoBackend& operator=(const DekoBackend&) = delete;
    Image create(ImageDesc) override;
    void destroy(Image) override;
    ImageDesc imageDesc(Image h) const override {return images_.get(h).desc;}
    Shader shader(const void*, uint32_t) override;
    void begin() override;
    void transition(Image, Usage, Usage) override;
    void target(Image, Image = {}) override;
    void clear(float, float, float, float, bool = false, float = 1, uint8_t = 0) override;
    void sample(Image,Sampler = {}) override;
    Buffer buffer(BufferKind,uint32_t,const void*) override;
    void update(Buffer,uint32_t,uint32_t,const void*) override;
    void destroy(Buffer) override;
    void destroy(Shader) override;
    void vertex(Buffer,const VertexLayout&) override;
    void uniform(Buffer,uint32_t = 0) override;
    void uploadRGBA8(Image,const void*,uint32_t) override;
    void draw(const Draw&) override;
    void resolve(Image, Image) override;
    void copy(Image, Image) override;
    void copyRegion(Image,Image,CopyRegion) override;
    void submit() override;
    std::vector<uint32_t> readback(Image);
    DkSwapchain swapchain(Image, Image);
    int acquire(DkSwapchain);
    void present(DkSwapchain, int, Image);
    void destroySwapchain(DkSwapchain);
    uint64_t imageBytes() const override { return images_.used; }
    uint64_t bufferBytes() const{return buffer_bytes_;}
};
}
