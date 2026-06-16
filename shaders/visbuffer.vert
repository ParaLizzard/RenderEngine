#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) flat out uint outInstanceID;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
} pc;

void main() {
    outInstanceID = gl_InstanceIndex;
    gl_Position = pc.viewProj * vec4(inPosition, 1.0);
}