#version 460

layout(location = 0) flat in uint v_DrawID;
layout(location = 0) out uvec2 outVisBuffer;

void main() {
    uint instanceID = v_DrawID;
    uint primitiveID = uint(gl_PrimitiveID);

    outVisBuffer = uvec2(instanceID, primitiveID);
}