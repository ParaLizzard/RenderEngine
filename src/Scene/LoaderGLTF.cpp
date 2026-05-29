#include "LoaderGLTF.h"


namespace Engine
{
    std::vector<GameObject> LoaderGLTF::loadObjectGLTF(Device& device, const std::filesystem::path& filePath, Model& megaBuffer,
                                           ResourceHeap& resourceHeap)
    {
        if (!std::filesystem::exists(filePath))
        {
            std::cout << "Failed to find " << filePath << '\n';
            throw std::runtime_error("Failed to load " + filePath.string());
        }

        auto Asset = loadAsset(filePath);


        return {};
    }

    fastgltf::Asset LoaderGLTF::loadAsset(const std::filesystem::path& filePath)
    {
        static constexpr auto supportedExtensions =
            fastgltf::Extensions::KHR_mesh_quantization |
            fastgltf::Extensions::KHR_texture_transform |
            fastgltf::Extensions::KHR_materials_variants;

        fastgltf::Parser parser(supportedExtensions);

        constexpr auto gltfOptions =
            fastgltf::Options::DontRequireValidAssetMember |
            fastgltf::Options::AllowDouble |
            fastgltf::Options::LoadExternalBuffers |
            fastgltf::Options::LoadExternalImages |
            fastgltf::Options::GenerateMeshIndices;

        auto gltfFile = fastgltf::MappedGltfFile::FromPath(filePath);
        if (!bool(gltfFile))
        {
            std::cerr << "Failed to open glTF file: " << fastgltf::getErrorMessage(gltfFile.error()) << '\n';
            throw std::runtime_error("Failed to load " + filePath.string());
        }

        auto asset = parser.loadGltf(gltfFile.get(), filePath.parent_path(), gltfOptions);
        if (asset.error() != fastgltf::Error::None)
        {
            std::cerr << "Failed to load glTF: " << fastgltf::getErrorMessage(asset.error()) << '\n';
            throw std::runtime_error("Failed to load " + filePath.string());
        }

        return std::move(asset.get());
    }

    fastgltf::Mesh loadMesh()

}
