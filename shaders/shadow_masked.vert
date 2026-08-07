#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shader_viewport_layer_array : require
#extension GL_EXT_multiview : enable

layout(location = 0) in  vec3 inPosition;
layout(location = 0) out vec2 outUV;
layout(location = 1) flat out uint outMaterialID;

struct ObjectData {
    mat4 modelMatrix;
    mat4 normalMatrix;
    vec4 boundingSphere;
    uint baseMeshlet;
    uint meshletCount;
    uint alphaMode;
    uint padding1;
};

struct VertexAttribute {
    float colorR, colorG, colorB;
    float normalX, normalY, normalZ;
    float uvX, uvY;
    float tangentX, tangentY, tangentZ, tangentW;
    uint texId;
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

layout(set = 0, binding = 5) readonly buffer ObjectDataBuffer {
    ObjectData objects[];
} objectData;

layout(set = 0, binding = 7) readonly buffer AttributeBuffer {
    VertexAttribute attributes[];
} attributeBuffer;

void main() {
    uint objectIndex = uint(gl_InstanceIndex);
    uint viewIndex   = uint(gl_ViewIndex);

    mat4 model         = objectData.objects[objectIndex].modelMatrix;
    mat4 lightViewProj = sceneUbo.lightViewProj[viewIndex];
    gl_Position        = lightViewProj * (model * vec4(inPosition, 1.0));

    VertexAttribute attr = attributeBuffer.attributes[gl_VertexIndex];
    outUV         = vec2(attr.uvX, attr.uvY);
    outMaterialID = attr.texId;
}
