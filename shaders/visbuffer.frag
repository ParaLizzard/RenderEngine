#version 460
layout(location = 0) flat in uint v_DrawID;
layout(location = 0) out uvec2 outVisBuffer;

void main() {
    outVisBuffer = uvec2(v_DrawID, uint(gl_PrimitiveID));
}