#pragma once
#include "commands.hpp"
#include <rex/graphics/fh1_edram_surfaces.h>
#include <map>

namespace fhnx::gpu {
// Bounded native-surface frontend. Uses the actual upstream identity and pitch
// helper. Tile ownership transfers / guest register decoding are NOT emulated.
class PinyonSurfaces {
public:
    using Key=rex::graphics::Fh1SurfaceKey;
    struct Surface {Key key;ImageDesc desc;Image image;Usage usage{Usage::undefined};uint64_t bytes;uint32_t end_tile;};
private:
    Commands& gpu_;
    uint64_t budget_,used_{};
    std::map<uint32_t,Surface> cache_;
    Surface* color_{};
    Surface* depth_{};
public:
    uint32_t cache_hits{},creations{};
    explicit PinyonSurfaces(Commands& gpu,uint64_t budget):gpu_(gpu),budget_(budget){}
    PinyonSurfaces(const PinyonSurfaces&)=delete;
    PinyonSurfaces& operator=(const PinyonSurfaces&)=delete;
    static ImageDesc describe(Key key,uint32_t width,uint32_t height) {
        using namespace rex::graphics;
        require(key.base_tiles<xenos::kEdramTileCount && key.pitch_tiles && key.pitch_tiles<2048,"surface key range");
        require(key.msaa==uint32_t(xenos::MsaaSamples::k1X) || key.msaa==uint32_t(xenos::MsaaSamples::k4X),"unsupported guest MSAA");
        require(key.format==0,key.is_depth?"unsupported guest depth format":"unsupported guest color/gamma format");
        require(key.pitch_tiles==Fh1PitchTiles(width,key.msaa),"surface guest pitch mismatch");
        ImageDesc d{width,height,1u<<key.msaa,key.is_depth?Format::z24s8:Format::rgba8};Images::validate(d);return d;
    }
    Surface& acquire(Key key,uint32_t width,uint32_t height) {
        auto d=describe(key,width,height);auto packed=key.Pack();auto it=cache_.find(packed);
        if(it!=cache_.end()) {
            require(it->second.desc.width==width && it->second.desc.height==height,"surface resize requires explicit release");
            ++cache_hits;return it->second;
        }
        // Reject overlapping/wrapping EDRAM footprints rather than inventing
        // coherent ownership transfers. Supported surfaces are disjoint only.
        uint32_t rows=(height*(key.msaa?2u:1u)+15)/16;
        uint32_t end=key.base_tiles+rows*key.pitch_tiles;
        require(end<=rex::graphics::xenos::kEdramTileCount,"EDRAM wrap unsupported");
        for(auto& [unused,s]:cache_)
            require(end<=s.key.base_tiles || key.base_tiles>=s.end_tile,"EDRAM ownership transfer unsupported");
        auto before=gpu_.imageBytes();auto image=gpu_.create(d);auto bytes=gpu_.imageBytes()-before;
        if(bytes>budget_-used_) {gpu_.destroy(image);throw std::runtime_error("surface cache budget exceeded");}
        try {it=cache_.emplace(packed,Surface{key,d,image,Usage::undefined,bytes,end}).first;}
        catch(...) {gpu_.destroy(image);throw;}
        used_+=bytes;++creations;return it->second;
    }
    Surface& get(Key key) {
        auto it=cache_.find(key.Pack());require(it!=cache_.end(),"surface not cached");
        auto k=it->second.key;
        require(k.base_tiles==key.base_tiles && k.pitch_tiles==key.pitch_tiles && k.msaa==key.msaa && k.format==key.format && k.is_depth==key.is_depth,"malformed packed-key collision");
        return it->second;
    }
    void transition(Surface& s,Usage next) {gpu_.transition(s.image,s.usage,next);s.usage=next;}
    void bind(Key color,const Key* depth=nullptr) {
        auto& c=get(color);Surface* z=depth?&get(*depth):nullptr;
        require(!c.key.is_depth && (!z || z->key.is_depth),"attachment roles");
        if(z) require(c.desc.width==z->desc.width && c.desc.height==z->desc.height && c.desc.samples==z->desc.samples,"surface attachment mismatch");
        transition(c,Usage::target);if(z)transition(*z,Usage::target);
        gpu_.target(c.image,z?z->image:Image{});color_=&c;depth_=z;
    }
    void clear(float r,float g,float b,float a,bool depth=false,float z=1,uint8_t stencil=0) {
        require(color_ && (!depth || depth_),"clear attachments");gpu_.clear(r,g,b,a,depth,z,stencil);
    }
    void draw(const Draw& d) {require(color_,"draw attachments");gpu_.draw(d);}
    void resolve(Key from,Key to) {
        auto& s=get(from);auto& d=get(to);
        require(!s.key.is_depth && !d.key.is_depth && s.desc.samples==4 && d.desc.samples==1,"native resolve subset");
        require(s.desc.width==d.desc.width && s.desc.height==d.desc.height,"native resolve size");
        transition(s,Usage::resolve_source);transition(d,Usage::resolve_dest);gpu_.resolve(s.image,d.image);
    }
    void copy(Key from,Key to) {
        auto& s=get(from);auto& d=get(to);
        require(!s.key.is_depth && !d.key.is_depth && s.desc.samples==1 && d.desc.samples==1 && s.image.slot!=d.image.slot,"native copy subset");
        require(s.desc.width==d.desc.width && s.desc.height==d.desc.height,"native copy size");
        transition(s,Usage::copy_source);transition(d,Usage::copy_dest);gpu_.copy(s.image,d.image);
    }
    void sample(Key key) {auto& s=get(key);require(!s.key.is_depth && s.desc.samples==1,"native sampled subset");transition(s,Usage::sampled);gpu_.sample(s.image);}
    void release(Key key) {
        get(key);auto it=cache_.find(key.Pack());
        gpu_.destroy(it->second.image);used_-=it->second.bytes;
        if(color_==&it->second)color_=nullptr;
        if(depth_==&it->second)depth_=nullptr;
        cache_.erase(it);
    }
    uint64_t bytes() const{return used_;}
    size_t count() const{return cache_.size();}
    void shutdown() {while(!cache_.empty())release(cache_.begin()->second.key);}
};
}
