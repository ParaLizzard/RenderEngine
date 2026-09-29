#include "TextureLoader.h"
#include "Core/Log.h"
#include "Vulkan/VulkanBuffer.h"
#include "Vulkan/VulkanDevice.h"
#include "Vulkan/VulkanMemory.h"
#include "Vulkan/VulkanSynchronization.h"
#include "Vulkan/VulkanTexture.h"
#include "Vulkan/VulkanBindlessHeap.h"

#include <ktx.h>
#include <ktxvulkan.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fstream>
#include <algorithm>
#include <cctype>

namespace Engine {

namespace {

static bool isBlockCompressedFormat(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
        return true;
    default:
        return false;
    }
}

static VkFormat adjustFormatSRGB(VkFormat format, bool srgb)
{
    if (srgb) {
        switch (format) {
        case VK_FORMAT_R8G8B8A8_UNORM:
            return VK_FORMAT_R8G8B8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM:
            return VK_FORMAT_B8G8R8A8_SRGB;
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
            return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case VK_FORMAT_BC2_UNORM_BLOCK:
            return VK_FORMAT_BC2_SRGB_BLOCK;
        case VK_FORMAT_BC3_UNORM_BLOCK:
            return VK_FORMAT_BC3_SRGB_BLOCK;
        case VK_FORMAT_BC7_UNORM_BLOCK:
            return VK_FORMAT_BC7_SRGB_BLOCK;
        case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
            return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
        case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
            return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
        case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
            return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
            return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
        default:
            return format;
        }
    } else {
        switch (format) {
        case VK_FORMAT_R8G8B8A8_SRGB:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
            return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
            return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case VK_FORMAT_BC2_SRGB_BLOCK:
            return VK_FORMAT_BC2_UNORM_BLOCK;
        case VK_FORMAT_BC3_SRGB_BLOCK:
            return VK_FORMAT_BC3_UNORM_BLOCK;
        case VK_FORMAT_BC7_SRGB_BLOCK:
            return VK_FORMAT_BC7_UNORM_BLOCK;
        case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
            return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
        case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
            return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
        case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
            return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
            return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
        default:
            return format;
        }
    }
}

static bool isKTXData(std::span<const uint8_t> data, std::string_view extension)
{
    if (data.size() >= 12 && data[0] == 0xAB && data[1] == 'K' && data[2] == 'T' && data[3] == 'X' && data[4] == ' ') {
        return true;
    }
    if (!extension.empty()) {
        std::string ext(extension);
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".ktx" || ext == ".ktx2" || ext == "ktx" || ext == "ktx2") {
            return true;
        }
    }
    return false;
}

static bool loadKTXFromMemory(std::span<const uint8_t> data, bool srgb, DecodedImageData& outImage)
{
    ktxTexture* ktxTex = nullptr;
    ktxResult result = ktxTexture_CreateFromMemory(
        data.data(),
        data.size(),
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        &ktxTex
    );

    if (result != KTX_SUCCESS || !ktxTex) {
        return false;
    }

    if (ktxTex->classId == ktxTexture2_c) {
        ktxTexture2* ktx2 = reinterpret_cast<ktxTexture2*>(ktxTex);
        if (ktxTexture2_NeedsTranscoding(ktx2)) {
            if (ktxTexture2_TranscodeBasis(ktx2, KTX_TTF_BC7_RGBA, 0) != KTX_SUCCESS) {
                ktxTexture_Destroy(ktxTex);
                return false;
            }
        }
    }

    VkFormat format = ktxTexture_GetVkFormat(ktxTex);
    if (format == VK_FORMAT_UNDEFINED) {
        format = srgb ? VK_FORMAT_BC7_SRGB_BLOCK : VK_FORMAT_BC7_UNORM_BLOCK;
    } else {
        format = adjustFormatSRGB(format, srgb);
    }

    outImage.extent.width = std::max(1u, ktxTex->baseWidth);
    outImage.extent.height = std::max(1u, ktxTex->baseHeight);
    outImage.extent.depth = std::max(1u, ktxTex->baseDepth);
    outImage.mipLevels = std::max(1u, ktxTex->numLevels);
    outImage.arrayLayers = ktxTex->isCubemap ? (ktxTex->numLayers * 6) : std::max(1u, ktxTex->numLayers);
    outImage.format = format;
    outImage.isCompressed = ktxTex->isCompressed || isBlockCompressedFormat(format);

    ktx_uint8_t* pData = ktxTexture_GetData(ktxTex);
    ktx_size_t dataSize = ktxTexture_GetDataSize(ktxTex);
    if (pData && dataSize > 0) {
        outImage.pixelData.assign(pData, pData + dataSize);
    }

    uint32_t numLayers = ktxTex->isCubemap ? 6 : outImage.arrayLayers;
    for (uint32_t level = 0; level < outImage.mipLevels; ++level) {
        for (uint32_t layer = 0; layer < numLayers; ++layer) {
            ktx_size_t offset = 0;
            KTX_error_code err = ktxTexture_GetImageOffset(ktxTex, level, ktxTex->isCubemap ? 0 : layer, ktxTex->isCubemap ? layer : 0, &offset);
            if (err == KTX_SUCCESS) {
                VkBufferImageCopy region{};
                region.bufferOffset = offset;
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel = level;
                region.imageSubresource.baseArrayLayer = layer;
                region.imageSubresource.layerCount = 1;
                region.imageExtent.width = std::max(1u, ktxTex->baseWidth >> level);
                region.imageExtent.height = std::max(1u, ktxTex->baseHeight >> level);
                region.imageExtent.depth = std::max(1u, ktxTex->baseDepth >> level);
                outImage.copyRegions.push_back(region);
            }
        }
    }

    ktxTexture_Destroy(ktxTex);
    return !outImage.pixelData.empty();
}

