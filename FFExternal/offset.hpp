#pragma once
#include <cstdint>

namespace Offsets {
    constexpr uintptr_t Player_TeamModeID         = 0x434;
    constexpr uintptr_t Player_IsClientBot        = 0x4A0;
    constexpr uintptr_t Player_CurrentAimRotation = 0x1A8C;
    constexpr uintptr_t Player_HP_Candidate       = 0xF40;
    constexpr uintptr_t JM_PlayerList             = 0x158;
    constexpr uintptr_t CameraControllerMgr_GameCamera = 0x20;
    constexpr uintptr_t Entity_CachedTransform    = 0x58;
    
    // ViewMatrix da Camera no iOS (se falhar, tente 0x2C0 ou 0x2D0)
    constexpr uintptr_t Camera_ViewMatrix         = 0x2E0;
}

inline uintptr_t g_UnityBase = 0;
