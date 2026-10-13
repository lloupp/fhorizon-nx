#version 450
// Independently authored synthetic vertex shader; not from the game.
layout(location = 0) out vec3 vColor;
void main() {
    const vec2 xy[3] = vec2[3](vec2(-0.75,-0.75), vec2(0.75,-0.75), vec2(0.0,0.75));
    const vec3 rgb[3] = vec3[3](vec3(1,0,0), vec3(0,1,0), vec3(0,0,1));
    int i = gl_VertexID % 3;
    gl_Position = vec4(xy[i], 0.0, 1.0);
    vColor = rgb[i];
}
