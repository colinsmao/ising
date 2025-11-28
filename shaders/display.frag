#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uGrid;

void main() {
    float s = texture(uGrid, vUV).r;  // -1 or +1
    float v = (s + 1.0) * 0.5;        // map to [0,1]
    FragColor = vec4(vec3(v), 1.0);
}
