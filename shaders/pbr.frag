#version 460
#extension GL_EXT_nonuniform_qualifier : require

const float PI = 3.141592;
const vec3 Fdielectric = vec3(0.04);
const float Epsilon = 0.00001;

layout (location = 0) in vec2 inUV; // from fullscreen.vert
layout (location = 0) out vec4 color;

// --- Set 0: Scene Data & IBL ---
layout(set = 0, binding = 1) uniform SceneData {
    vec4 cameraPosition;
    vec4 directionalLight;
    float maxReflectionLod;
    uint blueNoiseTexIndex;
} scene;

layout(set = 0, binding = 2) uniform samplerCube irradianceMap;
layout(set = 0, binding = 3) uniform samplerCube prefilterMap;
layout(set = 0, binding = 4) uniform sampler2D   brdfLUT;
layout(set = 0, binding = 5) uniform sampler2D   textures[]; // For blue noise

// --- Set 1: G-Buffer Inputs ---
layout(set = 1, binding = 0) uniform sampler2D gAlbedoMetallic;
layout(set = 1, binding = 1) uniform sampler2D gNormalRoughness;
layout(set = 1, binding = 2) uniform sampler2D gDepth;
layout(set = 1, binding = 3) uniform sampler2D gSsao;

layout(push_constant) uniform PushConsts {
    mat4 invViewProjection;
    uint debugIsTransparent;
    uint ssao;
} push;

// --- PBR Helper Functions ---
float ndfGGX(float cosLh, float roughness) {
    float alpha   = roughness * roughness;
    float alphaSq = alpha * alpha;
    float denom   = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
    return alphaSq / (PI * denom * denom);
}

float gaSchlickG1(float cosTheta, float k) {
    return cosTheta / (cosTheta * (1.0 - k) + k);
}

float gaSchlickGGX(float cosLi, float cosLo, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return gaSchlickG1(cosLi, k) * gaSchlickG1(cosLo, k);
}

vec3 fresnelSchlick(vec3 F0, float cosTheta) {
    return F0 + (vec3(1.0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 agxDefaultContrastApprox(vec3 x) {
    vec3 x2 = x * x;
    vec3 x4 = x2 * x2;
    return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}

vec3 agx(vec3 color) {
    const mat3 agx_mat = mat3(
    0.842479062253094, 0.0423282422610123, 0.0423756549057051,
    0.0784335999999992,  0.878468636469772,  0.0784336,
    0.0792237451477643, 0.0791661274605434, 0.879142973793104);
    const mat3 agx_mat_inv = mat3(
    1.19687900512017, -0.0528968517574562, -0.0529716355144438,
    -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
    -0.0990297440797205, -0.0989611768448433, 1.15107367264116);

    const float minEv = -12.47393;
    const float maxEv = 4.026069;

    color = agx_mat * color;
    color = clamp(log2(max(color, 1e-10)), minEv, maxEv);
    color = (color - minEv) / (maxEv - minEv);
    color = agxDefaultContrastApprox(color);
    color = agx_mat_inv * color;

    return clamp(color, 0.0, 1.0);
}

// Reconstruct world position from depth
vec3 reconstructWorldPos(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 worldPos = push.invViewProjection * ndc;
    return worldPos.xyz / worldPos.w;
}

void main() {
    float depth = texture(gDepth, inUV).r;

    // Background discard (assuming clear depth is 1.0)
    if (depth >= 1.0) {
        color = vec4(0.02, 0.02, 0.02, 1.0);
        return;
    }

    // Unpack G-Buffer
    vec4 albedoMetallic = texture(gAlbedoMetallic, inUV);
    vec3 albedo = albedoMetallic.rgb;
    float metalness = albedoMetallic.a;

    vec4 normalRoughness = texture(gNormalRoughness, inUV);
    vec3 worldNormal = normalize(normalRoughness.xyz * 2.0 - 1.0);

    float roughness = normalRoughness.a;

    vec3 worldPos = reconstructWorldPos(inUV, depth);

    // AO handling
    ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
    float totalAo = texture(gSsao, inUV).r;
    if(push.ssao == 0) totalAo = 1.0;

    // Direct Lighting Variables
    vec3 V = normalize(scene.cameraPosition.xyz - worldPos);
    vec3 L = normalize(-scene.directionalLight.xyz);
    vec3 H = normalize(V + L);

    float rawNdotL = dot(worldNormal, L);
    float NdotL    = max(rawNdotL, 0.0);
    float NdotV    = max(dot(worldNormal, V), Epsilon);

    vec3 F0 = mix(Fdielectric, albedo.rgb, metalness);

    // Direct Lighting Calculation
    float wrap = 0.5;
    float NdotL_diffuse = clamp((rawNdotL + wrap) / (1.0 + wrap), 0.0, 1.0);
    float lightRoughness = max(roughness, 0.05);

    float NDF = ndfGGX(max(dot(worldNormal, H), 0.0), lightRoughness);
    float G   = gaSchlickGGX(NdotL, NdotV, lightRoughness);
    vec3  F   = fresnelSchlick(F0, max(dot(H, V), 0.0));

    vec3 specular = (NDF * G * F) / (4.0 * NdotV * NdotL + Epsilon);
    vec3 kD       = (vec3(1.0) - F) * (1.0 - metalness);

    vec3 Lo = (kD * albedo.rgb / PI * NdotL_diffuse + specular * NdotL) * vec3(2.0);

    // IBL / Ambient Calculation
    vec3 F_ambient  = fresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD_ambient = (vec3(1.0) - F_ambient) * (1.0 - metalness);

    vec3 sampleN = vec3(worldNormal.x, -worldNormal.y, worldNormal.z);
    vec3 irradiance = texture(irradianceMap, sampleN).rgb;
    vec3 diffuse_ambient = irradiance * albedo.rgb;

    float MAX_REFLECTION_LOD = scene.maxReflectionLod;
    vec3 R = reflect(-V, worldNormal);
    vec3 sampleR = vec3(R.x, -R.y, R.z);

    vec3 prefilteredColor = textureLod(prefilterMap, sampleR, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 envBRDF = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specular_ambient = prefilteredColor * (F_ambient * envBRDF.x + envBRDF.y);

    vec3 ambient = (kD_ambient * diffuse_ambient + specular_ambient) * totalAo;
    vec3 finalColor = ambient + Lo;

    // Tonemapping
    float exposure = 0.1;
    finalColor *= exposure;
    finalColor = agx(finalColor);

    // Dither
    ivec2 noiseSize = textureSize(textures[nonuniformEXT(scene.blueNoiseTexIndex)], 0);
    ivec2 noiseCoord = pixelCoord % noiseSize;
    float dither = texelFetch(textures[nonuniformEXT(scene.blueNoiseTexIndex)], noiseCoord, 0).r - 0.5;
    finalColor += dither / 255.0;

    color = vec4(finalColor, 1.0);
}