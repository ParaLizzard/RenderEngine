#include "LoaderGLTF.h"

namespace Engine
{
    std::vector<GameObject> LoaderGLTF::loadObjectGLTF(
        Device&                        device,
        const std::filesystem::path&   filePath,
        Model&                         megaBuffer,
        ResourceHeap&                  resourceHeap,
        std::vector<Texture2D>&        outTextures)
    {
        if (!std::filesystem::exists(filePath))
        {
            std::cerr << "LoaderGLTF: File not found: " << filePath << '\n';
            throw std::runtime_error("Failed to load " + filePath.string());
        }

        fastgltf::Asset asset = loadAsset(filePath);

        // ------------------------------------------------------------------
        // 1. Upload every image once, build gltfImageIndex -> heapSlot table
        // ------------------------------------------------------------------
        std::vector<uint32_t> imageIndexToHeapSlot =
            uploadImages(device, asset, filePath.parent_path(), resourceHeap, outTextures);

        // ------------------------------------------------------------------
        // 2. Build material buffer: gltfMaterialIndex -> materialID
        // ------------------------------------------------------------------
        std::vector<uint32_t> materialIndexToID =
            buildMaterials(asset, imageIndexToHeapSlot, resourceHeap);

        // ------------------------------------------------------------------
        // 3. Load meshes, assign materialID per vertex
        // ------------------------------------------------------------------
        std::vector<GameObject> loadedNodes;
        loadedNodes.reserve(asset.nodes.size());

        // First pass: Create a GameObject for every node
        for (std::size_t i = 0; i < asset.nodes.size(); ++i)
        {
            loadedNodes.push_back(GameObject::createGameObject());
        }

        // Second pass: Extract data and link children
        for (std::size_t i = 0; i < asset.nodes.size(); ++i)
        {
            auto& gltfNode = asset.nodes[i];
            auto& obj = loadedNodes[i];

            // A. Link Children using your custom addChild method
            for (auto childIdx : gltfNode.children)
            {
                obj.addChild(loadedNodes[childIdx]);
            }

            // B. Extract Transforms
            std::visit(fastgltf::visitor{
                [&](const fastgltf::math::fmat4x4& matrix) {
                    // 1. Safely copy the raw floats into a local matrix
                    glm::mat4 bakedMatrix;
                    memcpy(&bakedMatrix, matrix.data(), sizeof(glm::mat4));

                    // 2. Decompose the matrix into your TransformComponent
                    glm::vec3 skew;
                    glm::vec4 perspective;
                    glm::decompose(
                        bakedMatrix,
                        obj.transform.scale,
                        obj.transform.rotation,
                        obj.transform.translation,
                        skew,
                        perspective
                    );

                    // glm::decompose can sometimes return a conjugated quaternion,
                    // so we conjugate it back just to be safe!
                    obj.transform.rotation = glm::conjugate(obj.transform.rotation);
                },
                [&](const fastgltf::TRS& trs) {
                    // Standard Translation, Rotation, Scale
                    obj.transform.translation = glm::vec3(trs.translation[0], trs.translation[1], trs.translation[2]);
                    obj.transform.rotation = glm::quat(trs.rotation[3], trs.rotation[0], trs.rotation[1], trs.rotation[2]);
                    obj.transform.scale = glm::vec3(trs.scale[0], trs.scale[1], trs.scale[2]);
                }
            }, gltfNode.transform);

            // C. Load Mesh (if this node has one)
            if (gltfNode.meshIndex.has_value())
            {
                auto& gltfMesh = asset.meshes[gltfNode.meshIndex.value()];
                auto subMeshHandle = loadMesh(asset, gltfMesh, megaBuffer, materialIndexToID);
                if (subMeshHandle.has_value())
                {
                    obj.subMesh = subMeshHandle.value();
                }
            }
        }

        // Move all successfully built nodes into our main output vector
        std::vector<GameObject> gameObjects;
        for (auto& node : loadedNodes)
        {
            gameObjects.push_back(std::move(node));
        }

        resourceHeap.flushPendingUpdates();

        return gameObjects;
    }

    // -------------------------------------------------------------------------
    // Asset parsing
    // -------------------------------------------------------------------------

