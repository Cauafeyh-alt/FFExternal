#pragma once
#include <cstdint>

namespace Offsets {
    // Player
    constexpr uintptr_t Player_TeamModeID         = 0x434;
    constexpr uintptr_t Player_IsClientBot        = 0x4A0;
    constexpr uintptr_t Player_CurrentAimRotation = 0x1A8C;
    constexpr uintptr_t Player_HP_Candidate       = 0xF40;
    constexpr uintptr_t Player_IsDead_RVA         = 0x561C768;

    // JMAGGLCNGIG (Match)
    constexpr uintptr_t JM_PlayerList             = 0x158;

    // Camera
    constexpr uintptr_t CameraControllerMgr_GameCamera = 0x20;

    // RVAs (GameFacade)
    constexpr uintptr_t GF_CurrentMatch           = 0x591B898;
    constexpr uintptr_t GF_CurrentLocalPlayer     = 0x591BD2C;
    constexpr uintptr_t GF_CurrentCameraMgr       = 0x5927F58;

    // RVAs (Funcoes Nativas)
    constexpr uintptr_t Entity_GetPosition_RVA    = 0x6B49D04;
    constexpr uintptr_t Camera_WorldToScreen_RVA  = 0x8CFE5C0;
    
    // Entity (O que estava faltando)
    constexpr uintptr_t Entity_CachedTransform    = 0x58;
}

inline uintptr_t g_UnityBase = 0;
