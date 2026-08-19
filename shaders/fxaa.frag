#version 460

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Constants {
    vec2 resolution;
} pc;

layout(set = 0, binding = 0) uniform sampler2D inputImage;

float fxaaLuma(vec3 rgb) {
    return sqrt(clamp(dot(rgb, vec3(0.299, 0.587, 0.114)), 0.0, 1.0));
}

vec3 sampleColor(vec2 uv) {
    return texture(inputImage, uv).rgb;
}

const float FXAA_EDGE_THRESHOLD_MIN = 0.0312;
const float FXAA_EDGE_THRESHOLD_MAX = 0.063;
const float FXAA_SUBPIX_QUALITY = 0.75;

const int EXTRA_STEPS = 12;
const float SEARCH_STEPS[12] = float[](1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0);

vec3 applyFXAA(vec2 uv, vec2 rcpFrame) {
    vec3 colorCenter = sampleColor(uv);
    float lumaM = fxaaLuma(colorCenter);

    float lumaS = fxaaLuma(sampleColor(uv + vec2( 0.0,  1.0) * rcpFrame));
    float lumaE = fxaaLuma(sampleColor(uv + vec2( 1.0,  0.0) * rcpFrame));
    float lumaN = fxaaLuma(sampleColor(uv + vec2( 0.0, -1.0) * rcpFrame));
    float lumaW = fxaaLuma(sampleColor(uv + vec2(-1.0,  0.0) * rcpFrame));

    float maxSM = max(lumaS, lumaM);
    float minSM = min(lumaS, lumaM);
    float maxESM = max(lumaE, maxSM);
    float minESM = min(lumaE, minSM);
    float maxWN = max(lumaN, lumaW);
    float minWN = min(lumaN, lumaW);

    float rangeMax = max(maxWN, maxESM);
    float rangeMin = min(minWN, minESM);
    float range = rangeMax - rangeMin;

    if (range < max(FXAA_EDGE_THRESHOLD_MIN, rangeMax * FXAA_EDGE_THRESHOLD_MAX)) {
        return colorCenter;
    }

    float lumaNW = fxaaLuma(sampleColor(uv + vec2(-1.0, -1.0) * rcpFrame));
    float lumaSE = fxaaLuma(sampleColor(uv + vec2( 1.0,  1.0) * rcpFrame));
    float lumaNE = fxaaLuma(sampleColor(uv + vec2( 1.0, -1.0) * rcpFrame));
    float lumaSW = fxaaLuma(sampleColor(uv + vec2(-1.0,  1.0) * rcpFrame));

    float lumaNS = lumaN + lumaS;
    float lumaWE = lumaW + lumaE;
    float subpixNSWE = lumaNS + lumaWE;
    float edgeHorz1 = (-2.0 * lumaM) + lumaNS;
    float edgeVert1 = (-2.0 * lumaM) + lumaWE;

    float lumaNESE = lumaNE + lumaSE;
    float lumaNWNE = lumaNW + lumaNE;
    float edgeHorz2 = (-2.0 * lumaE) + lumaNESE;
    float edgeVert2 = (-2.0 * lumaN) + lumaNWNE;

    float lumaNWSW = lumaNW + lumaSW;
    float lumaSWSE = lumaSW + lumaSE;
    float edgeHorz4 = (abs(edgeHorz1) * 2.0) + abs(edgeHorz2);
    float edgeVert4 = (abs(edgeVert1) * 2.0) + abs(edgeVert2);
    float edgeHorz3 = (-2.0 * lumaW) + lumaNWSW;
    float edgeVert3 = (-2.0 * lumaS) + lumaSWSE;
    float edgeHorz = abs(edgeHorz3) + edgeHorz4;
    float edgeVert = abs(edgeVert3) + edgeVert4;

    bool horzSpan = edgeHorz >= edgeVert;

    float luma1 = horzSpan ? lumaN : lumaW;
    float luma2 = horzSpan ? lumaS : lumaE;

    float gradient1 = luma1 - lumaM;
    float gradient2 = luma2 - lumaM;

    bool is1Steeper = abs(gradient1) >= abs(gradient2);
    float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));

    float stepLength = horzSpan ? rcpFrame.y : rcpFrame.x;
    float lumaLocalAverage = 0.0;

    if (is1Steeper) {
        stepLength = -stepLength;
        lumaLocalAverage = 0.5 * (luma1 + lumaM);
    } else {
        lumaLocalAverage = 0.5 * (luma2 + lumaM);
    }

    vec2 currentUv = uv;
    if (horzSpan) {
        currentUv.y += stepLength * 0.5;
    } else {
        currentUv.x += stepLength * 0.5;
    }

    vec2 offset = horzSpan ? vec2(rcpFrame.x, 0.0) : vec2(0.0, rcpFrame.y);

    vec2 uvP = currentUv + offset;
    vec2 uvN = currentUv - offset;

    float lumaEndP = fxaaLuma(sampleColor(uvP)) - lumaLocalAverage;
    float lumaEndN = fxaaLuma(sampleColor(uvN)) - lumaLocalAverage;

    bool doneP = abs(lumaEndP) >= gradientScaled;
    bool doneN = abs(lumaEndN) >= gradientScaled;

    if (!doneP) uvP += offset * SEARCH_STEPS[0];
    if (!doneN) uvN -= offset * SEARCH_STEPS[0];

    for (int i = 1; i < EXTRA_STEPS; i++) {
        if (!doneP) {
            lumaEndP = fxaaLuma(sampleColor(uvP)) - lumaLocalAverage;
            doneP = abs(lumaEndP) >= gradientScaled;
            if (!doneP) uvP += offset * SEARCH_STEPS[i];
        }
        if (!doneN) {
            lumaEndN = fxaaLuma(sampleColor(uvN)) - lumaLocalAverage;
            doneN = abs(lumaEndN) >= gradientScaled;
            if (!doneN) uvN -= offset * SEARCH_STEPS[i];
        }
        if (doneP && doneN) break;
    }

    float distanceP = horzSpan ? (uvP.x - uv.x) : (uvP.y - uv.y);
    float distanceN = horzSpan ? (uv.x - uvN.x) : (uv.y - uvN.y);

    bool isDirection1 = distanceP < distanceN;
    float distanceFinal = min(distanceP, distanceN);

    float edgeThickness = distanceP + distanceN;

    float pixelOffset = -distanceFinal / edgeThickness + 0.5;

    bool isLumaCenterSmaller = lumaM < lumaLocalAverage;
    bool correctVariation = ((isDirection1 ? lumaEndP : lumaEndN) < 0.0) != isLumaCenterSmaller;

    float finalOffset = correctVariation ? pixelOffset : 0.0;

    float lumaAverage = (1.0 / 12.0) * (2.0 * (lumaNS + lumaWE) + lumaNWSW + lumaNESE);
    float subPixelOffset1 = clamp(abs(lumaAverage - lumaM) / range, 0.0, 1.0);
    float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;
    float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * FXAA_SUBPIX_QUALITY;

    finalOffset = max(finalOffset, subPixelOffsetFinal);

    vec2 finalUv = uv;
    if (horzSpan) {
        finalUv.y += finalOffset * stepLength;
    } else {
        finalUv.x += finalOffset * stepLength;
    }

    return sampleColor(finalUv);
}

void main() {
    vec2 rcpFrame = 1.0 / pc.resolution;
    vec3 color = applyFXAA(inUV, rcpFrame);
    outColor = vec4(color, 1.0);
}