#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) flat out uint v_DrawID;

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
    v_DrawID = objectID;

    // 3. Transform using the actual model matrix!
    mat4 model = objects[objectID].modelMatrix;
    gl_Position = pc.viewProj * model * vec4(inPosition, 1.0);
}