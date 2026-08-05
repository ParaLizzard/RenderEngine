#version 460

#extension GL_EXT_mesh_shader : require

layout(location = 0) flat in uint inInstanceID;
layout(location = 1) perprimitiveEXT flat in uint inSyntheticIndex;

layout(location = 0) out uvec2 outVisBuffer;

void main() {
    outVisBuffer = uvec2(inInstanceID + 1u, inSyntheticIndex);
}
