#version 460


layout(location = 0) flat in uint inInstanceID;
layout(location = 0) out uvec2 outVisBuffer;

void main() {
    uint instanceID = inInstanceID;
    uint primitiveID = uint(gl_PrimitiveID);

    outVisBuffer = uvec2(instanceID, primitiveID);
}