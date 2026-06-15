#version 460

layout(location = 0) out uvec2 outVisBuffer;

void main() {
    uint instanceID = gl_InstanceIndex;
    uint primitiveID = gl_PrimitiveID;

    outVisBuffer = uvec2(instanceID, primitiveID);
}