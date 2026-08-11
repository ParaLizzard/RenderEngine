#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_8bit_storage : require

layout(location = 0) flat in uint inInstanceID;
layout(location = 1) flat in uint inSyntheticIndex;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inCurClipPos;
layout(location = 4) in vec4 inPrevClipPos;

layout(location = 0) out uvec2 outVisBuffer;
layout(location = 1) out vec2 outVelocity;

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

struct MaterialData {
    vec4  albedoFactor;
    vec4  emissiveFactor;
    uint  albedoIndex;
    uint  normalIndex;
    uint  roughnessMetallicIndex;
    uint  emissiveIndex;
    uint  occlusionIndex;
    uint  flags;
    float alphaCutoff;
    float normalScale;
    float roughnessFactor;
    float metallicFactor;
    uint  padding0;
    uint  padding1;
};

struct VertexAttribute {
    float colorR, colorG, colorB;
    float normalX, normalY, normalZ;
    float uvX, uvY;
    float tangentX, tangentY, tangentZ, tangentW;
    uint  texId;
};

layout(set = 0, binding = 0) readonly buffer MaterialHeapBuffer { MaterialData materials[]; } materialHeap;
layout(set = 0, binding = 5) readonly buffer ObjectBuffer       { ObjectData objects[];    } objectBuffer;
layout(set = 0, binding = 7) readonly buffer AttributeBuffer    { VertexAttribute attributes[]; } attributeBuffer;
layout(set = 0, binding = 8) readonly buffer MeshletBuffer      { Meshlet meshlets[];       } meshletBuffer;
layout(set = 0, binding = 9) readonly buffer MeshletVertexMap   { uint meshletVertices[];   } meshletVertexMap;
layout(set = 0, binding = 10) readonly buffer MeshletTriangleMap { uint8_t meshletTriangles[]; } meshletTriangleMap;
layout(set = 0, binding = 12) uniform sampler2D bindlessTextures[];

void main() {
    uint objectID            = inSyntheticIndex >> 22u;
    uint localMeshletID      = (inSyntheticIndex >> 9u) & 0x1FFFu;
    uint triangleIndexOffset = inSyntheticIndex & 0x1FFu;

    uint triBase = (triangleIndexOffset / 3u) * 3u;

    uint meshletID = objectBuffer.objects[objectID].baseMeshlet + localMeshletID;
    Meshlet m = meshletBuffer.meshlets[meshletID];

    uint localByte0   = uint(meshletTriangleMap.meshletTriangles[m.indexOffset + triBase]);
    uint globalVert0  = meshletVertexMap.meshletVertices[m.vertexOffset + localByte0];

    VertexAttribute a0 = attributeBuffer.attributes[globalVert0];
    uint materialId    = a0.texId;

    MaterialData mat = materialHeap.materials[materialId];

    float alpha = mat.albedoFactor.a;
    if (mat.albedoIndex != 0xFFFFFFFFu) {
        alpha *= texture(bindlessTextures[nonuniformEXT(mat.albedoIndex)], inUV).a;
    }

    if (alpha < mat.alphaCutoff) {
        discard;
    }

    outVisBuffer = uvec2(inInstanceID + 1u, inSyntheticIndex);
    vec2 curNDC  = inCurClipPos.xy / inCurClipPos.w;
    vec2 prevNDC = inPrevClipPos.xy / inPrevClipPos.w;
    outVelocity  = (curNDC - prevNDC) * 0.5;
}
