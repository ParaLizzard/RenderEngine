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

layout(set = 0, binding = 0) readonly buffer MaterialBuffer {
    Material materials[];
};

layout(set = 0, binding = 1) uniform sampler2D textures[];

float ndfGGX(float cosLh, float roughness) {
    float alpha   = roughness * roughness;
    float alphaSq = alpha * alpha;
    float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
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
    return F0 + (vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 linear_rgb_to_oklab(vec3 c)
{
    // Step 1: Approximate cone response (LMS color space) via matrix multiplication
    float l = 0.4122214708f * c.r + 0.5363325363f * c.g + 0.0514459929f * c.b;
    float m = 0.2119034982f * c.r + 0.6806995451f * c.g + 0.1073969566f * c.b;
    float s = 0.0883024619f * c.r + 0.2817188376f * c.g + 0.6299787005f * c.b;

    // Step 2: Apply the non-linear cube root (perceptual response)
    // Note: sign() and abs() handle negative inputs safely if you have out-of-gamut/HDR values
    float l_ = sign(l) * pow(abs(l), 1.0f / 3.0f);
    float m_ = sign(m) * pow(abs(m), 1.0f / 3.0f);
    float s_ = sign(s) * pow(abs(s), 1.0f / 3.0f);

    // Step 3: Rotate into the final Oklab components (L, a, b)
    return vec3(
    0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
    1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
    0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_
    );
}

vec3 oklab_to_linear_rgb(vec3 c)
{
    // Step 1: Undo the Oklab rotation matrix to get back to non-linear LMS
    float l_ = c.x + 0.3963377774f * c.y + 0.2158037573f * c.z;
    float m_ = c.x - 0.1055613458f * c.y - 0.0638541728f * c.z;
    float s_ = c.x - 0.0894841775f * c.y - 1.2914855414f * c.z;

    // Step 2: Invert the cube root (raise to power of 3)
    float l = l_ * l_ * l_;
    float m = m_ * m_ * m_;
    float s = s_ * s_ * s_;

    // Step 3: Convert LMS back into standard Linear RGB
    return vec3(
    +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
    -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
    -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s
    );
}

void main() {
    Material mat = materials[inTexID];

    vec4 albedo = texture(textures[nonuniformEXT(mat.albedoIndex)], inUV) * mat.albedoFactor;
    float roughness = texture(textures[nonuniformEXT(mat.roughnessMetallicIndex)], inUV).g * mat.roughnessFactor;
    float metalness = texture(textures[nonuniformEXT(mat.roughnessMetallicIndex)], inUV).b * mat.metallicFactor;

    vec3 T = normalize(inTangent.xyz);
    vec3 N = normalize(inNormal);
    vec3 B = cross(N, T) * inTangent.w;
    mat3 TBN = mat3(T, B, N);

    vec3 tNorm = texture(textures[nonuniformEXT(mat.normalIndex)], inUV).rgb * 2.0 - 1.0;
    tNorm.xy *= mat.normalScale;
    vec3 worldNormal = normalize(TBN * tNorm);

    // --- LIGHTING MATH ---

    // 1. Fake Camera View Vector (Looking slightly down)
    vec3 V = normalize(vec3(0.0, 1.0, 1.0));

    // 2. Fake Directional Light
    vec3 L = normalize(vec3(1.0, 1.0, 1.0)); // Light direction
    vec3 H = normalize(V + L);
    vec3 radiance = vec3(2.0); // Bright white light

    float NdotL = max(dot(worldNormal, L), 0.0);
    float NdotV = max(dot(worldNormal, V), 0.0);

    vec3 F0 = mix(Fdielectric, albedo.rgb, metalness);

    // Cook-Torrance BRDF
    float NDF = ndfGGX(max(dot(worldNormal, H), 0.0), roughness);
    float G   = gaSchlickGGX(NdotL, NdotV, roughness);
    vec3 F    = fresnelSchlick(F0, max(dot(H, V), 0.0));

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + Epsilon;
    vec3 specular     = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metalness;

    // Specular + Diffuse
    vec3 Lo = (kD * albedo.rgb / PI + specular) * radiance * NdotL;

    // Basic ambient light so shadows aren't pitch black
    vec3 ambient = vec3(0.03) * albedo.rgb;

    vec3 finalColor = ambient + Lo;

    // Gamma correction
    //finalColor = pow(finalColor, vec3(1.0/2.2));

    color = vec4(finalColor, albedo.a);

    //vec3 oklab = linear_rgb_to_oklab(finalColor);

    //oklab.y *= 1.2;

    //vec3 linearRGB = oklab_to_linear_rgb(oklab);

    //vec3 srgbCorrected = pow(max(linearRGB, vec3(0.0)), vec3(1.0 / 2.2));

    //color = vec4(srgbCorrected, albedo.a);
}
