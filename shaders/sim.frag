#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uGrid;
uniform vec2      uTexelSize;        // (1.0/W, 1.0/H)
uniform int       uParity;          // 0 or 1
uniform float     uTemperature;     // T (k_B = 1)
uniform float     uRandSeed;
uniform float     uFieldStrength; // h

// Simple hash-based RNG
float hash(vec2 p, float seed) {
    vec3 p3 = fract(vec3(p.x, p.y, seed) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    vec2  gridSize = 1.0 / uTexelSize;
    ivec2 ij       = ivec2(floor(vUV * gridSize)); // (i,j)

    int   parity   = (ij.x + ij.y) & 1;
    float sCenter  = texture(uGrid, vUV).r;        // -1 or +1

    // Not this parity → just copy
    if (parity != uParity) {
        FragColor = vec4(sCenter, 0.0, 0.0, 1.0);
        return;
    }

    // Toroidal neighbors via GL_REPEAT wrap
    vec2 offsetX = vec2(uTexelSize.x, 0.0);
    vec2 offsetY = vec2(0.0, uTexelSize.y);

    float sUp    = texture(uGrid, vUV + offsetY).r;
    float sDown  = texture(uGrid, vUV - offsetY).r;
    float sLeft  = texture(uGrid, vUV - offsetX).r;
    float sRight = texture(uGrid, vUV + offsetX).r;

    // Neighbor contribution
    float sumN = sUp + sDown + sLeft + sRight;

    // Ising part: ΔE_J = 2 * s * sumN
    float deltaJ  = 2.0 * sCenter * sumN;

    // Field part: ΔE_h = 2 * h * s
    float deltaMu = 2.0 * uFieldStrength * sCenter;

    // Total ΔE = neighbor + chemical potential
    float dE = deltaJ + deltaMu;

    // Glauber probability: p = 1 / (1 + exp(ΔE / T))
    float p = 1.0 / (1.0 + exp(dE / uTemperature));

    float r = hash(vUV * gridSize, uRandSeed);

    float sNew = sCenter;
    if (r < p) {
        sNew = -sCenter;
    }

    FragColor = vec4(sNew, 0.0, 0.0, 1.0);
}
