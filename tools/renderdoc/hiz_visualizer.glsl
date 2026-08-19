#version 450 core

layout(binding = 0) uniform sampler2D tex2D;

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 outColor;

#ifndef DEPTH_MIN
#define DEPTH_MIN 0.0001
#endif

#ifndef DEPTH_MAX
#define DEPTH_MAX 1.0
#endif

// Smooth vibrant color map
vec3 colorMap(float t) {
    // 0.0 = Blue (Near) -> Cyan -> Green -> Yellow -> Red (Far)
    vec3 c0 = vec3(0.1, 0.4, 1.0); // Blue (Closest)
    vec3 c1 = vec3(0.0, 0.9, 0.9); // Cyan
    vec3 c2 = vec3(0.2, 1.0, 0.2); // Green
    vec3 c3 = vec3(1.0, 0.9, 0.0); // Yellow
    vec3 c4 = vec3(1.0, 0.15, 0.15); // Red (Furthest in active range)
    
    if (t < 0.25) return mix(c0, c1, t / 0.25);
    if (t < 0.50) return mix(c1, c2, (t - 0.25) / 0.25);
    if (t < 0.75) return mix(c2, c3, (t - 0.50) / 0.25);
    return mix(c3, c4, (t - 0.75) / 0.25);
}

void main() {
    float depth = texture(tex2D, texCoord).r;

    // 1. Unwritten Padding (0.0)
    if (depth <= 0.000001) {
        outColor = vec4(0.05, 0.02, 0.08, 1.0); // Dark Purple
        return;
    }

    // 2. Sky / Cleared Depth (1.0)
    if (depth >= 0.99999) {
        outColor = vec4(0.02, 0.05, 0.12, 1.0); // Dark Navy
        return;
    }

    // 3. Scene Geometry: Linear or calibrated range
    float t = clamp((depth - DEPTH_MIN) / (DEPTH_MAX - DEPTH_MIN), 0.0, 1.0);
    
    outColor = vec4(colorMap(t), 1.0);
}
