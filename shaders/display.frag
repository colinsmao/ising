#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uGrid;
uniform vec3      uSpinUpColor;   // color for spin +1
uniform vec3      uSpinDownColor; // color for spin -1

void main() {
    float s = texture(uGrid, vUV).r;    // -1 or +1
    float t = (s + 1.0) * 0.5;          // [-1,+1] -> [0,1]

    // t = 0 → down color, t = 1 → up color
    vec3 col = mix(uSpinDownColor, uSpinUpColor, t);

    FragColor = vec4(col, 1.0);
}