static bool loadSTBFromMemory(std::span<const uint8_t> data, bool srgb, DecodedImageData& outImage)
{
    if (data.empty()) {
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;

    if (stbi_is_hdr_from_memory(data.data(), static_cast<int>(data.size()))) {
        float* pixels = stbi_loadf_from_memory(data.data(), static_cast<int>(data.size()), &width, &height, &channels, 4);
        if (!pixels) {
            return false;
        }

        outImage.extent = VkExtent3D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
        outImage.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        outImage.mipLevels = 1;
        outImage.arrayLayers = 1;
        outImage.isCompressed = false;

        size_t byteSize = static_cast<size_t>(width) * height * 4 * sizeof(float);
        const uint8_t* bytePtr = reinterpret_cast<const uint8_t*>(pixels);
        outImage.pixelData.assign(bytePtr, bytePtr + byteSize);

        stbi_image_free(pixels);
        return true;
    }

    stbi_uc* pixels = stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &width, &height, &channels, 4);
    if (!pixels) {
        return false;
    }

    outImage.extent = VkExtent3D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
    outImage.format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    outImage.mipLevels = 1;
    outImage.arrayLayers = 1;
    outImage.isCompressed = false;

    size_t byteSize = static_cast<size_t>(width) * height * 4;
    outImage.pixelData.assign(pixels, pixels + byteSize);

    stbi_image_free(pixels);
    return true;
}

}

DecodedImageData TextureLoader::LoadFromFile(const std::filesystem::path& path, bool srgb)
{
    if (!std::filesystem::exists(path)) {
        LOG_WARN("TextureLoader", "File does not exist: {}", path.string());
        return {};
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_WARN("TextureLoader", "Failed to open file: {}", path.string());
        return {};
    }

    std::streamsize fileSize = file.tellg();
    if (fileSize <= 0) {
        return {};
    }

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        LOG_WARN("TextureLoader", "Failed to read file: {}", path.string());
        return {};
    }

    return LoadFromMemory(buffer, path.extension().string(), srgb);
}

DecodedImageData TextureLoader::LoadFromMemory(std::span<const uint8_t> data, std::string_view extension, bool srgb)
{
    DecodedImageData result{};
    if (data.empty()) {
        return result;
    }

    if (isKTXData(data, extension)) {
        if (loadKTXFromMemory(data, srgb, result)) {
            return result;
        }
    }

    if (loadSTBFromMemory(data, srgb, result)) {
        return result;
    }

    if (!isKTXData(data, extension)) {
        if (loadKTXFromMemory(data, srgb, result)) {
            return result;
        }
    }

    return result;
}

