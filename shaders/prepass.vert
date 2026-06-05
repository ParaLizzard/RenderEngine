#version 460

invariant gl_Position;

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec2 inUV;
layout (location = 4) in vec4 inTangent;
layout (location = 5) in uint inTexID;

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec4 outTangent;
layout (location = 4) out flat uint outTexID;
layout (location = 5) out vec3 outColor;

layout(push_constant) uniform PushConsts {
    mat4 viewProjection;
    uint debugIsTransparent;
} push;

struct ObjectData {
    mat4 modelMatrix;
    mat4 normalMatrix;
    vec4 boundingSphere;
};

layout(std430, set = 1, binding = 0) readonly buffer ObjectBuffer {
    ObjectData objectData[];
};

void main() {
    outTexID = inTexID;
    outColor = inColor;

    mat4 modelMatrix = objectData[gl_InstanceIndex].modelMatrix;
    vec4 locPos = modelMatrix * vec4(inPos, 1.0);
    outWorldPos = locPos.xyz;
    outUV = inUV;

    mat3 normalMatrix = mat3(objectData[gl_InstanceIndex].normalMatrix);
    outNormal = normalize(normalMatrix * inNormal);

    vec3 worldTangent = normalMatrix * inTangent.xyz;
    if (length(worldTangent) < 0.001) {
        outTangent = vec4(0.0, 0.0, 0.0, inTangent.w);
    } else {
        outTangent = vec4(normalize(worldTangent), inTangent.w);
    }

    gl_Position = push.viewProjection * locPos;
}