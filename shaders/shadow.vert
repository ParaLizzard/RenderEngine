#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shader_viewport_layer_array : require

layout(location = 0) in vec3 inPosition;

struct ObjectData {
    mat4 modelMatrix;
    mat4 normalMatrix;
    vec4 boundingSphere;
};

layout(set = 0, binding = 1) uniform SceneUbo {
    vec4 cameraPosition;
    vec4 directionalLight;
    mat4 lightViewProj[3];
    vec4 cascadesSplits;
    float maxReflectionLod;
    uint blueNoiseTexIndex;
    vec2 padding;
} sceneUbo;

layout(set = 1, binding = 0) readonly buffer ObjectDataBuffer {
    ObjectData objects[];
} objectData;

layout(push_constant) uniform PushConsts {
    uint cascadeIndex;
} push;

void main() {
    uint objectIndex  = uint(gl_InstanceIndex);
    uint cascadeIndex = push.cascadeIndex;

    mat4 model         = objectData.objects[objectIndex].modelMatrix;
    mat4 lightViewProj = sceneUbo.lightViewProj[cascadeIndex];
    gl_Position = lightViewProj * (model * vec4(inPosition, 1.0));
    gl_Layer    = int(cascadeIndex);
}
