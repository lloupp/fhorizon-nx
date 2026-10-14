#pragma once
#include <array>
namespace fhnx::gpu::bundle {
inline constexpr std::array<unsigned char,48> program0={0,0,0,0,48,1,196,0,40,0,0,0,200,15,0,0,0,0,0,0,1,0,1,0,200,15,0,1,0,0,0,0,128,0,2,0,201,15,128,0,0,0,0,0,194,1,1,0};
inline constexpr char glsl0[]=R"fhnx(#version 460
layout(location=0) out vec4 outColor;
layout(std140,binding=1) uniform XenosFloatConstants {vec4 c[4];} xe;
vec4 xe_mul(vec4 a,vec4 b){return vec4(a.x==0.0||b.x==0.0?0.0:a.x*b.x,a.y==0.0||b.y==0.0?0.0:a.y*b.y,a.z==0.0||b.z==0.0?0.0:a.z*b.z,a.w==0.0||b.w==0.0?0.0:a.w*b.w);}
void main(){
vec4 r0;
vec4 r1;
vec4 r2;
vec4 r3;
vec4 r4;
vec4 r5;
vec4 r6;
vec4 r7;
precise vec4 value0=xe_mul(xe.c[0].xyzw,xe.c[1].xyzw);
r0=value0;
precise vec4 value1=(r0.xyzw+xe.c[2].xyzw);
r1=value1;
precise vec4 value2=clamp(max(r1.xyzw,r1.xyzw),0.0,1.0);
outColor=value2;
}
)fhnx";
inline constexpr std::array<unsigned char,48> program1={0,0,0,0,48,1,196,0,40,0,0,0,200,15,0,0,0,13,0,0,1,0,1,0,200,15,0,1,0,0,0,0,128,0,2,0,201,15,128,0,0,0,0,0,194,1,1,0};
inline constexpr char glsl1[]=R"fhnx(#version 460
layout(location=0) out vec4 outColor;
layout(std140,binding=1) uniform XenosFloatConstants {vec4 c[4];} xe;
vec4 xe_mul(vec4 a,vec4 b){return vec4(a.x==0.0||b.x==0.0?0.0:a.x*b.x,a.y==0.0||b.y==0.0?0.0:a.y*b.y,a.z==0.0||b.z==0.0?0.0:a.z*b.z,a.w==0.0||b.w==0.0?0.0:a.w*b.w);}
void main(){
vec4 r0;
vec4 r1;
vec4 r2;
vec4 r3;
vec4 r4;
vec4 r5;
vec4 r6;
vec4 r7;
precise vec4 value0=xe_mul(xe.c[0].yxzw,xe.c[1].xyzw);
r0=value0;
precise vec4 value1=(r0.xyzw+xe.c[2].xyzw);
r1=value1;
precise vec4 value2=clamp(max(r1.xyzw,r1.xyzw),0.0,1.0);
outColor=value2;
}
)fhnx";
}
