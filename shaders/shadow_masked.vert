#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_shader_8bit_storage : require

layout(push_constant) uniform PushConsts {
    uint cascadeIndex;
} push;

layout(location = 0) out vec2 outUV;
layout(location = 1) flat out uint outMaterialID;

struct PositionData {
    float x, y, z;
};

struct Meshlet {
    float center_x, center_y, center_z, radius;
    uint cone_axis_cutoff;
    uint vertexOffset;
    uint indexOffset;
    uint vertexCount;
    uint triangleCount;
};

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
    mat4 viewProj;
    mat4 prevViewProj;
    vec4 frustumPlanes[6];
    vec4 cameraPosition;
    vec4 directionalLight;
    mat4 lightViewProj[3];
    vec4 cascadesSplits;
    float maxReflectionLod;
    uint blueNoiseTexIndex;
    vec2 padding;
} sceneUbo;

// Set 0: MegaBuffer geometry
layout(set = 0, binding = 5) readonly buffer ObjectBuffer { ObjectData objects[]; };
layout(set = 0, binding = 6) readonly buffer VertexBuffer { PositionData positions[]; };
layout(set = 0, binding = 7) readonly buffer AttributeBuffer { VertexAttribute attributes[]; };
layout(set = 0, binding = 8) readonly buffer MeshletBuffer { Meshlet meshlets[]; };
layout(set = 0, binding = 9) readonly buffer MeshletVertexMap { uint meshletVertices[]; };
layout(set = 0, binding = 10) readonly buffer MeshletTriangleMap { uint8_t meshletTriangles[]; };

// Set 1: Pass/Cull storage
layout(set = 1, binding = 3) readonly buffer CompactedIndexBuffer { uint syntheticIndices[]; };

void main() {
    uint syntheticIndex = syntheticIndices[gl_VertexIndex];

    uint objectID            = syntheticIndex >> 22u;
    uint localMeshletID      = (syntheticIndex >> 9u) & 0x1FFFu;
    uint triangleIndexOffset = syntheticIndex & 0x1FFu;

    uint meshletID = objects[objectID].baseMeshlet + localMeshletID;
    Meshlet m = meshlets[meshletID];

    uint localTriByte   = uint(meshletTriangles[m.indexOffset + triangleIndexOffset]);
    uint globalVertexID = meshletVertices[m.vertexOffset + localTriByte];

    PositionData pd = positions[globalVertexID];
    VertexAttribute attr = attributes[globalVertexID];

    outUV         = vec2(attr.uvX, attr.uvY);
    outMaterialID = attr.texId;

    uint viewIndex   = push.cascadeIndex;
    mat4 model         = objects[objectID].modelMatrix;
    gl_Position        = sceneUbo.lightViewProj[viewIndex] * (model * vec4(pd.x, pd.y, pd.z, 1.0));
}
