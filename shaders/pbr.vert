#version 460

layout (location = 0) in vec3 inPos;
//layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec2 inUV;
layout (location = 4) in vec4 inTangent;
layout (location = 5) in uint inTexID;

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec4 outTangent;
layout (location = 4) out flat uint outTexID;

layout(push_constant) uniform PushConsts {
    mat4 modelMatrix;
    mat4 viewProjection;
} push;

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

void main() {
    outTexID = inTexID;

    vec4 locPos = push.modelMatrix * vec4(inPos, 1.0);
    outWorldPos = locPos.xyz;

    outUV = inUV;

    mat3 normalMatrix = transpose(inverse(mat3(push.modelMatrix)));

    outNormal = normalize(normalMatrix * inNormal);

    vec3 worldTangent = normalMatrix * inTangent.xyz;
    if (length(worldTangent) < 0.001) {
        outTangent = vec4(0.0, 0.0, 0.0, inTangent.w);
    } else {
        outTangent = vec4(normalize(worldTangent), inTangent.w);
    }

    gl_Position = push.viewProjection * locPos;
}