#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform vec2 uDot; // [0,1]^2 dot position (x=temp axis, y=mu axis later)

void main() {
    vec3 bg = vec3(0.12, 0.12, 0.15);
    vec3 col = bg;

    // Optional subtle grid (10x10)
    vec2 g = fract(vUV * 10.0);
    float line = (step(g.x, 0.01) + step(g.y, 0.01));
    col += line * 0.03;

    // Dot
    float d = length(vUV - uDot);
    float r = 0.03;
    float edge = smoothstep(r, r - 0.01, d);

    vec3 dotColor = vec3(1.0, 0.8, 0.2);
    col = mix(dotColor, col, edge);

    FragColor = vec4(col, 1.0);
}
