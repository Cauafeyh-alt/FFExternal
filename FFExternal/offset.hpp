#pragma once
#include <cstdint>

namespace Offsets {
    // GameFacade / MatchGame
    constexpr uintptr_t GameFacade_CurrentMatchGame = 0x8;
    constexpr uintptr_t MatchGame_M_Match           = 0x90;
    constexpr uintptr_t MatchGame_CameraModeMgr     = 0xD8;

    // JMAGGLCNGIG (m_Match)
    constexpr uintptr_t JM_PlayerList               = 0x158;
    constexpr uintptr_t JM_PlayerList2              = 0x160;

    // Player
    constexpr uintptr_t Player_MainCameraTransform  = 0x3E8;
    constexpr uintptr_t Player_TeamModeID           = 0x434;
    constexpr uintptr_t Player_IsClientBot          = 0x4A0;
    constexpr uintptr_t Player_CurrentAimRotation   = 0x1A8C;
    constexpr uintptr_t Player_HP_Candidate         = 0xF40;

    // CameraControllerManager
    constexpr uintptr_t CameraControllerMgr_GameCamera = 0x20;

    // Unity / Entity
    constexpr uintptr_t Entity_CachedTransform      = 0x58;
    // NOTE: 0x2E0 is the stock Unity view-matrix offset.
    // NOT confirmed for the FF build — verify on-device before trusting.
    constexpr uintptr_t Camera_ViewMatrix           = 0x2E0;
}
// g_UnityBase removed — now an accessor pair in hack.hpp.
