#pragma once
#include "Core/CVar.h"

namespace Engine {
    AUTO_CVAR(CVarSSAOEnabled, bool, "r.SSAO.Enable", true, "Enable Screen-Space Ambient Occlusion", CVarFlags::SaveToConfig | CVarFlags::RenderDirty);
    AUTO_CVAR(CVarSSAOStrength, float, "r.SSAO.Strength", 1.25f, "SSAO occlusion multiplier", CVarFlags::SaveToConfig);
    AUTO_CVAR(CVarSSAORadius, float, "r.SSAO.Radius", 0.5f, "SSAO sampling radius", CVarFlags::SaveToConfig);
    AUTO_CVAR(CVarAAMethod, int, "r.AntiAliasing.Method", 2, "0: None, 1: FXAA, 2: TAA", CVarFlags::SaveToConfig | CVarFlags::RenderDirty);
    AUTO_CVAR(CVarTonemapMethod, int, "r.Tonemap.Method", 1, "0: ACES, 1: AgX", CVarFlags::SaveToConfig);
    AUTO_CVAR(CVarTonemapExposure, float, "r.Tonemap.Exposure", 2.0f, "Camera tonemap exposure", CVarFlags::SaveToConfig);
    AUTO_CVAR(CVarFreezeCulling, bool, "r.Debug.FreezeCulling", false, "Freeze frustum & occlusion culling", CVarFlags::None);
    AUTO_CVAR(CVarHiZDebugMode, int, "r.Debug.HiZView", 0, "0: Normal, 1: Show Hi-Z depth mip", CVarFlags::None);
    AUTO_CVAR(CVarHiZDebugMip, int, "r.Debug.HiZMipLevel", 0, "Hi-Z mip level to visualize", CVarFlags::None);
} // namespace Engine
