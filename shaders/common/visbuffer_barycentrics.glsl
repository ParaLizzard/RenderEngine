#ifndef VISBUFFER_BARYCENTRICS_GLSL
#define VISBUFFER_BARYCENTRICS_GLSL

void decodeSyntheticIndex(uint synth, out uint localMeshletID, out uint triangleIndexOffset) {
    localMeshletID      = (synth >> 9u) & 0x1FFFu;
    triangleIndexOffset = synth & 0x1FFu;
}

vec3 solveBarycentrics(vec4 clip0, vec4 clip1, vec4 clip2, vec2 pixelNDC) {
    vec4 d0 = clip0 - clip2;
    vec4 d1 = clip1 - clip2;

    float Ax = pixelNDC.x * d0.w - d0.x;
    float Bx = pixelNDC.x * d1.w - d1.x;
    float Rx = clip2.x - pixelNDC.x * clip2.w;

    float Ay = pixelNDC.y * d0.w - d0.y;
    float By = pixelNDC.y * d1.w - d1.y;
    float Ry = clip2.y - pixelNDC.y * clip2.w;

    float M = Ax * By - Ay * Bx;
    M = abs(M) < 1e-7 ? (M >= 0.0 ? 1e-7 : -1e-7) : M;

    float u = (Rx * By - Ry * Bx) / M;
    float v = (Ax * Ry - Ay * Rx) / M;
    return vec3(u, v, 1.0 - u - v);
}

void computeQuadUvDerivatives(vec2 texCoord, uint rawInstanceID, uvec2 pixelInQuad, out vec2 ddx_uv, out vec2 ddy_uv) {
    vec2 texCoord_h = subgroupQuadSwapHorizontal(texCoord);
    vec2 texCoord_v = subgroupQuadSwapVertical(texCoord);
    uint prim_h = subgroupQuadSwapHorizontal(rawInstanceID);
    uint prim_v = subgroupQuadSwapVertical(rawInstanceID);

    ddx_uv = (pixelInQuad.x == 0u) ? (texCoord_h - texCoord) : (texCoord - texCoord_h);
    ddy_uv = (pixelInQuad.y == 0u) ? (texCoord_v - texCoord) : (texCoord - texCoord_v);

    if (prim_h != rawInstanceID) {
        ddx_uv = subgroupQuadSwapVertical(ddx_uv);
    }
    if (prim_v != rawInstanceID) {
        ddy_uv = subgroupQuadSwapHorizontal(ddy_uv);
    }
}

vec3 hashColor(uint id) {
    uint h = id * 747796405u + 2891336453u;
    h = (h ^ (h >> 16u)) * 277803737u;
    h = (h ^ (h >> 16u)) * 277803737u;
    h = h ^ (h >> 16u);
    return vec3(float(h & 255u) / 255.0, float((h >> 8u) & 255u) / 255.0, float((h >> 16u) & 255u) / 255.0);
}

#endif
