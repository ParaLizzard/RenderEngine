#version 450

layout (binding = 0) uniform sampler2D samplerSSAO;
layout (binding = 1) uniform sampler2D samplerDepth;

layout (binding = 2) uniform UBO
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

void main()
{
    float centerRawDepth = texture(samplerDepth, inUV).r;
    float centerDepth = reconstructViewPos(inUV, centerRawDepth).z;
    
    vec2 texelSize = 1.0 / vec2(textureSize(samplerSSAO, 0));
    float result = 0.0;
    float weightSum = 0.0;

    const int blurRange = 2;
    float kernel[5] = float[](0.06136, 0.24477, 0.38774, 0.24477, 0.06136);

    for (int x = -blurRange; x <= blurRange; x++)
    {
        for (int y = -blurRange; y <= blurRange; y++)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            float sampleRawDepth = texture(samplerDepth, inUV + offset).r;
            float sampleDepth = reconstructViewPos(inUV + offset, sampleRawDepth).z;

            float spatialWeight = kernel[x + blurRange] * kernel[y + blurRange];

            float depthDiff = centerDepth - sampleDepth;
            const float sigmaDepth = 0.1;
            float rangeWeight = exp(-(depthDiff * depthDiff) / (2.0 * sigmaDepth * sigmaDepth));

            float weight = spatialWeight * rangeWeight;
            
            result += texture(samplerSSAO, inUV + offset).r * weight;
            weightSum += weight;
        }
    }
    
    outFragColor = weightSum > 0.0001 ? result / weightSum : texture(samplerSSAO, inUV).r;
}