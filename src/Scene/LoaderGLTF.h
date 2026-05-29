#pragma once
#include "GameObject.h"
#include "Renderer/ResourceHeap.h"
#include <filesystem>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

namespace Engine
{

    class LoaderGLTF
    {
    public:
        static std::vector<GameObject> loadObjectGLTF(Device& device, const std::filesystem::path& filePath, Model& megaBuffer, ResourceHeap& resourceHeap);


    private:
        static fastgltf::Asset loadAsset(const std::filesystem::path& filePath);
    };
}

