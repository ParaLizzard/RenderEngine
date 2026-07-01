#version 460
layout(location = 0) in vec3 inPosition;
layout(location = 0) flat out uint v_DrawID;

struct ObjectData {
    mat4 modelMatrix;
    mat4 normalMatrix;
    vec4 boundingSphere;
};

layout(set = 0, binding = 0) readonly buffer ObjectBuffer {
    ObjectData objects[];
};

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
} pc;

void main() {
    v_DrawID = gl_InstanceIndex;
    mat4 model = objects[gl_InstanceIndex].modelMatrix;
    gl_Position = pc.viewProj * model * vec4(inPosition, 1.0);
}