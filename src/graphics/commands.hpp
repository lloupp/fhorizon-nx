#pragma once
// Target-neutral boundary for a future native executor. No Vulkan or deko types.
#include <array>
#include <cstdint>
#include <stdexcept>

namespace fhnx::gpu {
enum class Format { rgba8, z24s8 };
enum class Usage { undefined, target, sampled, resolve_source, resolve_dest, readback, present, copy_source, copy_dest };
enum class Stencil { disabled, replace, equal };
struct ImageDesc {
    uint32_t width{}, height{}, samples{1};
    Format format{Format::rgba8};
    bool present{};
};
struct Image { uint32_t slot{UINT32_MAX}, generation{}; };
struct Shader { uint32_t slot{UINT32_MAX}, generation{1}; };
enum class BufferKind {vertex, index16, uniform};
struct Buffer {uint32_t slot{UINT32_MAX},generation{};};
struct Attribute {uint32_t offset{},components{};};
struct VertexLayout {uint32_t stride{},count{};std::array<Attribute,8> attributes{};};
enum class Filter {nearest,linear};
enum class Wrap {repeat,clamp};
struct Sampler {Filter filter{Filter::nearest};Wrap wrap{Wrap::repeat};};
struct Draw {
    Shader vertex, fragment;
    uint32_t first{}, count{3};
    bool depth_test{}, depth_write{}, color_write{true};
    Stencil stencil{Stencil::disabled};
    uint8_t reference{1};
    Buffer indices{};
    int32_t vertex_offset{};
};
inline void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }

// Shared validation, also run on host. Destruction invalidates all old handles.
class Images {
public:
    struct Entry { ImageDesc desc{}; Usage usage{}; uint64_t bytes{}; uint32_t generation{1}; bool live{}; };
    static constexpr uint32_t capacity = 16;
    std::array<Entry, capacity> entries{};
    uint64_t used{}, limit;
    explicit Images(uint64_t budget) : limit(budget) {}
    static void validate(ImageDesc d) {
        require(d.width && d.height && d.width <= 4096 && d.height <= 4096, "image dimensions");
        require(d.samples == 1 || d.samples == 4, "unsupported sample count");
        require(d.format == Format::rgba8 || d.format == Format::z24s8, "unsupported format");
        require(!d.present || (d.format == Format::rgba8 && d.samples == 1), "present format/samples");
    }
    Image insert(ImageDesc d, uint64_t bytes) {
        validate(d);
        require(bytes && bytes <= limit - used, "image budget exceeded");
        for (uint32_t i=0; i<capacity; ++i) if (!entries[i].live) {
            auto& e=entries[i]; e.desc=d; e.bytes=bytes; e.usage=Usage::undefined; e.live=true;
            used+=bytes; return {i,e.generation};
        }
        throw std::runtime_error("image slots exhausted");
    }
    Entry& get(Image h) {
        require(h.slot < capacity, "image slot"); auto& e=entries[h.slot];
        require(e.live && e.generation == h.generation, "stale image handle"); return e;
    }
    void erase(Image h) { auto& e=get(h); used-=e.bytes; e.live=false; ++e.generation; }
    void transition(Image h, Usage expected, Usage next) {
        auto& e=get(h); require(e.usage == expected, "unexpected resource state");
        require(next != Usage::undefined, "cannot discard initialized state");
        require(next != Usage::present || e.desc.present, "not presentable");
        require(next != Usage::sampled || (e.desc.format == Format::rgba8 && e.desc.samples == 1), "sampled format");
        require(next != Usage::readback || (e.desc.format == Format::rgba8 && e.desc.samples == 1), "readback format");
        e.usage=next;
    }
    void targets(Image color, Image depth) {
        auto& c=get(color); require(c.usage == Usage::target && c.desc.format == Format::rgba8, "color target state");
        if (depth.slot != UINT32_MAX) {
            auto& z=get(depth); require(z.usage == Usage::target && z.desc.format == Format::z24s8, "depth target state");
            require(c.desc.width == z.desc.width && c.desc.height == z.desc.height && c.desc.samples == z.desc.samples, "attachment mismatch");
        }
    }
    void resolve(Image source, Image dest) {
        auto& s=get(source); auto& d=get(dest);
        require(s.usage == Usage::resolve_source && d.usage == Usage::resolve_dest, "resolve states");
        require(s.desc.format == Format::rgba8 && d.desc.format == Format::rgba8 && s.desc.samples == 4 && d.desc.samples == 1, "resolve formats/samples");
        require(s.desc.width == d.desc.width && s.desc.height == d.desc.height, "resolve dimensions");
    }
};

class Commands {
public:
    virtual ~Commands() = default;
    virtual Image create(ImageDesc) = 0;
    virtual void destroy(Image) = 0;
    // DKSH for Switch; shader conversion from Xenos/SPIR-V is a separate boundary.
    virtual Shader shader(const void*, uint32_t) = 0;
    virtual void begin() = 0;
    virtual void transition(Image, Usage before, Usage after) = 0;
    virtual void target(Image color, Image depth = {}) = 0;
    virtual void clear(float r, float g, float b, float a, bool depth = false, float z = 1, uint8_t stencil = 0) = 0;
    virtual void sample(Image,Sampler = {}) = 0;
    virtual Buffer buffer(BufferKind,uint32_t,const void*) = 0;
    virtual void update(Buffer,uint32_t offset,uint32_t bytes,const void*) = 0;
    virtual void destroy(Buffer) = 0;
    virtual void destroy(Shader) = 0;
    virtual void vertex(Buffer,const VertexLayout&) = 0;
    virtual void uniform(Buffer,uint32_t binding = 0) = 0;
    virtual void uploadRGBA8(Image,const void*,uint32_t bytes) = 0;
    virtual void draw(const Draw&) = 0;
    virtual void resolve(Image source, Image dest) = 0;
    virtual void copy(Image source, Image dest) = 0;
    virtual uint64_t imageBytes() const = 0;
    // Submit + wait before command storage/resource reuse in this first bounded backend.
    virtual void submit() = 0;
};
} // namespace fhnx::gpu
