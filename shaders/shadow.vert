#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 inPosition;

struct ObjectData {
    mat4 modelMatrix;
    mat4 normalMatrix;
    vec4 boundingSphere;
};

layout(set = 0, binding = 1) uniform SceneUbo {
    vec4 cameraPosition;
    vec4 directionalLight;
    mat4 lightViewProj[4];
    vec4 cascadesSplits;
    float maxReflectionLod;
    uint blueNoiseTexIndex;
    vec2 padding;
} sceneUbo;

layout(set = 1, binding = 0) readonly buffer ObjectDataBuffer {
    ObjectData objects[];
} objectData;

layout(push_constant) uniform PushConstants {
    uint cascadeIndex;
} pc;

void main() {
    mat4 model = objectData.objects[gl_InstanceIndex].modelMatrix;
    mat4 lightViewProj = sceneUbo.lightViewProj[pc.cascadeIndex];
    gl_Position = lightViewProj * (model * vec4(inPosition, 1.0));
}
