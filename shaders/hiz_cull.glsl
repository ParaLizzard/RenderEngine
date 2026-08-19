#ifndef HIZ_CULL_GLSL
#define HIZ_CULL_GLSL

bool isSphereOccludedHiZ(
    sampler2D hizPyramid,
    vec3 centerView,
    float radius,
    vec4 projParams,
    vec4 hizParams,
    vec2 screenParams
) {
    float zNearView = centerView.z - radius;
    if (zNearView <= hizParams.w || centerView.z <= radius) {
        return false;
    }

    float p00 = projParams.x;
    float p11 = projParams.y;
    
    float sqXZ = centerView.x * centerView.x + centerView.z * centerView.z;
    float sqYZ = centerView.y * centerView.y + centerView.z * centerView.z;
    float r2 = radius * radius;

    if (sqXZ <= r2 || sqYZ <= r2) {
        return false;
    }

    float boxWidth  = (radius * p00) / sqrt(sqXZ - r2);
    float boxHeight = (radius * p11) / sqrt(sqYZ - r2);
    vec2 centerNDC  = vec2((centerView.x * p00) / centerView.z, (centerView.y * p11) / centerView.z);

    vec4 screenUVBox = vec4(
        clamp(0.5 + 0.5 * (centerNDC.x - boxWidth),  0.0, 1.0),
        clamp(0.5 + 0.5 * (centerNDC.y - boxHeight), 0.0, 1.0),
        clamp(0.5 + 0.5 * (centerNDC.x + boxWidth),  0.0, 1.0),
        clamp(0.5 + 0.5 * (centerNDC.y + boxHeight), 0.0, 1.0)
    );

    vec2 sizePx = (screenUVBox.zw - screenUVBox.xy) * screenParams;
    float maxDim = max(sizePx.x, sizePx.y);
    if (maxDim < 0.5 || maxDim > 128.0) {
        return false;
    }

    float mipLevel = clamp(ceil(log2(max(maxDim * 0.5, 1.0))), 0.0, 4.0);

    vec4 hizUVBox = vec4(
        screenUVBox.xy * hizParams.xy,
        screenUVBox.zw * hizParams.xy
    );

    float d00 = textureLod(hizPyramid, hizUVBox.xy, mipLevel).r;
    float d10 = textureLod(hizPyramid, hizUVBox.zy, mipLevel).r;
    float d01 = textureLod(hizPyramid, hizUVBox.xw, mipLevel).r;
    float d11 = textureLod(hizPyramid, hizUVBox.zw, mipLevel).r;

    float maxOccluderDepth = max(max(d00, d10), max(d01, d11));

    float zNearNDC = (projParams.z * zNearView + projParams.w) / zNearView;

    const float DEPTH_BIAS = 0.0005;
    return (zNearNDC > (maxOccluderDepth + DEPTH_BIAS));
}

#endif
