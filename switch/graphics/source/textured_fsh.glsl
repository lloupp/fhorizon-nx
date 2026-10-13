#version 460
layout(location=0) in vec4 tint;
layout(location=1) in vec2 texcoord;
layout(binding=0) uniform sampler2D checker;
layout(location=0) out vec4 color;
void main() { color = texture(checker,texcoord) * tint; }