    fastgltf::Asset LoaderGLTF::loadAsset(const std::filesystem::path& filePath)
    {
        static constexpr auto supportedExtensions =
            fastgltf::Extensions::KHR_mesh_quantization |
            fastgltf::Extensions::KHR_texture_transform |
            fastgltf::Extensions::KHR_materials_variants;

        fastgltf::Parser parser(supportedExtensions);

        constexpr auto gltfOptions =
            fastgltf::Options::DontRequireValidAssetMember |
            fastgltf::Options::AllowDouble          |
            fastgltf::Options::LoadExternalBuffers  |
            fastgltf::Options::LoadExternalImages   |
            fastgltf::Options::GenerateMeshIndices;

        auto gltfFile = fastgltf::MappedGltfFile::FromPath(filePath);
        if (!bool(gltfFile))
        {
            std::cerr << "LoaderGLTF: Failed to open: "
                      << fastgltf::getErrorMessage(gltfFile.error()) << '\n';
            throw std::runtime_error("Failed to load " + filePath.string());
        }

        auto asset = parser.loadGltf(gltfFile.get(), filePath.parent_path(), gltfOptions);
        if (asset.error() != fastgltf::Error::None)
        {
            std::cerr << "LoaderGLTF: Parse error: "
                      << fastgltf::getErrorMessage(asset.error()) << '\n';
            throw std::runtime_error("Failed to load " + filePath.string());
        }

        return std::move(asset.get());
    }

    // -------------------------------------------------------------------------
    // Image upload — one upload per unique gltf image index (deduplication)
    // Returns: vector[gltfImageIndex] = bindless heap slot index
    // -------------------------------------------------------------------------

    std::vector<uint32_t> LoaderGLTF::uploadImages(
        Device&                        device,
        fastgltf::Asset&               asset,
        const std::filesystem::path&   assetDir,
        ResourceHeap&                  resourceHeap,
        std::vector<Texture2D>&        outTextures)
    {
        std::vector<uint32_t> slots(asset.images.size(), ResourceHeap::INVALID_HANDLE);

        for (std::size_t i = 0; i < asset.images.size(); ++i)
        {
            auto& gltfImage = asset.images[i];

            // fastgltf stores image data in a variant; handle both URI and embedded
            std::visit(fastgltf::visitor
            {
                [&](fastgltf::sources::URI& uriSource)
                {
                    // External file on disk
                    std::filesystem::path imagePath = assetDir / uriSource.uri.path();

                    Texture2D tex{};
                    // Albedo/emissive → sRGB; normal/ORM → UNORM.
                    // We don't know the semantic here, so default to UNORM (SRGB
                    // conversion is responsibility of the shader or the caller can
                    // re-upload with the correct format).
                    tex.loadFromFile(
                        imagePath.string(),
                        VK_FORMAT_R8G8B8A8_UNORM,
                        &device,
                        resourceHeap);


                    slots[i] = tex.heapHandle.index;
                    outTextures.push_back(std::move(tex));
                },
                [&](fastgltf::sources::Array& arraySource)
                {
                    // Embedded / loaded-into-memory image bytes
                    Texture2D tex{};
                    tex.fromBuffer(
                        arraySource.bytes.data(),
                        static_cast<VkDeviceSize>(arraySource.bytes.size()),
                        VK_FORMAT_R8G8B8A8_UNORM,
                        0, 0,   // width/height derived inside fromBuffer via stb
                        &device,
                        resourceHeap);

                    slots[i] = tex.heapHandle.index;
                    outTextures.push_back(std::move(tex));
                },
                [&](fastgltf::sources::BufferView& viewSource)
                {
                    auto& bufferView = asset.bufferViews[viewSource.bufferViewIndex];
                    auto& buffer = asset.buffers[bufferView.bufferIndex];

                    // Now we extract the bytes from the parent buffer
                    std::visit(fastgltf::visitor
                    {
                        [&](fastgltf::sources::Array& arraySource)
                        {
                            Texture2D tex{};
                            tex.fromBuffer(
                                arraySource.bytes.data() + bufferView.byteOffset,
                                static_cast<VkDeviceSize>(bufferView.byteLength),
                                VK_FORMAT_R8G8B8A8_UNORM, 0, 0, &device, resourceHeap);
                            slots[i] = tex.heapHandle.index;
                            outTextures.push_back(std::move(tex));
                        },
                        [&](fastgltf::sources::Vector& vectorSource)
                        {
                            Texture2D tex{};
                            tex.fromBuffer(
                                vectorSource.bytes.data() + bufferView.byteOffset,
                                static_cast<VkDeviceSize>(bufferView.byteLength),
                                VK_FORMAT_R8G8B8A8_UNORM, 0, 0, &device, resourceHeap);
                            slots[i] = tex.heapHandle.index;
                            outTextures.push_back(std::move(tex));
                        },
                        [&](auto&) {
                            std::cerr << "LoaderGLTF: Unsupported buffer data source for BufferView in image " << i << ".\n";
                        }
                    }, buffer.data);
                },

                [&](auto&) {
                    std::cerr << "LoaderGLTF: Unsupported image source type for image "
                              << i << ", using fallback.\n";
                    // slots[i] stays INVALID_HANDLE → shader uses fallback white tex
                }
            }, gltfImage.data);
        }

        return slots;
    }

