#version 460

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

struct CompactMaterial {
    uint packedNormal;
    uint packedRadiance;
    uint packedAO;
    uint padding;
};

layout(push_constant) uniform Constants {
    vec2 resolution;
} pc;

layout(set = 0, binding = 0) uniform sampler2D inputImage;

vec3 ACESFilm(vec3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

// Helper function to sample from the SSBO like a texture
/*vec4 sampleSceneColor(vec2 uv) {
    ivec2 coords = ivec2(uv * pc.resolution);
    coords = clamp(coords, ivec2(0), ivec2(pc.resolution) - 1);

    uint index = coords.y * int(pc.resolution.x) + coords.x;
    //uint packedCol = materials[index].packedRadiance;

    return texture(inputImage, uv);
}*/


vec4 sampleSceneColor(vec2 uv) {
    vec4 color = texture(inputImage, uv);

    // Tonemap the HDR color to LDR Linear SDR
    color.rgb = ACESFilm(color.rgb);

    return color;
}

// Standard FXAA 3.11 Quality Parameters
const float FXAA_SUBPIX = 0.0;
const float FXAA_EDGE_THRESHOLD = 0.166;
const float FXAA_EDGE_THRESHOLD_MIN = 0.0833;

const int FXAA_SEARCH_STEPS = 5;
const float FXAA_SEARCH_OFFSETS[5] = float[](1.0, 1.5, 2.0, 4.0, 12.0);

float fxaaLuma(vec4 color) {
    return dot(color.rgb, vec3(0.299, 0.587, 0.114));
}

vec4 applyFXAA(vec2 uv, vec2 rcpFrame) {
    vec4 colorCenter = sampleSceneColor(uv);
    float lumaM = fxaaLuma(colorCenter);

    float lumaS = fxaaLuma(sampleSceneColor(uv + vec2( 0.0,  1.0) * rcpFrame));
    float lumaE = fxaaLuma(sampleSceneColor(uv + vec2( 1.0,  0.0) * rcpFrame));
    float lumaN = fxaaLuma(sampleSceneColor(uv + vec2( 0.0, -1.0) * rcpFrame));
    float lumaW = fxaaLuma(sampleSceneColor(uv + vec2(-1.0,  0.0) * rcpFrame));

    float maxSM = max(lumaS, lumaM);
    float minSM = min(lumaS, lumaM);
    float maxESM = max(lumaE, maxSM);
    float minESM = min(lumaE, minSM);
    float maxWN = max(lumaN, lumaW);
    float minWN = min(lumaN, lumaW);

    float rangeMax = max(maxWN, maxESM);
    float rangeMin = min(minWN, minESM);
    float rangeMaxScaled = rangeMax * FXAA_EDGE_THRESHOLD;
    float range = rangeMax - rangeMin;
    float rangeMaxClamped = max(FXAA_EDGE_THRESHOLD_MIN, rangeMaxScaled);
    if(range < rangeMaxClamped) {
        return colorCenter;
    }

    float lumaNW = fxaaLuma(sampleSceneColor(uv + vec2(-1.0, -1.0) * rcpFrame));
    float lumaSE = fxaaLuma(sampleSceneColor(uv + vec2( 1.0,  1.0) * rcpFrame));
    float lumaNE = fxaaLuma(sampleSceneColor(uv + vec2( 1.0, -1.0) * rcpFrame));
    float lumaSW = fxaaLuma(sampleSceneColor(uv + vec2(-1.0,  1.0) * rcpFrame));

    float lumaNS = lumaN + lumaS;
    float lumaWE = lumaW + lumaE;
    float subpixRcpRange = 1.0 / range;
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

    float subpixNWSWNESE = lumaNWSW + lumaNESE;
    float lengthSign = rcpFrame.x;
    bool horzSpan = edgeHorz >= edgeVert;
    float subpixA = subpixNSWE * 2.0 + subpixNWSWNESE;
    if(!horzSpan) lumaN = lumaW;
    if(!horzSpan) lumaS = lumaE;
    if(horzSpan) lengthSign = rcpFrame.y;
    float subpixB = (subpixA * (1.0 / 12.0)) - lumaM;

    float gradientN = lumaN - lumaM;
    float gradientS = lumaS - lumaM;
    float lumaNN = lumaN + lumaM;
    float lumaSS = lumaS + lumaM;
    bool pairN = abs(gradientN) >= abs(gradientS);
    float gradient = max(abs(gradientN), abs(gradientS));

    if(pairN) lengthSign = -lengthSign;
    float subpixC = clamp(abs(subpixB) * subpixRcpRange, 0.0, 1.0);

    vec2 posB = uv;
    vec2 offNP = horzSpan ? vec2(rcpFrame.x, 0.0) : vec2(0.0, rcpFrame.y);

    if(!horzSpan) posB.x += lengthSign * 0.5;
    if( horzSpan) posB.y += lengthSign * 0.5;

    vec2 posN = posB - offNP * FXAA_SEARCH_OFFSETS[0];
    vec2 posP = posB + offNP * FXAA_SEARCH_OFFSETS[0];

    float subpixD = ((-2.0) * subpixC) + 3.0;
    float subpixE = subpixC * subpixC;

    if(!pairN) lumaNN = lumaSS;
    float gradientScaled = gradient * 0.25;
    float lumaMM = lumaM - lumaNN * 0.5;
    float subpixF = subpixD * subpixE;
    bool lumaMLTZero = lumaMM < 0.0;
    float lumaEndN = fxaaLuma(sampleSceneColor(posN)) - lumaNN * 0.5;
    float lumaEndP = fxaaLuma(sampleSceneColor(posP)) - lumaNN * 0.5;
    bool doneN = abs(lumaEndN) >= gradientScaled;
    bool doneP = abs(lumaEndP) >= gradientScaled;

    for (int i = 1; i < FXAA_SEARCH_STEPS; ++i) {
        if (!doneN) posN -= offNP * FXAA_SEARCH_OFFSETS[i];
        if (!doneP) posP += offNP * FXAA_SEARCH_OFFSETS[i];

        if (doneN && doneP) break;

        if (!doneN) lumaEndN = fxaaLuma(sampleSceneColor(posN)) - lumaNN * 0.5;
        if (!doneP) lumaEndP = fxaaLuma(sampleSceneColor(posP)) - lumaNN * 0.5;

        doneN = abs(lumaEndN) >= gradientScaled;
        doneP = abs(lumaEndP) >= gradientScaled;
    }

    float dstN = horzSpan ? (uv.x - posN.x) : (uv.y - posN.y);
    float dstP = horzSpan ? (posP.x - uv.x) : (posP.y - uv.y);
    bool goodSpanN = (lumaEndN < 0.0) != lumaMLTZero;
    bool goodSpanP = (lumaEndP < 0.0) != lumaMLTZero;
    float spanLength = (dstP + dstN);
    float spanLengthRcp = 1.0 / spanLength;

    bool directionN = dstN < dstP;
    float dstMin = min(dstN, dstP);
    bool goodSpan = directionN ? goodSpanN : goodSpanP;

    float subpixG = subpixF * subpixF;
    float pixelOffset = (dstMin * (-spanLengthRcp)) + 0.5;
    float subpixH = subpixG * FXAA_SUBPIX;

    float pixelOffsetGood = goodSpan ? pixelOffset : 0.0;
    float pixelOffsetSubpix = max(pixelOffsetGood, subpixH);

    vec2 posFinal = uv;
    if(!horzSpan) posFinal.x += pixelOffsetSubpix * lengthSign;
    if( horzSpan) posFinal.y += pixelOffsetSubpix * lengthSign;

    return sampleSceneColor(posFinal);
}

void main(){
    vec2 rcpFrame = 1.0 / pc.resolution;
    outColor = applyFXAA(inUV, rcpFrame);

    //outColor = vec4(inUV.x, inUV.y, 0.0, 1.0);
}