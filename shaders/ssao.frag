#version 460

layout (binding = 0) uniform sampler2D samplerDepth;

struct CompactMaterial {
    uint packedNormal;
    uint packedRadiance;
    uint packedAO;
    uint padding;
};

layout(std430, binding = 1) readonly buffer NormalBuffer {
    uint packedNormals[];
};

layout (binding = 2) uniform sampler2D ssaoNoise;

layout (constant_id = 0) const int SSAO_KERNEL_SIZE = 64;
layout (constant_id = 1) const float SSAO_RADIUS = 0.5;

layout (binding = 3) uniform UBO
{
    mat4 projection;
    mat4 invProjection;
    mat4 view;
    vec4 samples[64];
    float nearPlane;
    float farPlane;
} ubo;

layout (location = 0) in vec2 inUV;
layout (location = 0) out float outFragColor;

vec3 reconstructViewPos(vec2 uv, float depth)
{
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth, 1.0);

    vec4 viewPos = ubo.invProjection * ndc;
    return viewPos.xyz / viewPos.w;
}

vec3 decodeOctNormal(uint packedNormal) {
    vec2 f = vec2(packedNormal & 0xFFu, (packedNormal >> 8u) & 0xFFu) / 255.0 * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-n.z, 0.0);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

void main()
{
    float depth = texture(samplerDepth, inUV).r;
    vec3 fragPos = reconstructViewPos(inUV, depth);

    ivec2 texDim = textureSize(samplerDepth, 0);
    ivec2 pixelCoords = ivec2(inUV * vec2(texDim));
    uint pixelIndex = pixelCoords.y * texDim.x + pixelCoords.x;

    uint packedNormal = packedNormals[pixelIndex];

    vec3 worldNormal = decodeOctNormal(packedNormal);

    vec3 normal = mat3(ubo.view) * worldNormal;
    normal = normalize(normal);

    ivec2 noiseDim = textureSize(ssaoNoise, 0);
    vec2 targetDim = vec2(texDim.x / 2.0, texDim.y / 2.0);
    vec2 noiseScale = targetDim / vec2(noiseDim);
    vec3 randomVec = texture(ssaoNoise, inUV * noiseScale).xyz;

    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0f;
    float validSamples = 0.0f;
    const float bias = 0.025f;

    for(int i = 0; i < SSAO_KERNEL_SIZE; i++)
    {
        vec3 samplePos = TBN * ubo.samples[i].xyz;
        samplePos = fragPos + samplePos * SSAO_RADIUS;

        vec4 offset;
        offset.x = samplePos.x * ubo.projection[0][0];
        offset.y = samplePos.y * ubo.projection[1][1];
        offset.z = samplePos.z * ubo.projection[2][2] + ubo.projection[3][2];
        offset.w = samplePos.z * ubo.projection[2][3];

        offset.xyz /= offset.w;
        offset.xy = offset.xy * 0.5f + 0.5f;

        float rawSampleDepth = texture(samplerDepth, offset.xy).r;
        vec3 sampledViewPos = reconstructViewPos(offset.xy, rawSampleDepth);
        float sampleDepthZ = sampledViewPos.z;

        // Smoothly fade out samples that are too far away (e.g. background pixels)
        float rangeCheck = smoothstep(0.0, 1.0, 1.0 - (abs(fragPos.z - sampleDepthZ) / SSAO_RADIUS));

        occlusion += (sampleDepthZ >= samplePos.z + bias ? 1.0f : 0.0f) * rangeCheck;
        validSamples += rangeCheck;
    }

    float occlusionRatio = validSamples > 0.1f ? (occlusion / validSamples) : 0.0f;
    outFragColor = 1.0f - occlusionRatio;
}