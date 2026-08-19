#version 450 core

layout(binding = 0) uniform usampler2D tex2D;

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 outColor;

// Visualization Mode:
// 0 = Meshlet ID (default)
// 1 = Instance / Object ID
// 2 = Triangle Offset ID
// 3 = Full Synthetic Primitive ID
#ifndef VIS_MODE
#define VIS_MODE 0
#endif

uint hash(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

void main() {
    ivec2 size = textureSize(tex2D, 0);
    ivec2 coord = ivec2(texCoord * vec2(size));
    uvec2 visData = texelFetch(tex2D, coord, 0).rg;

    uint instanceID     = visData.r;
    uint syntheticIndex = visData.g;

    // Unrendered Background / Sky Sentinel (instanceID == 0)
    if (instanceID == 0u) {
        outColor = vec4(0.04, 0.04, 0.06, 1.0); // Dark Slate
        return;
    }

    // Decode Synthetic Index
    uint objectID       = (syntheticIndex >> 22u) & 0x3FFu;
    uint localMeshletID = (syntheticIndex >> 9u) & 0x1FFFu;
    uint triangleOffset = syntheticIndex & 0x1FFu;

    uint key = 0u;
#if VIS_MODE == 1
    // Object / Instance ID
    key = hash(instanceID);
#elif VIS_MODE == 2
    // Triangle ID
    key = hash(instanceID * 374761393u + triangleOffset * 668265263u);
#elif VIS_MODE == 3
    // Full Synthetic Index
    key = hash(syntheticIndex);
#else
    // Default: Meshlet ID (Instance + Local Meshlet)
    key = hash(instanceID * 374761393u + localMeshletID * 668265263u);
#endif

    float r = float((key >>  0) & 0xFFu) / 255.0;
    float g = float((key >>  8) & 0xFFu) / 255.0;
    float b = float((key >> 16) & 0xFFu) / 255.0;

    outColor = vec4(clamp(vec3(r, g, b) * 1.3 + 0.2, 0.0, 1.0), 1.0);
}
