#ifndef SHADOW_CSM_GLSL
#define SHADOW_CSM_GLSL

const vec2 poissonDisk[16] = vec2[]( 
    vec2(-0.38277543,  0.27676845),
    vec2( 0.79197514,  0.19090188),
    vec2(-0.09418410, -0.92938870),
    vec2(-0.81409955,  0.91437590),
    vec2( 0.34495938,  0.29387760),
    vec2(-0.94201624, -0.39906216),
    vec2( 0.44323325, -0.97511554),
    vec2( 0.19984126,  0.78641367),
    vec2( 0.94558609, -0.76890725),
    vec2(-0.91588581,  0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2( 0.97484398,  0.75648379),
    vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2(-0.24188840,  0.99706507),
    vec2( 0.14383161, -0.14100790)
);

const vec4 cascadeAtlas[3] = vec4[](
    vec4(1.0, 2.0 / 3.0, 0.0, 0.0),
    vec4(0.5, 1.0 / 3.0, 0.0, 2.0 / 3.0),
    vec4(0.5, 1.0 / 3.0, 0.5, 2.0 / 3.0)
);

uint selectCascadeDithered(float viewDepth, float dither, vec4 splits) {
    float blendBand = 0.2;
    float ditheredDepth = viewDepth + (dither - 0.5) * blendBand;
    if (ditheredDepth > splits[1]) return 2u;
    if (ditheredDepth > splits[0]) return 1u;
    return 0u;
}

float evaluatePCF(sampler2DShadow shadowMap, vec4 shadowCoord, uint cascadeIndex, float bias, vec2 texelSize, uint samplesC0, uint samplesC1, uint samplesC2) {
    uint numPcfTaps = (cascadeIndex == 0u) ? samplesC0 : ((cascadeIndex == 1u) ? samplesC1 : samplesC2);
    numPcfTaps = clamp(numPcfTaps, 1u, 16u);

    float shadow = 0.0;
    float compareDepth = shadowCoord.z - bias;
    vec2 baseAtlasUV = shadowCoord.xy * cascadeAtlas[cascadeIndex].xy + cascadeAtlas[cascadeIndex].zw;
    vec2 minUV = cascadeAtlas[cascadeIndex].zw;
    vec2 maxUV = cascadeAtlas[cascadeIndex].zw + cascadeAtlas[cascadeIndex].xy;

    float filterRadius = (cascadeIndex == 0u) ? 1.5 : ((cascadeIndex == 1u) ? 1.0 : 0.8);
    vec2 tapScale = texelSize * filterRadius;
    vec2 innerMinUV = minUV + tapScale;
    vec2 innerMaxUV = maxUV - tapScale;

    if (baseAtlasUV.x >= innerMinUV.x && baseAtlasUV.x <= innerMaxUV.x &&
        baseAtlasUV.y >= innerMinUV.y && baseAtlasUV.y <= innerMaxUV.y) {
        for (uint i = 0u; i < numPcfTaps; i++) {
            shadow += texture(shadowMap, vec3(baseAtlasUV + poissonDisk[i] * tapScale, compareDepth));
        }
    } else {
        for (uint i = 0u; i < numPcfTaps; i++) {
            vec2 sampleUV = clamp(baseAtlasUV + poissonDisk[i] * tapScale, minUV, maxUV);
            shadow += texture(shadowMap, vec3(sampleUV, compareDepth));
        }
    }
    return shadow / float(numPcfTaps);
}

float evaluatePCSS(sampler2D depthMap, sampler2DShadow shadowMap, vec4 shadowCoord, uint cascadeIndex, float bias, vec2 texelSize, uint samplesC0, uint samplesC1, uint samplesC2) {
    float scale = (cascadeIndex == 0u) ? 1.0 : ((cascadeIndex == 1u) ? 0.25 : 0.05);
    int numBlockerTaps = (cascadeIndex == 0u) ? 16 : ((cascadeIndex == 1u) ? 8 : 4);
    uint numPcfTaps = (cascadeIndex == 0u) ? samplesC0 : ((cascadeIndex == 1u) ? samplesC1 : samplesC2);
    numPcfTaps = clamp(numPcfTaps, 1u, 16u);

    float searchRadius = 12.0 * scale; 
    float avgBlockerDepth = 0.0;
    int numBlockers = 0;
    vec2 baseAtlasUV = shadowCoord.xy * cascadeAtlas[cascadeIndex].xy + cascadeAtlas[cascadeIndex].zw;
    vec2 minUV = cascadeAtlas[cascadeIndex].zw;
    vec2 maxUV = cascadeAtlas[cascadeIndex].zw + cascadeAtlas[cascadeIndex].xy;

    for (int i = 0; i < numBlockerTaps; i++) {
        vec2 offset = poissonDisk[i] * texelSize * searchRadius;
        vec2 sampleUV = clamp(baseAtlasUV + offset, minUV, maxUV);
        float depth = texture(depthMap, sampleUV).r;
        if (depth < shadowCoord.z - bias) {
            avgBlockerDepth += depth;
            numBlockers++;
        }
    }

    if (numBlockers == 0) return 1.0;

    avgBlockerDepth /= float(numBlockers);
    float depthDiff = max(shadowCoord.z - avgBlockerDepth, 0.0);
    float pcfRadius = clamp(depthDiff * 2500.0 * scale, 0.001, 16.0 * scale); 

    float illuminated = 0.0;
    float compareDepth = shadowCoord.z - bias;
    for (uint i = 0u; i < numPcfTaps; i++) {
        vec2 offset = poissonDisk[i] * texelSize * pcfRadius;
        vec2 sampleUV = clamp(baseAtlasUV + offset, minUV, maxUV);
        illuminated += texture(shadowMap, vec3(sampleUV, compareDepth));
    }
    return illuminated / float(numPcfTaps);
}

#endif