    // -------------------------------------------------------------------------
    // Resolve a gltf TextureInfo to a bindless slot index.
    // Falls back to `fallbackSlot` (e.g. a 1x1 white or flat-normal texture).
    // -------------------------------------------------------------------------

    static uint32_t resolveTextureSlot(
        const fastgltf::Asset&          asset,
        const std::optional<fastgltf::TextureInfo>& texInfo,
        const std::vector<uint32_t>&    imageSlots,
        uint32_t                        fallbackSlot)
    {
        if (!texInfo.has_value())
            return fallbackSlot;

        const auto& gltfTex = asset.textures[texInfo->textureIndex];
        if (!gltfTex.imageIndex.has_value())
            return fallbackSlot;

        uint32_t slot = imageSlots[gltfTex.imageIndex.value()];
        return (slot == ResourceHeap::INVALID_HANDLE) ? fallbackSlot : slot;
    }

    // Same overload for NormalTextureInfo
    static uint32_t resolveTextureSlot(
        const fastgltf::Asset&          asset,
        const std::optional<fastgltf::NormalTextureInfo>& texInfo,
        const std::vector<uint32_t>&    imageSlots,
        uint32_t                        fallbackSlot)
    {
        if (!texInfo.has_value())
            return fallbackSlot;

        const auto& gltfTex = asset.textures[texInfo->textureIndex];
        if (!gltfTex.imageIndex.has_value())
            return fallbackSlot;

        uint32_t slot = imageSlots[gltfTex.imageIndex.value()];
        return (slot == ResourceHeap::INVALID_HANDLE) ? fallbackSlot : slot;
    }

    // Same overload for OcclusionTextureInfo
    static uint32_t resolveTextureSlot(
        const fastgltf::Asset&          asset,
        const std::optional<fastgltf::OcclusionTextureInfo>& texInfo,
        const std::vector<uint32_t>&    imageSlots,
        uint32_t                        fallbackSlot)
    {
        if (!texInfo.has_value())
            return fallbackSlot;

        const auto& gltfTex = asset.textures[texInfo->textureIndex];
        if (!gltfTex.imageIndex.has_value())
            return fallbackSlot;

        uint32_t slot = imageSlots[gltfTex.imageIndex.value()];
        return (slot == ResourceHeap::INVALID_HANDLE) ? fallbackSlot : slot;
    }

    // -------------------------------------------------------------------------
    // Build material buffer entries from GLTF materials
    // Returns: vector[gltfMaterialIndex] = materialID in ResourceHeap
    // -------------------------------------------------------------------------