std::unique_ptr<VulkanTexture> TextureLoader::UploadToGPU(
    VulkanDevice& device,
    VulkanMemory& allocator,
    const DecodedImageData& data,
    bool generateMipmaps,
    std::string_view debugName)
{
    if (data.pixelData.empty() || data.extent.width == 0 || data.extent.height == 0) {
        LOG_ERROR("TextureLoader", "Cannot upload empty image data to GPU ('{}')", debugName);
        return nullptr;
    }

    bool needsMipgen = generateMipmaps && (data.mipLevels <= 1) && !data.isCompressed;
    uint32_t mipLevels = std::max(1u, data.mipLevels);
    if (needsMipgen) {
        mipLevels = VulkanTexture::CalculateMipLevels(data.extent.width, data.extent.height);
    }

    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
    if (data.arrayLayers == 6) {
        viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    } else if (data.arrayLayers > 1) {
        viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    }

    TextureUsage usage = TextureUsage::Sampled | TextureUsage::TransferDst;
    if (needsMipgen && mipLevels > 1) {
        usage = usage | TextureUsage::TransferSrc;
    }

    TextureDesc desc{
        .debugName = debugName.empty() ? "UploadedTexture" : debugName,
        .extent = data.extent,
        .format = data.format,
        .usage = usage,
        .mipLevels = mipLevels,
        .arrayLayers = std::max(1u, data.arrayLayers),
        .sampleCount = VK_SAMPLE_COUNT_1_BIT,
        .imageType = VK_IMAGE_TYPE_2D,
        .viewType = viewType
    };

    auto texture = std::make_unique<VulkanTexture>(device, allocator, desc);

    BufferDesc stgDesc{
        .debugName = debugName.empty() ? "TextureUploadStaging" : std::string(debugName) + "_Staging",
        .size = data.pixelData.size(),
        .usage = BufferUsage::STAGING,
        .memoryUsage = MemoryUsage::CPU_TO_GPU
    };
    VulkanBuffer stagingBuffer(device, allocator, stgDesc);
    stagingBuffer.UpdateData(data.pixelData.data(), data.pixelData.size(), 0);

    device.ExecuteImmediate(QueueType::Graphics, [&](VkCommandBuffer cmd) {
        VulkanSync::TransitionLayout(
            cmd,
            texture->GetHandle(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT,
            mipLevels,
            desc.arrayLayers);

        if (data.copyRegions.empty()) {
            VkBufferImageCopy region{};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = desc.arrayLayers;
            region.imageOffset = { 0, 0, 0 };
            region.imageExtent = data.extent;

            vkCmdCopyBufferToImage(
                cmd,
                stagingBuffer.GetHandle(),
                texture->GetHandle(),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &region);
        } else {
            vkCmdCopyBufferToImage(
                cmd,
                stagingBuffer.GetHandle(),
                texture->GetHandle(),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                static_cast<uint32_t>(data.copyRegions.size()),
                data.copyRegions.data());
        }

        if (needsMipgen && mipLevels > 1) {
            texture->GenerateMipmaps(cmd);
        } else {
            VulkanSync::TransitionLayout(
                cmd,
                texture->GetHandle(),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT,
                mipLevels,
                desc.arrayLayers);
        }
    });

    return texture;
}

std::unique_ptr<VulkanTexture> TextureLoader::LoadAndUpload(
    VulkanDevice& device,
    VulkanMemory& allocator,
    const std::filesystem::path& path,
    bool srgb,
    bool generateMipmaps)
{
    DecodedImageData decoded = LoadFromFile(path, srgb);
    if (decoded.pixelData.empty()) {
        return nullptr;
    }
    return UploadToGPU(device, allocator, decoded, generateMipmaps, path.filename().string());
}

LoadedTexture TextureLoader::LoadAndRegister(
    VulkanDevice& device,
    VulkanMemory& allocator,
    VulkanBindlessHeap& heap,
    const std::filesystem::path& path,
    bool srgb,
    bool generateMipmaps)
{
    auto tex = LoadAndUpload(device, allocator, path, srgb, generateMipmaps);
    if (!tex) {
        return { nullptr, {} };
    }
    BindlessTextureHandle handle = heap.RegisterTexture(tex->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return { std::move(tex), handle };
}

}