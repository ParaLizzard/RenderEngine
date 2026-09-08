#pragma once
#include <cstdint>
#include <cstddef>
#include <limits>

namespace Engine::Constants {

    // =========================================================================
    // Core Limits & Frame Buffering
    // =========================================================================
    namespace Limits {
        inline constexpr size_t   MAX_FRAMES_IN_FLIGHT    = 3;
        inline constexpr uint32_t MAX_SCENE_OBJECTS       = 8192;
        inline constexpr uint32_t MAX_TEXTURES            = 4096;
        inline constexpr uint32_t MAX_SAMPLERS            = 64;
        inline constexpr uint32_t MAX_LIGHTS              = 1024;
        inline constexpr uint32_t MAX_SHADOW_CASCADES     = 3;
    }

    // =========================================================================
    // Sentinels / Invalid Identifiers
    // =========================================================================
    namespace Sentinels {
        inline constexpr uint32_t INVALID_INDEX           = 0xFFFFFFFFu;
        inline constexpr uint32_t INVALID_BINDLESS_SLOT   = 0xFFFFFFFFu;
        inline constexpr uint64_t INVALID_GPU_ADDRESS     = 0ull;
    }

    // =========================================================================
    // Hardware & GPU Alignment Invariants
    // =========================================================================
    namespace Hardware {
        inline constexpr size_t   PUSH_CONSTANT_MAX_SIZE  = 128;
        inline constexpr size_t   BDA_BUFFER_ALIGNMENT    = 16;
        inline constexpr size_t   CACHELINE_SIZE          = 64;
    }

    // =========================================================================
    // Meshlet Geometry Pipeline
    // =========================================================================
    namespace Meshlet {
        inline constexpr uint32_t MAX_MESHLET_VERTICES    = 64;
        inline constexpr uint32_t MAX_MESHLET_TRIANGLES   = 124;
        inline constexpr float    CONE_WEIGHT             = 0.5f;
    }

    // =========================================================================
    // Compute Shader Workgroup Sizes
    // =========================================================================
    namespace Compute {
        inline constexpr uint32_t CULL_WORKGROUP_SIZE               = 256;
        inline constexpr uint32_t HIZ_MIP_WORKGROUP_SIZE            = 16;
        inline constexpr uint32_t HIZ_MIP_GENERATION_WORKGROUP_SIZE = HIZ_MIP_WORKGROUP_SIZE;
        inline constexpr uint32_t MATERIAL_WORKGROUP_SIZE           = 8;
        inline constexpr uint32_t SSAO_WORKGROUP_SIZE               = 8;
    }

    // =========================================================================
    // Profiler Constraints
    // =========================================================================
    namespace Profiling {
        inline constexpr float    WARMUP_DELAY_SECONDS    = 30.0f;
        inline constexpr uint32_t MAX_TIMESTAMP_QUERIES   = 128;
    }

    // =========================================================================
    // Shadow & Lighting Configuration
    // =========================================================================
    namespace Lighting {
        inline constexpr bool     USE_D16_SHADOW_MAPS   = true;
        inline constexpr uint32_t ENABLE_PCSS           = 0;
        inline constexpr uint32_t PCF_SAMPLES_CASCADE_0 = 8;
        inline constexpr uint32_t PCF_SAMPLES_CASCADE_1 = 4;
        inline constexpr uint32_t PCF_SAMPLES_CASCADE_2 = 4;
    }

    // =========================================================================
    // Material & Post-Processing Configuration
    // =========================================================================
    namespace PostProcess {
        inline constexpr float    MATERIAL_MAX_ANISOTROPY = 4.0f;
        inline constexpr float    TAA_MODULATION_FACTOR   = 0.9f;
        inline constexpr bool     AGX_PUNCHY              = true;
    }

    // Flat aliases for backwards/unscoped compatibility:
    using namespace Limits;
    using namespace Sentinels;
    using namespace Hardware;
    using namespace Meshlet;
    using namespace Compute;
    using namespace Profiling;
    using namespace Lighting;
    using namespace PostProcess;

} // namespace Engine::Constants
