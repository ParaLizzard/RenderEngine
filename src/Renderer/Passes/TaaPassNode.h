#pragma once
#include "Renderer/RenderPassNode.h"

namespace Engine {

    class TaaPassNode: public RenderPassNode
    {
    public:
        TaaPassNode(Device &device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap);
        ~TaaPassNode() override;

        TaaPassNode(const TaaPassNode &) = delete;
        TaaPassNode &operator=(const TaaPassNode &) = delete;

    private:
        Device &device;
        Renderer &renderer;
        Model &megabBuffer;
        ResourceHeap &resourceHeap;
    };

} // namespace Engine

