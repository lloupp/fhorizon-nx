#pragma once
#include "commands.hpp"
#include <rex/graphics/format/ucode.h>
#include <bitset>
#include <bit>
#include <cmath>
#include <cstring>
#include <sstream>
#include <span>

namespace fhnx::gpu {
// Offline GLSL producer for a strict real Xenos pixel-ALU subset. This does not
// link the Vulkan/SPIR-V executor or implement the complete ShaderTranslator.
class XenosPixelShader {
public:
    struct Operand {bool temporary{};uint32_t index{};std::array<uint8_t,4> components{};};
    struct Operation {rex::graphics::ucode::AluVectorOpcode opcode;Operand lhs,rhs;uint32_t destination;bool export_color,clamp;};
    struct Program {std::vector<Operation> operations;uint32_t constant_mask{};};
    static constexpr uint32_t constant_count=4,temporary_count=8,constant_binding=1;
    Program decode(std::span<const uint8_t> bytes,rex::graphics::xenos::ShaderType stage) const {
        using namespace rex::graphics;using namespace ucode;
        require(stage==xenos::ShaderType::kPixel,"Xenos GLSL: vertex/compute stage unsupported");
        require(bytes.size()>=24 && bytes.size()<=3*7*4 && bytes.size()%12==0,"Xenos GLSL: microcode size/alignment");
        std::vector<uint32_t> words(bytes.size()/4);for(size_t i=0;i<words.size();++i)for(unsigned b=0;b<4;++b)words[i]=(words[i]<<8)|bytes[i*4+b];
        ControlFlowInstruction cf[2]{};UnpackControlFlowInstructions(words.data(),cf);
        require(cf[0].opcode()==ControlFlowOpcode::kAlloc && cf[0].alloc.alloc_type()==AllocType::kPsColors && cf[0].alloc.size()==0 && cf[0].dword_0==0 && cf[0].dword_1==0xc400,"Xenos GLSL: requires one canonical color allocation");
        auto exec=cf[1].exec;
        require(exec.opcode()==ControlFlowOpcode::kExecEnd && exec.address()==1 && exec.count()>=1 && exec.count()<=6 && exec.addressing_mode()==AddressingMode::kAbsolute,"Xenos GLSL: linear absolute EXEC_END subset");
        require(cf[1].dword_0==(1u|(exec.count()<<12)) && cf[1].dword_1==0x2800,"Xenos GLSL: fetch/serialize/predicate/CF flags unsupported");
        require(words.size()==3*(1+exec.count()),"Xenos GLSL: trailing/truncated instructions");
        Program result;std::bitset<temporary_count> initialized;bool exported=false;unsigned multiplications=0;
        for(uint32_t i=0;i<exec.count();++i){auto raw=words.data()+3*(i+1);auto op=std::bit_cast<AluInstruction>(std::array<uint32_t,3>{raw[0],raw[1],raw[2]});
            auto opcode=op.vector_opcode();require(opcode==AluVectorOpcode::kAdd || opcode==AluVectorOpcode::kMul || opcode==AluVectorOpcode::kMax,"Xenos GLSL: vector opcode unsupported");
            if(opcode==AluVectorOpcode::kMul)require(++multiplications<=2,"Xenos GLSL: bounded multiply budget");
            auto& info=GetAluVectorOpcodeInfo(opcode);require(info.GetOperandCount()==2 && info.changed_state==kAluOpChangedStateNone,"Xenos GLSL: state-changing ALU unsupported");
            // Accept only full vector writes and retain_prev scalar NOP. All
            // unconsumed scalar/source3/address/predicate bits must be zero.
            uint32_t canonical=op.vector_dest()|(uint32_t(op.is_export())<<15)|(15u<<16)|(uint32_t(op.vector_clamp())<<24)|(uint32_t(AluScalarOpcode::kRetainPrev)<<26);
            require(raw[0]==canonical && op.vector_write_mask()==15,"Xenos GLSL: partial/scalar/relative writes unsupported");
            require(!(raw[1]&~0x00ffff00u) && (raw[1]&0xff)==0 && (raw[2]&0x200000ffu)==0,"Xenos GLSL: source3/predication/addressing flags unsupported");
            auto operand=[&](unsigned source){Operand v;v.temporary=op.src_is_temp(source);auto index=op.src_reg(source);
                if(v.temporary){require(!AluInstruction::is_src_temp_relative(index),"Xenos GLSL: relative source unsupported");v.index=index;require(v.index<temporary_count && initialized[v.index],"Xenos GLSL: uninitialized/large temporary");}
                else {v.index=index;require(v.index<constant_count && !op.src_const_is_addressed(source),"Xenos GLSL: constant index/address unsupported");result.constant_mask|=1u<<v.index;}
                for(unsigned c=0;c<4;++c){v.components[c]=uint8_t(AluInstruction::GetSwizzledComponentIndex(op.src_swizzle(source),c));}
                return v;};
            auto a=operand(1),b=operand(2);uint32_t dest=op.vector_dest();
            require(!exported,"Xenos GLSL: instructions after export unsupported");
            if(op.is_export()){require(dest==uint32_t(ExportRegister::kPSColor0) && i+1==exec.count(),"Xenos GLSL: only final color0 export");exported=true;}
            else {require(dest<temporary_count,"Xenos GLSL: temporary budget");initialized.set(dest);}
            result.operations.push_back({opcode,a,b,dest,op.is_export(),op.vector_clamp()});
        }
        require(exported,"Xenos GLSL: missing color export");return result;
    }
    static void validateProgram(const Program& program){
        require(!program.operations.empty() && program.operations.size()<=6,"Xenos GLSL: IR budget");
        std::bitset<temporary_count> initialized;uint32_t constants=0;bool exported=false;unsigned multiplications=0;
        for(size_t i=0;i<program.operations.size();++i){auto& o=program.operations[i];
            using rex::graphics::ucode::AluVectorOpcode;
            require(o.opcode==AluVectorOpcode::kAdd || o.opcode==AluVectorOpcode::kMul || o.opcode==AluVectorOpcode::kMax,"Xenos GLSL: IR opcode");
            if(o.opcode==AluVectorOpcode::kMul)require(++multiplications<=2,"Xenos GLSL: IR multiply budget");
            require(!exported,"Xenos GLSL: IR after export");
            for(auto* source:{&o.lhs,&o.rhs}){auto& v=*source;
                if(v.temporary)require(v.index<temporary_count && initialized[v.index],"Xenos GLSL: IR undefined temp");
                else {require(v.index<constant_count,"Xenos GLSL: IR constant index");constants|=1u<<v.index;}
                for(auto c:v.components)require(c<4,"Xenos GLSL: IR swizzle");}
            if(o.export_color){require(o.destination==0 && i+1==program.operations.size(),"Xenos GLSL: IR color export");exported=true;}
            else {require(o.destination<temporary_count,"Xenos GLSL: IR temp budget");initialized.set(o.destination);}
        }
        require(exported && constants==program.constant_mask,"Xenos GLSL: IR export/constants contract");
    }
    std::string glsl(const Program& program) const {
        validateProgram(program);
        require(!program.operations.empty() && program.operations.size()<=6,"Xenos GLSL: empty/large IR");
        std::ostringstream s;s<<"#version 460\nlayout(location=0) out vec4 outColor;\nlayout(std140,binding=1) uniform XenosFloatConstants {vec4 c[4];} xe;\n";
        // Preserve the Xenos D3D9 multiplication rule for zero operands. This
        // first resource contract accepts finite, bounded float constants only.
        s<<"vec4 xe_mul(vec4 a,vec4 b){return vec4(a.x==0.0||b.x==0.0?0.0:a.x*b.x,a.y==0.0||b.y==0.0?0.0:a.y*b.y,a.z==0.0||b.z==0.0?0.0:a.z*b.z,a.w==0.0||b.w==0.0?0.0:a.w*b.w);}\nvoid main(){\n";
        auto operand=[](const Operand& o){std::string v=(o.temporary?"r":"xe.c[")+std::to_string(o.index)+(o.temporary?"":"]")+".";for(auto c:o.components){require(c<4,"Xenos GLSL: IR swizzle");v+="xyzw"[c];}return v;};
        for(size_t i=0;i<program.operations.size();++i){auto& o=program.operations[i];auto a=operand(o.lhs),b=operand(o.rhs);std::string value;
            using rex::graphics::ucode::AluVectorOpcode;
            if(o.opcode==AluVectorOpcode::kAdd)value="("+a+"+"+b+")";
            else if(o.opcode==AluVectorOpcode::kMul)value="xe_mul("+a+","+b+")";
            else if(o.opcode==AluVectorOpcode::kMax)value="max("+a+","+b+")";
            else throw std::runtime_error("Xenos GLSL: IR opcode unsupported");
            if(o.clamp)value="clamp("+value+",0.0,1.0)";
            // SSA values avoid overwrite/read-order issues in the bounded IR.
            s<<"precise vec4 value"<<i<<"="<<value<<";\n";
            if(o.export_color)s<<"outColor=value"<<i<<";\n";
            else s<<"r"<<o.destination<<"=value"<<i<<";\n";
        }
        // All temporary declarations precede uses; zero initialization is not
        // substituted for undefined reads (the decoder rejects them).
        std::string text=s.str();auto start=text.find("void main(){\n")+13;
        std::string declarations;for(unsigned r=0;r<temporary_count;++r)declarations+="vec4 r"+std::to_string(r)+";\n";text.insert(start,declarations);return text+"}\n";
    }
};
struct alignas(16) XenosFloatConstants {std::array<std::array<float,4>,4> values;};
inline void validateXenosConstants(const XenosFloatConstants& constants){for(auto& v:constants.values)for(float f:v)require(std::isfinite(f) && std::abs(f)<=16,"Xenos GLSL: finite bounded constants required");}
}
