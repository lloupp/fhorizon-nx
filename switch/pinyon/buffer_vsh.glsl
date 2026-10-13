#version 460
layout(location=0) in vec3 position;
layout(location=1) in vec4 color;
layout(location=2) in vec2 uv;
layout(std140,binding=0) uniform Constants {mat4 transform;vec4 tint;} u;
layout(location=0) out vec4 tint;
layout(location=1) out vec2 texcoord;
void main(){gl_Position=u.transform*vec4(position,1);tint=color*u.tint;texcoord=uv;}
