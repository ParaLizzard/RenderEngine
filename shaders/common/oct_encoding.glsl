#ifndef OCT_ENCODING_GLSL
#define OCT_ENCODING_GLSL

uint encodeOctNormal(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    vec2 res = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * sign(n.xy);
    uvec2 packedInput = uvec2(round(clamp(res * 0.5 + 0.5, 0.0, 1.0) * 255.0));
    return packedInput.x | (packedInput.y << 8u);
}

vec3 decodeOctNormal(uint packedNormal) {
    vec2 f = vec2(float(packedNormal & 255u), float((packedNormal >> 8u) & 255u)) / 255.0;
    f = f * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = clamp(-n.z, 0.0, 1.0);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

vec2 encodeOctNormalVec2(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    vec2 res = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * sign(n.xy);
    return res * 0.5 + 0.5;
}

vec3 decodeOctNormalVec2(vec2 enc) {
    vec2 f = enc * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = clamp(-n.z, 0.0, 1.0);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

#endif