    std::vector<uint32_t> LoaderGLTF::buildMaterials(
        fastgltf::Asset&               asset,
        const std::vector<uint32_t>&   imageSlots,
        ResourceHeap&                  resourceHeap)
    {
        // Sentinel material at index 0 — used for primitives with no material
        // Everything white / flat-normal so the shader gets sane defaults.
        const uint32_t WHITE_SLOT       = resourceHeap.getFallbackWhiteSlot();
        const uint32_t FLAT_NORMAL_SLOT = resourceHeap.getFallbackFlatNormalSlot();

        std::vector<uint32_t> matIDs;
        matIDs.reserve(asset.materials.size() + 1);

        // Slot 0: default material (no-material primitives point here)
        {
            ResourceHeap::MaterialData defaults{};
            defaults.albedoIndex           = WHITE_SLOT;
            defaults.normalIndex           = FLAT_NORMAL_SLOT;
            defaults.roughnessMetallicIndex = WHITE_SLOT;  // R=1, G=1 → rough=1, metal=0 after masking
            defaults.emissiveIndex         = WHITE_SLOT;
            defaults.albedoFactor          = glm::vec4(1.0f);
            defaults.emissiveFactor        = glm::vec4(0.0f);
            defaults.roughnessFactor       = 1.0f;
            defaults.metallicFactor        = 0.0f;
            matIDs.push_back(resourceHeap.pushMaterial(defaults));
        }

        for (auto& mat : asset.materials)
        {
            ResourceHeap::MaterialData gpuMat{};

            // --- Albedo (baseColor) -------------------------------------------
            gpuMat.albedoIndex = resolveTextureSlot(
                asset, mat.pbrData.baseColorTexture, imageSlots, WHITE_SLOT);

            auto& bc = mat.pbrData.baseColorFactor;
            gpuMat.albedoFactor = glm::vec4(bc[0], bc[1], bc[2], bc[3]);

            // --- Normal map --------------------------------------------------
            gpuMat.normalIndex = resolveTextureSlot(
                asset, mat.normalTexture, imageSlots, FLAT_NORMAL_SLOT);

            gpuMat.normalScale = mat.normalTexture.has_value()
                                 ? mat.normalTexture->scale : 1.0f;

            // --- Roughness + Metallic (packed ORM, GLTF spec: G=rough, B=metal) -
            gpuMat.roughnessMetallicIndex = resolveTextureSlot(
                asset, mat.pbrData.metallicRoughnessTexture, imageSlots, WHITE_SLOT);

            gpuMat.roughnessFactor = mat.pbrData.roughnessFactor;
            gpuMat.metallicFactor  = mat.pbrData.metallicFactor;

            // --- Emissive -----------------------------------------------------
            gpuMat.emissiveIndex = resolveTextureSlot(
                asset, mat.emissiveTexture, imageSlots, WHITE_SLOT);

            auto& em = mat.emissiveFactor;
            gpuMat.emissiveFactor = glm::vec4(em[0], em[1], em[2], 0.0f);

            // --- Occlusion ---------------------------------------------------
            gpuMat.occlusionIndex = resolveTextureSlot(
                asset, mat.occlusionTexture, imageSlots, WHITE_SLOT);

            // --- Alpha mode flag bits ----------------------------------------
            gpuMat.flags = 0;
            if (mat.alphaMode == fastgltf::AlphaMode::Mask)  gpuMat.flags |= (1u << 0);
            if (mat.alphaMode == fastgltf::AlphaMode::Blend) gpuMat.flags |= (1u << 1);
            if (mat.doubleSided)                              gpuMat.flags |= (1u << 2);
            gpuMat.alphaCutoff = mat.alphaCutoff;

            matIDs.push_back(resourceHeap.pushMaterial(gpuMat));
        }

        return matIDs;
    }

    // -------------------------------------------------------------------------
    // Mesh loading — one SubMesh per gltf Mesh (all primitives merged)
    // -------------------------------------------------------------------------

    std::optional<Model::SubMesh> LoaderGLTF::loadMesh(
        fastgltf::Asset&               asset,
        fastgltf::Mesh&                mesh,
        Model&                         megaBuffer,
        const std::vector<uint32_t>&   materialIndexToID)
    {
        std::vector<Model::Vertex> vertices;
        std::vector<uint32_t>      indices;

        for (auto& prim : mesh.primitives)
        {
            // -----------------------------------------------------------------
            // Guard: need at least positions and indices
            // -----------------------------------------------------------------
            auto* positionIt = prim.findAttribute("POSITION");
            if (positionIt == prim.attributes.end() || !prim.indicesAccessor.has_value())
                continue;

            // -----------------------------------------------------------------
            // Resolve material ID for this primitive
            // Index 0 in materialIndexToID is always the default material.
            // -----------------------------------------------------------------
            uint32_t materialID = materialIndexToID[0];  // default
            if (prim.materialIndex.has_value())
            {
                // +1 because index 0 is our sentinel default
                materialID = materialIndexToID[prim.materialIndex.value() + 1];
            }

            // Which TEXCOORD_N to use for base color (handles KHR_texture_transform)
            std::size_t baseColorTexcoordIndex = 0;
            if (prim.materialIndex.has_value())
            {
                auto& mat = asset.materials[prim.materialIndex.value()];
                if (mat.pbrData.baseColorTexture.has_value())
                {
                    auto& bcTex = mat.pbrData.baseColorTexture.value();
                    if (bcTex.transform && bcTex.transform->texCoordIndex.has_value())
                        baseColorTexcoordIndex = bcTex.transform->texCoordIndex.value();
                    else
                        baseColorTexcoordIndex = bcTex.texCoordIndex;
                }
            }

            const uint32_t vertexBase  = static_cast<uint32_t>(vertices.size());
            const uint32_t indexBase   = static_cast<uint32_t>(indices.size());

            // -----------------------------------------------------------------
            // Positions (required)
            // -----------------------------------------------------------------
            auto& posAccessor = asset.accessors[positionIt->accessorIndex];
            if (!posAccessor.bufferViewIndex.has_value()) continue;

            vertices.resize(vertexBase + posAccessor.count);

            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                asset, posAccessor, [&](fastgltf::math::fvec3 p, std::size_t idx)
                {
                    auto& v      = vertices[vertexBase + idx];
                    v.position   = glm::vec3(p.x(), p.y(), p.z());
                    v.color      = glm::vec3(1.0f);
                    v.normal     = glm::vec3(0.0f, 1.0f, 0.0f);
                    v.tangent    = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                    v.uv         = glm::vec2(0.0f);
                    v.texId      = materialID;   // ← material ID assigned here
                });

            // -----------------------------------------------------------------
            // UVs
            // -----------------------------------------------------------------
            std::string uvAttr = "TEXCOORD_" + std::to_string(baseColorTexcoordIndex);
            if (const auto* uvIt = prim.findAttribute(uvAttr); uvIt != prim.attributes.end())
            {
                auto& uvAccessor = asset.accessors[uvIt->accessorIndex];
                if (uvAccessor.bufferViewIndex.has_value())
                {
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                        asset, uvAccessor, [&](fastgltf::math::fvec2 uv, std::size_t idx)
                        {
                            vertices[vertexBase + idx].uv = glm::vec2(uv.x(), uv.y());
                        });
                }
            }

