#version 460

layout(location = 0) in vec3 inPosition;

layout(location = 0) flat out uint outInstanceID;

struct ObjectData {
    mat4 modelMatrix;
    mat4 normalMatrix;
    vec4 boundingSphere;
};

layout(std430, set = 0, binding = 0) readonly buffer ObjectBuffer {
    ObjectData objects[];
};

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
} pc;

void main() {
    // 1. Fetch the true global ID for this object
    uint objectID = gl_InstanceIndex;

    // 2. Pass it down to the VisBuffer
    outInstanceID = objectID;

    mat4 model = objects[objectID].modelMatrix;
    gl_Position = pc.viewProj * model * vec4(inPosition, 1.0);
}