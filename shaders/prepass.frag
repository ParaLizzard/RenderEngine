#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 inTangent;
layout (location = 4) in flat uint inTexID;

// Multiple Render Targets (MRT)
layout (location = 0) out vec4 outAlbedoMetallic;
layout (location = 1) out vec4 outNormalRoughness;

struct Material {
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
    uint  padding[2];
};

layout(set = 0, binding = 0) readonly buffer MaterialBuffer {
    Material materials[];
};

layout(set = 0, binding = 5) uniform sampler2D textures[];

void main() {
    Material mat = materials[inTexID];

    vec4 albedo = texture(textures[nonuniformEXT(mat.albedoIndex)], inUV) * mat.albedoFactor;

    uint isMask = mat.flags & 1u;
    if (isMask != 0u && albedo.a < mat.alphaCutoff) {
        discard;
    }

    float roughness = texture(textures[nonuniformEXT(mat.roughnessMetallicIndex)], inUV).g * mat.roughnessFactor;
    float metalness = texture(textures[nonuniformEXT(mat.roughnessMetallicIndex)], inUV).b * mat.metallicFactor;

    vec3 T = normalize(inTangent.xyz);
    vec3 N = normalize(inNormal);
    vec3 B = cross(N, T) * inTangent.w;
    if (!gl_FrontFacing) {
        T = -T; B = -B; N = -N;
    }

    mat3 TBN = mat3(T, B, N);
    vec3 tNorm = texture(textures[nonuniformEXT(mat.normalIndex)], inUV).rgb * 2.0 - 1.0;
    tNorm.xy *= mat.normalScale;

    vec3 worldNormal = normalize(TBN * tNorm);
    if (!gl_FrontFacing) { worldNormal = -worldNormal; }

    vec3 packedNormal = worldNormal * 0.5 + 0.5;

    // Pack into G-Buffer
    outAlbedoMetallic = vec4(albedo.rgb, metalness);
    outNormalRoughness = vec4(packedNormal, roughness);
}