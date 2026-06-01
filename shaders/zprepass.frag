#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 inUV;
layout(location = 1) flat in uint inMaterialIndex;

struct MaterialData {
    vec4 albedoFactor;
    vec4 emissiveFactor;
    uint albedoIndex;
    uint normalIndex;
    uint roughnessMetallicIndex;
    uint emissiveIndex;
    uint occlusionIndex;
    uint flags;
    float alphaCutoff;
    float normalScale;
    float roughnessFactor;
    float metallicFactor;
    uint pad1;
    uint pad2;
};

layout(std140, set = 0, binding = 0) readonly buffer MaterialSSBO {
    MaterialData materials[];
};

layout(set = 0, binding = 5) uniform sampler2D bindlessTextures[];

void main() {
    MaterialData mat = materials[inMaterialIndex];
    vec4 albedoTex = texture(bindlessTextures[nonuniformEXT(mat.albedoIndex)], inUV);
    float alpha = albedoTex.a * mat.albedoFactor.a;

    if (alpha < mat.alphaCutoff) {
        discard;
    }
}