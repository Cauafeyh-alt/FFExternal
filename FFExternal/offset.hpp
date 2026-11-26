#pragma once
#include <cstdint>

namespace Offsets {
    // GameFacade / MatchGame
    constexpr uintptr_t GameFacade_CurrentMatchGame  = 0x8;
    constexpr uintptr_t MatchGame_M_Match            = 0x90;
    constexpr uintptr_t MatchGame_CameraModeMgr      = 0xD8;

    // JMAGGLCNGIG (m_Match)
    constexpr uintptr_t JM_PlayerList                = 0x158; // LMOIPODCHOL
    constexpr uintptr_t JM_PlayerList2               = 0x160; // DPFFPGAFGDK (fallback)

    // Player (confirmado no dump)
    constexpr uintptr_t Player_MainCameraTransform   = 0x3E8;
    constexpr uintptr_t Player_TeamModeID            = 0x434;
    constexpr uintptr_t Player_IsClientBot           = 0x4A0;
    constexpr uintptr_t Player_CurrentAimRotation    = 0x550; // Quaternion
    constexpr uintptr_t Player_HP_Candidate          = 0xF40; // int AHDAHPKHLJM

    // CameraControllerManager
    constexpr uintptr_t CameraControllerMgr_GameCamera = 0x20; // EKIPKGNKILM

    // Unity / Entity
    constexpr uintptr_t Entity_CachedTransform       = 0x58; // a confirmar
    constexpr uintptr_t Camera_ViewMatrix            = 0x2E0; // Unity padrão
}

inline uintptr_t g_UnityBase = 0;
