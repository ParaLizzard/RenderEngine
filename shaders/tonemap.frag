#version 460

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    int debugMode;
    int debugMipLevel;
    vec2 uvScale;
    int tonemapMethod;
    float exposure;
    int agxPunchy;
    int pad;
} pc;

layout(set = 0, binding = 0) uniform sampler2D inputImage;
layout(set = 0, binding = 1) uniform sampler2D hiZImage;

vec3 ACESFilm(vec3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

const mat3 AgXInsetMatrix = mat3(
    0.842479062253094, 0.0423282422610123, 0.0423756549057051,
    0.0784335999999992, 0.878468636469772, 0.0784336,
    0.0792237451477643, 0.0791661274605434, 0.879142973793399
);

const mat3 AgXOutsetMatrix = mat3(
    1.196879024257973, -0.0528968517590742, -0.0529716355227448,
    -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
    -0.0990297440797205, -0.0989611768448433, 1.151073672641819
);

vec3 agxDefaultContrastApprox(vec3 x) {
    vec3 x2 = x * x;
    vec3 x4 = x2 * x2;
    return + 15.5843477169 * (x4 * x2)
           - 34.8211054708 * (x4 * x)
           + 28.5366472099 * x4
           - 9.4674751499 * (x2 * x)
           + 1.2483848488 * x2
           - 0.0401509172 * x
           + 0.0003058863;
}

vec3 AgX(vec3 color, float exposure, bool punchy) {
    const float min_ev = -10.0;
    const float max_ev = 6.5;

    color *= exp2(exposure);

    vec3 val = AgXInsetMatrix * color;

    val = max(val, vec3(1e-10));
    val = clamp((log2(val) - min_ev) / (max_ev - min_ev), 0.0, 1.0);

    val = agxDefaultContrastApprox(val);

    if (punchy) {
        val = pow(max(val, vec3(0.0)), vec3(1.15));
        float luma = dot(val, vec3(0.2126, 0.7152, 0.0722));
        val = max(mix(vec3(luma), val, 1.25), vec3(0.0));
    }

    val = AgXOutsetMatrix * val;

    return clamp(val, 0.0, 1.0);
}

vec3 turboColormap(float x) {
    const vec4 kRedVec4   = vec4(0.13572138,  4.61539260, -42.66032258, 132.13108234);
    const vec4 kGreenVec4 = vec4(0.09140261,  2.19418839,   4.84296658, -14.18503333);
    const vec4 kBlueVec4  = vec4(0.10667330, 12.64194608, -60.58204836, 110.36276771);
    const vec2 kRedVec2   = vec2(-152.94239396,  59.28637943);
    const vec2 kGreenVec2 = vec2(   4.27729857,   2.82956604);
    const vec2 kBlueVec2  = vec2( -89.90310912,  27.34824973);

    x = clamp(x, 0.0, 1.0);
    vec4 v4 = vec4(1.0, x, x * x, x * x * x);
    vec2 v2 = v4.zw * v4.z;

    return clamp(vec3(
        dot(v4, kRedVec4)   + dot(v2, kRedVec2),
        dot(v4, kGreenVec4) + dot(v2, kGreenVec2),
        dot(v4, kBlueVec4)  + dot(v2, kBlueVec2)
    ), 0.0, 1.0);
}

void main() {
    if (pc.debugMode == 1) {
        vec2 hizUV = inUV * pc.uvScale;
        float rawDepth = textureLod(hiZImage, hizUV, float(pc.debugMipLevel)).r;

        if (rawDepth <= 0.000001) {
            outColor = vec4(0.05, 0.02, 0.08, 1.0);
            return;
        }
        if (rawDepth >= 0.999999) {
            outColor = vec4(0.02, 0.04, 0.08, 1.0);
            return;
        }

        const float near = 0.1;
        const float far = 100.0;
        float linearDist = (near * far) / (far - rawDepth * (far - near));

        float t = pow(clamp((linearDist - near) / (40.0 - near), 0.0, 1.0), 0.5);
        outColor = vec4(turboColormap(t), 1.0);
        return;
    }

    vec3 color = texture(inputImage, inUV).rgb;
    vec3 tonemapped;
    if (pc.tonemapMethod == 1) {
        tonemapped = AgX(color, pc.exposure, pc.agxPunchy != 0);
    } else {
        tonemapped = ACESFilm(color);
    }
    outColor = vec4(tonemapped, 1.0);
}
