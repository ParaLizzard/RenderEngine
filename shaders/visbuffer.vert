#version 460

#extension GL_EXT_shader_8bit_storage : require

layout(location = 0) flat out uint outInstanceID;
layout(location = 1) flat out uint outSyntheticIndex;

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

// Set 0: MegaBuffer geometry
layout(set = 0, binding = 5) readonly buffer ObjectBuffer { ObjectData objects[]; };
layout(set = 0, binding = 6) readonly buffer VertexBuffer { PositionData positions[]; };
layout(set = 0, binding = 8) readonly buffer MeshletBuffer { Meshlet meshlets[]; };
layout(set = 0, binding = 9) readonly buffer MeshletVertexMap { uint meshletVertices[]; };
layout(set = 0, binding = 10) readonly buffer MeshletTriangleMap { uint8_t meshletTriangles[]; };

// Set 1: Pass/Cull storage
layout(set = 1, binding = 3) readonly buffer CompactedIndexBuffer { uint syntheticIndices[]; };

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
} pc;

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

    outInstanceID = objectID;
    outSyntheticIndex = syntheticIndex;

    mat4 model = objects[objectID].modelMatrix;
    gl_Position = pc.viewProj * (model * vec4(pd.x, pd.y, pd.z, 1.0));
}