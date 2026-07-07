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
layout (constant_id = 1) const float SSAO_RADIUS = 0.3;

// Combined UBO matching the C++ SsaoUbo struct
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

// Helper function to reconstruct view-space position from hardware depth
vec3 reconstructViewPos(vec2 uv, float depth)
{
    // Convert UV and depth to Normalized Device Coordinates (NDC)
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth, 1.0);

    // Transform NDC back to View Space
    vec4 viewPos = ubo.invProjection * ndc;
    return viewPos.xyz / viewPos.w;
}

void main()
{
    // 1. Reconstruct the View-Space position
    float depth = texture(samplerDepth, inUV).r;
    vec3 fragPos = reconstructViewPos(inUV, depth);

    ivec2 texDim = textureSize(samplerDepth, 0);
    ivec2 pixelCoords = ivec2(inUV * vec2(texDim));
    uint pixelIndex = pixelCoords.y * texDim.x + pixelCoords.x;

    uint packedNormal = packedNormals[pixelIndex];

    // Decode normal (Assuming standard RGBA8 packing. Change this if you use Octahedron decoding!)
    vec4 unpacked = unpackUnorm4x8(packedNormal);
    vec3 worldNormal = normalize(unpacked.xyz * 2.0 - 1.0);

    // Transform World Space normal to View Space to match fragPos
    vec3 normal = mat3(ubo.view) * worldNormal;
    normal = normalize(normal);

    // 3. Get the random vector
    //ivec2 texDim = textureSize(samplerDepth, 0);
    ivec2 noiseDim = textureSize(ssaoNoise, 0);
    vec2 noiseScale = vec2(float(texDim.x)/float(noiseDim.x), float(texDim.y)/float(noiseDim.y));
    vec3 randomVec = texture(ssaoNoise, inUV * noiseScale).xyz;

    // 4. Build TBN Matrix (Now safely entirely in View Space!)
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    // 5. Calculate Occlusion
    float occlusion = 0.0f;
    const float bias = 0.025f;

    for(int i = 0; i < SSAO_KERNEL_SIZE; i++)
    {
        // Get sample position in view space
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

        float sampleDepthZ = ubo.projection[3][2] / (rawSampleDepth - ubo.projection[2][2]);

        float rangeCheck = smoothstep(0.0f, 1.0f, SSAO_RADIUS / abs(fragPos.z - sampleDepthZ));

        occlusion += (sampleDepthZ >= samplePos.z + bias ? 1.0f : 0.0f) * rangeCheck;
    }

    outFragColor = 1.0f - (occlusion / float(SSAO_KERNEL_SIZE));
}