            // -----------------------------------------------------------------
            // Normals
            // -----------------------------------------------------------------
            if (auto* normalIt = prim.findAttribute("NORMAL"); normalIt != prim.attributes.end())
            {
                auto& normalAccessor = asset.accessors[normalIt->accessorIndex];
                if (normalAccessor.bufferViewIndex.has_value())
                {
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                        asset, normalAccessor, [&](fastgltf::math::fvec3 n, std::size_t idx)
                        {
                            vertices[vertexBase + idx].normal = glm::vec3(n.x(), n.y(), n.z());
                        });
                }
            }

            // -----------------------------------------------------------------
            // Tangents (needed for normal mapping)
            // -----------------------------------------------------------------
            if (auto* tangentIt = prim.findAttribute("TANGENT"); tangentIt != prim.attributes.end())
            {
                auto& tangentAccessor = asset.accessors[tangentIt->accessorIndex];
                if (tangentAccessor.bufferViewIndex.has_value())
                {
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                        asset, tangentAccessor, [&](fastgltf::math::fvec4 t, std::size_t idx)
                        {
                            vertices[vertexBase + idx].tangent =
                                glm::vec4(t.x(), t.y(), t.z(), t.w());
                        });
                }
            }

            // -----------------------------------------------------------------
            // Vertex colors (optional, COLOR_0)
            // -----------------------------------------------------------------
            if (const auto* colorIt = prim.findAttribute("COLOR_0"); colorIt != prim.attributes.end())
            {
                auto& colorAccessor = asset.accessors[colorIt->accessorIndex];
                if (colorAccessor.bufferViewIndex.has_value())
                {
                    // GLTF allows vec3 or vec4 colors; handle both
                    if (colorAccessor.type == fastgltf::AccessorType::Vec4)
                    {
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                            asset, colorAccessor, [&](fastgltf::math::fvec4 c, std::size_t idx)
                            {
                                vertices[vertexBase + idx].color = glm::vec3(c.x(), c.y(), c.z());
                            });
                    }
                    else
                    {
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                            asset, colorAccessor, [&](fastgltf::math::fvec3 c, std::size_t idx)
                            {
                                vertices[vertexBase + idx].color = glm::vec3(c.x(), c.y(), c.z());
                            });
                    }
                }
            }

            // -----------------------------------------------------------------
            // Indices — offset by vertexBase to account for merged primitives
            // -----------------------------------------------------------------
            auto& indexAccessor = asset.accessors[prim.indicesAccessor.value()];
            if (!indexAccessor.bufferViewIndex.has_value()) continue;

            indices.resize(indexBase + indexAccessor.count);

            if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedByte ||
                indexAccessor.componentType == fastgltf::ComponentType::UnsignedShort)
            {
                std::vector<uint16_t> tmp(indexAccessor.count);
                fastgltf::copyFromAccessor<uint16_t>(asset, indexAccessor, tmp.data());
                for (std::size_t i = 0; i < tmp.size(); ++i)
                    indices[indexBase + i] = static_cast<uint32_t>(tmp[i]) + vertexBase;
            }
            else
            {
                fastgltf::copyFromAccessor<uint32_t>(
                    asset, indexAccessor, indices.data() + indexBase);
                // Apply vertexBase offset for merged primitives
                for (std::size_t i = indexBase; i < indices.size(); ++i)
                    indices[i] += vertexBase;
            }
        }

        if (vertices.empty() || indices.empty())
            return std::nullopt;

        return megaBuffer.registerMesh(vertices, indices);
    }

} // namespace Engine