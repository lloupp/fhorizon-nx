#pragma once
#include "pm4_frontend.hpp"

namespace fhnx::gpu {
// Explicit resource bindings for a bounded synthetic guest address space.
// No guest pointer is dereferenced, and no ShiftGlue pointer ABI is changed.
// Shader/fetch translation is deliberately outside this first implementation.
class Pm4DrawExecutor {
    struct IndexBinding {Buffer buffer;uint32_t bytes;rex::graphics::xenos::Endian endian;};
    PinyonSurfaces& surfaces_;
    std::map<uint32_t,IndexBinding> indices_;
public:
    explicit Pm4DrawExecutor(PinyonSurfaces& surfaces):surfaces_(surfaces){}
    static std::vector<uint16_t> decodeIndex16(std::span<const uint8_t> bytes,rex::graphics::xenos::Endian endian) {
        using rex::graphics::xenos::Endian;
        require(bytes.size()%2==0 && bytes.size()<=131070,"PM4 index upload size");
        require(endian==Endian::kNone || endian==Endian::k8in16,"PM4 index upload endian");
        std::vector<uint16_t> result;result.reserve(bytes.size()/2);
        for(size_t i=0;i<bytes.size();i+=2)
            result.push_back(endian==Endian::k8in16 ? (uint16_t(bytes[i])<<8)|bytes[i+1] :
                                                    (uint16_t(bytes[i+1])<<8)|bytes[i]);
        return result;
    }
    void bindIndices(uint32_t address,Buffer buffer,uint32_t bytes,rex::graphics::xenos::Endian endian) {
        require(!(address&1) && bytes && !(bytes&1) && buffer.slot!=UINT32_MAX,"PM4 invalid index binding");
        require(uint64_t(address)+bytes<=uint64_t(UINT32_MAX)+1,"PM4 index address overflow");
        require(indices_.contains(address) || indices_.size()<16,"PM4 index binding budget");
        indices_[address]={buffer,bytes,endian};
    }
    void unbindIndices(uint32_t address){require(indices_.erase(address)==1,"PM4 missing index binding");}
    void prepare(std::span<const Pm4Frontend::IndexedDraw> draws) {
        // Validate resource references for all draws before allocating targets.
        for(auto& d:draws)validate(d);
        for(auto& d:draws){surfaces_.acquire(d.color,d.width,d.height);surfaces_.acquire(d.depth,d.width,d.height);}
    }
    void validate(const Pm4Frontend::IndexedDraw& d) const {
        auto it=indices_.find(d.address);
        require(it!=indices_.end(),"PM4 unmapped index DMA address");
        require(d.index_bytes<=it->second.bytes && d.count<=d.index_bytes/2 &&
                d.endian==it->second.endian,"PM4 DMA range/endian binding mismatch");
    }
    void issue(const Pm4Frontend::IndexedDraw& d,Draw pipeline) {
        validate(d);
        require(pipeline.first==0 && pipeline.vertex_offset==0 && pipeline.color_write &&
                pipeline.stencil==Stencil::disabled,"PM4 pipeline override unsupported");
        surfaces_.bind(d.color,&d.depth);
        pipeline.count=d.count;pipeline.indices=indices_.at(d.address).buffer;
        pipeline.depth_test=d.depth_test;pipeline.depth_write=d.depth_write;
        surfaces_.draw(pipeline);
    }
};
}
