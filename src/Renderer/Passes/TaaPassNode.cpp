//
// Created by Martin Varga on 11.08.2026.
//

#include "TaaPassNode.h"

namespace Engine {
    TaaPassNode::TaaPassNode(Device &device, Renderer &renderer, Model &megaBuffer, ResourceHeap &resourceHeap) :
    RenderPassNode("TAA Pass"),
    device(device),
    renderer(renderer),
    megabBuffer(megaBuffer),
    resourceHeap(resourceHeap)
    {

    }

    TaaPassNode::~TaaPassNode()
    {}
} // namespace Engine
