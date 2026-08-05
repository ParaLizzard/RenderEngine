#version 460

layout(location = 0) flat in uint inInstanceID;
layout(location = 0) out uvec2 outVisBuffer;

layout(set = 1, binding = 3) readonly buffer CompactedIndexBuffer { uint syntheticIndices[]; };

void main() {
    uint syntheticIndex = syntheticIndices[gl_PrimitiveID * 3u];
    outVisBuffer = uvec2(inInstanceID + 1u, syntheticIndex);
}