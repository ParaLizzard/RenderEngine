#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) flat in uint inInstanceID;
layout(location = 1) flat in uint inSyntheticIndex;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inCurClipPos;
layout(location = 4) in vec4 inPrevClipPos;

layout(location = 0) out uvec2 outVisBuffer;
layout(location = 1) out vec2 outVelocity;

struct ObjectData {
    mat4 modelMatrix;
    mat4 normalMatrix;
    vec4 boundingSphere;
    uint baseMeshlet;
    uint meshletCount;
    uint alphaMode;
    uint materialId;
};

struct MaterialData {
    vec4  albedoFactor;
    vec4  emissiveFactor;
    uint  albedoIndex;
    uint  normalIndex;
    uint  roughnessMetallicIndex;
    uint  emissiveIndex;
    uint  occlusionIndex;
    uint  flags;
    float alphaCutoff;
    float normalScale;
    float roughnessFactor;
    float metallicFactor;
    uint  padding0;
    uint  padding1;
};

layout(set = 0, binding = 0) readonly buffer MaterialHeapBuffer { MaterialData materials[]; } materialHeap;
layout(set = 0, binding = 5) readonly buffer ObjectBuffer       { ObjectData objects[];    } objectBuffer;
layout(set = 0, binding = 12) uniform sampler2D bindlessTextures[];

void main() {
    uint objectID = inSyntheticIndex >> 22u;
    uint materialId = objectBuffer.objects[objectID].materialId;

    MaterialData mat = materialHeap.materials[materialId];

    float alpha = mat.albedoFactor.a;
    if (mat.albedoIndex != 0xFFFFFFFFu) {
        alpha *= texture(bindlessTextures[nonuniformEXT(mat.albedoIndex)], inUV).a;
    }

    if (alpha < mat.alphaCutoff) {
        discard;
    }

    outVisBuffer = uvec2(inInstanceID + 1u, inSyntheticIndex);
    vec2 curNDC  = inCurClipPos.xy / inCurClipPos.w;
    vec2 prevNDC = inPrevClipPos.xy / inPrevClipPos.w;
    outVelocity  = (curNDC - prevNDC) * 0.5;
}
