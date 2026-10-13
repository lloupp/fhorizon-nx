#version 460
// Explicit vertices avoid UAM's IMAD(reg) modulo lowering, unsupported in Eden 0.2.1.
// Near triangle first, larger far triangle second: depth must preserve the near one.
const vec4 position[6] = vec4[](
    vec4(0,0.55,0.2,1), vec4(-0.55,-0.55,0.2,1), vec4(0.55,-0.55,0.2,1),
    vec4(0,0.95,0.8,1), vec4(-0.95,-0.95,0.8,1), vec4(0.95,-0.95,0.8,1));
const vec2 uv[6] = vec2[](vec2(0.5,1),vec2(0,0),vec2(1,0),
                              vec2(0.5,1),vec2(0,0),vec2(1,0));
const vec4 colors[6] = vec4[](vec4(1),vec4(1),vec4(1),
                              vec4(0.2,0.3,1,1),vec4(0.2,0.3,1,1),vec4(0.2,0.3,1,1));
layout(location=0) out vec4 tint;
layout(location=1) out vec2 texcoord;
void main() {
    gl_Position = position[gl_VertexID];
    tint = colors[gl_VertexID];
    texcoord = uv[gl_VertexID];
}
