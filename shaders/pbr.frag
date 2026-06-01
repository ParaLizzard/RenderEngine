#version 460
#extension GL_EXT_nonuniform_qualifier : require

const float PI = 3.141592;
const vec3 Fdielectric = vec3(0.04);
const float Epsilon = 0.00001;

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 inTangent;
layout (location = 4) in flat uint inTexID;
layout (location = 5) in vec3 inColor;

layout (location = 0) out vec4 color;

struct Material {
    vec4  albedoFactor;
    vec4  emissiveFactor;
    uint  albedoIndex;
    uint  normalIndex;
    uint  roughnessMetallicIndex;
    uint  emissiveIndex;
    uint  occlusionIndex;
    uint  flags;
    float alphaCutoff;
    float normalScale;
    float roughnessFactor;
    float metallicFactor;
    uint  padding[2];
};

layout(push_constant) uniform PushConsts {
    layout(offset = 128) uint debugIsTransparent;
} push;

layout(set = 0, binding = 0) readonly buffer MaterialBuffer {
    Material materials[];
};

// vec3 in UBO has vec4 alignment — use vec4 and read .xyz to avoid C++ struct misalignment
layout(set = 0, binding = 1) uniform SceneData {
    vec4 cameraPosition;
    vec4 directionalLight;
} scene;

layout(set = 0, binding = 2) uniform samplerCube irradianceMap;
layout(set = 0, binding = 3) uniform samplerCube prefilterMap;
layout(set = 0, binding = 4) uniform sampler2D   brdfLUT;

layout(set = 0, binding = 5) uniform sampler2D textures[];

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

void main() {
    Material mat = materials[inTexID];
    vec4 albedo = texture(textures[nonuniformEXT(mat.albedoIndex)], inUV) * mat.albedoFactor;

    if (push.debugIsTransparent == 1u) {
        if (albedo.a < 0.1) {
            color = vec4(albedo.rgb + vec3(0.0, 0.2, 0.0), 1.0);
            return;
        }
    }

    uint isMask = mat.flags & 1u;
    if (isMask != 0u && albedo.a < mat.alphaCutoff) {
        discard;
    }

    float roughness = texture(textures[nonuniformEXT(mat.roughnessMetallicIndex)], inUV).g * mat.roughnessFactor;
    float metalness = texture(textures[nonuniformEXT(mat.roughnessMetallicIndex)], inUV).b * mat.metallicFactor;

    vec3 T = normalize(inTangent.xyz);
    vec3 N = normalize(inNormal);
    vec3 B = cross(N, T) * inTangent.w;
    mat3 TBN = mat3(T, B, N);
    vec3 tNorm = texture(textures[nonuniformEXT(mat.normalIndex)], inUV).rgb * 2.0 - 1.0;
    tNorm.xy  *= mat.normalScale;
    vec3 worldNormal = normalize(TBN * tNorm);

    vec3 V = normalize(scene.cameraPosition.xyz - inWorldPos);
    vec3 L = normalize(-scene.directionalLight.xyz);
    vec3 H = normalize(V + L);

    float rawNdotL = dot(worldNormal, L);
    float NdotL    = max(rawNdotL, 0.0);
    float NdotV    = max(dot(worldNormal, V), Epsilon);

    vec3 F0 = mix(Fdielectric, albedo.rgb, metalness);

    // --- Direct lighting (only when surface faces the light) ---
    vec3 Lo = vec3(0.0);

    // 1. Calculate the soft wrap diffuse (allows light to wrap around the equator)
    // Tweak this 'wrap' value! 0.0 = hard edge, 0.5 = soft, 1.0 = extreme wrap
    float wrap = 0.5;
    float NdotL_diffuse = clamp((rawNdotL + wrap) / (1.0 + wrap), 0.0, 1.0);

    // 2. Standard Specular (only happens on the lit side)
    float lightRoughness = max(roughness, 0.05);
    float NDF = ndfGGX(max(dot(worldNormal, H), 0.0), lightRoughness);
    float G   = gaSchlickGGX(NdotL, NdotV, lightRoughness);
    vec3  F   = fresnelSchlick(F0, max(dot(H, V), 0.0));

    vec3 specular = (NDF * G * F) / (4.0 * NdotV * NdotL + Epsilon);
    vec3 kD       = (vec3(1.0) - F) * (1.0 - metalness);

    // 3. Combine. Notice specular uses the hard NdotL, diffuse uses the soft NdotL_diffuse!
    Lo = (kD * albedo.rgb / PI * NdotL_diffuse + specular * NdotL) * vec3(2.0);

    // --- Ambient / IBL ---
    vec3 F_ambient  = fresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD_ambient = (vec3(1.0) - F_ambient) * (1.0 - metalness);

    vec3 irradiance      = texture(irradianceMap, worldNormal).rgb;
    vec3 diffuse_ambient = irradiance * albedo.rgb;

    float MAX_REFLECTION_LOD = float(textureQueryLevels(prefilterMap) - 1);
    vec3  R                  = reflect(-V, worldNormal);
    vec3  prefilteredColor   = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2  envBRDF            = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3  specular_ambient   = prefilteredColor * (F_ambient * envBRDF.x + envBRDF.y);

    vec3 ambient    = kD_ambient * diffuse_ambient + specular_ambient;
    vec3 finalColor = ambient + Lo;

    float exposure = 1.2;
    finalColor = vec3(1.0) - exp(-finalColor * exposure);

    color = vec4(finalColor, albedo.a);
}