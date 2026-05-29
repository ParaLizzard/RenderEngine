#pragma once
#include "GameObject.h"
#include "Renderer/ResourceHeap.h"

namespace Engine
{

    class LoaderGLTF
    {
    public:
        static std::vector<GameObject> loadObjectGLTF(Device& device, std::string& fileName, Model& megaBuffer, ResourceHeap& resourceHeap);
        
    private:

    };
}

