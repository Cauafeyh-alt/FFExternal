#pragma once
#include <cstdint>

namespace Offsets {
    // GameFacade / MatchGame (CONFIRMADO)
    constexpr uintptr_t GameFacade_CurrentMatchGame  = 0x8;
    constexpr uintptr_t MatchGame_M_Match            = 0x90;  // protected JMAGGLCNGIG m_Match
    constexpr uintptr_t MatchGame_CameraModeMgr      = 0xD8;  // protected CameraControllerManager m_CameraControllerManager

    // JMAGGLCNGIG (m_Match) (CONFIRMADO)
    constexpr uintptr_t JM_PlayerList                = 0x158; // protected List<Player> LMOIPODCHOL
    constexpr uintptr_t JM_PlayerList2               = 0x160; // protected List<Player> DPFFPGAFGDK

    // Player (CONFIRMADO NO DUMP)
    constexpr uintptr_t Player_MainCameraTransform   = 0x3E8;  // public Transform MainCameraTransform
    constexpr uintptr_t Player_TeamModeID            = 0x434;  // public uint TeamModeID
    constexpr uintptr_t Player_IsClientBot           = 0x4A0;  // public bool IsClientBot
    constexpr uintptr_t Player_CurrentAimRotation    = 0x1A8C; // public Quaternion m_CurrentAimRotation
    constexpr uintptr_t Player_HP_Candidate          = 0xF40;  // private int AHDAHPKHLJM

    // CameraControllerManager (CONFIRMADO)
    constexpr uintptr_t CameraControllerMgr_GameCamera = 0x20; // private Camera EKIPKGNKILM

    // Unity / Entity (CONFIRMADO)
    constexpr uintptr_t Entity_CachedTransform       = 0x58;  // protected Transform m_CachedTransform
    constexpr uintptr_t Camera_ViewMatrix            = 0x2E0; // Unity padrão (mantido)
}

inline uintptr_t g_UnityBase = 0;
