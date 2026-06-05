#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include "Scene/Texture.h"
#include "Renderer/ResourceHeap.h"
#include "Scene/Model.h"
#include "Scene/GameObject.h"
#include "Core/Device.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <filesystem>
#include <optional>
#include <vector>
#include <future>
#include "Core/JobSystem.h"

namespace Engine
{
    struct ParsedImage
    {
        std::vector<unsigned char> data;
        uint32_t width = 0;
        uint32_t height = 0;
        bool isValid = false;
        bool isSRGB = false;
        bool isKTX2 = false;
        void* ktxTexPtr = nullptr;
    };

    struct ParsedPrimitive
    {
        std::vector<Model::VertexPosition> positions;
        std::vector<Model::VertexAttribute> attributes;
        std::vector<uint32_t> indices;
        uint32_t localMaterialIndex = 0; // 0 = Default, index+1 = GLTF Material Index
    };

    struct ParsedNode
    {
        TransformComponent transform;
        std::vector<size_t> childrenIndices;
        std::vector<ParsedPrimitive> primitives;
    };

    struct ParsedGLTF
    {
        std::vector<ParsedImage> images;
        std::vector<ResourceHeap::MaterialData> materials;
        std::vector<ParsedNode> nodes;
        bool success = false;
    };

    class LoaderGLTF
    {
    public:
        static std::future<ParsedGLTF> loadAsync(JobSystem& jobSystem, const std::filesystem::path& filePath);

        static std::vector<GameObject> finalize(
            ParsedGLTF& parsedData,
            Device& device,
            Model& megaBuffer,
            ResourceHeap& resourceHeap,
            std::deque<Texture2D>& outTextures);

    private:
        static fastgltf::Asset loadAsset(const std::filesystem::path& filePath);

        static void decodeImages(
            JobSystem& jobSystem,
            fastgltf::Asset& asset,
            const std::filesystem::path& assetDir,
            ParsedGLTF& outData);

        static void extractMaterials(
            fastgltf::Asset& asset,
            ParsedGLTF& outData);

        static void extractNodesAndMeshes(
            fastgltf::Asset& asset,
            ParsedGLTF& outData);
    };
}
