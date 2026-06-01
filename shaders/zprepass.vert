#version 460

invariant gl_Position;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec4 inTangent;
layout(location = 5) in uint inTexId;

layout(push_constant) uniform PushConstants {
    mat4 modelMatrix;
    mat4 viewProjection;
    uint debugIsTransparent;
} pc;

layout(location = 0) out vec2 outUV;
layout(location = 1) flat out uint outMaterialIndex;

void main() {
    vec4 locPos = pc.modelMatrix * vec4(inPosition, 1.0);
    gl_Position = pc.viewProjection * locPos;

    outUV = inUV;
    outMaterialIndex = inTexId;
}