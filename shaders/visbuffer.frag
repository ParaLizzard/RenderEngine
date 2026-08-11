#version 460

layout(location = 0) flat in uint inInstanceID;
layout(location = 1) flat in uint inSyntheticIndex;
layout(location = 2) in vec4 inCurClipPos;
layout(location = 3) in vec4 inPrevClipPos;

layout(location = 0) out uvec2 outVisBuffer;
layout(location = 1) out vec2 outVelocity;

void main() {
    outVisBuffer = uvec2(inInstanceID + 1u, inSyntheticIndex);
    vec2 curNDC  = inCurClipPos.xy / inCurClipPos.w;
    vec2 prevNDC = inPrevClipPos.xy / inPrevClipPos.w;
    outVelocity  = (curNDC - prevNDC) * 0.5;
}