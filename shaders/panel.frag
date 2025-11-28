#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform vec2 uDot;              // [0,1]^2 selected point
uniform vec3 uSpinUpColor;      // spin-up color
uniform vec3 uSpinDownColor;    // spin-down color

void main() {
    // ----- Background: angle-based shading spinUp -> spinDown -----
    vec2 center = vec2(0.5);
    vec2 rel    = vUV - center;

    // Angle in [-pi, pi], 0 along +X, π / -π along -X (horizontal negative)
    float phi = atan(rel.y, rel.x);
    const float PI = 3.14159265359;

    // Map phi ∈ [-pi/2, pi/2] to t ∈ [0,1], clamped for more extreme values
    float t = (phi + 0.5 * PI) / PI;
    t = clamp(t, 0.0, 1.0);

    vec3 col = mix(uSpinDownColor, uSpinUpColor, t);

    // ----- Sharp dot on top -----
    float d = length(vUV - uDot);
    float r = 0.02; // dot radius

    if (d < r) {
        col = vec3(0.0);  // solid black dot
    }

    FragColor = vec4(col, 1.0);
}
