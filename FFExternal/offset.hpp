#pragma once
#include <cstdint>

namespace Offsets {
    // GameFacade — campo estático (static_fields + 0x8)
    constexpr uintptr_t GameFacade_CurrentMatchGame = 0x8;

    // MatchGame — offsets absolutos
    constexpr uintptr_t MatchGame_M_Match                 = 0x90;
    constexpr uintptr_t MatchGame_CameraModeMgr           = 0xA8;
    constexpr uintptr_t MatchGame_CameraControllerManager = 0xD8;

    // JMAGGLCNGIG — NÃO VALIDADO no dump. Mantido como fallback.
    constexpr uintptr_t JM_PlayerList  = 0x158;
    constexpr uintptr_t JM_PlayerList2 = 0x160;

    // Player — absolutos
    constexpr uintptr_t Player_MainCameraTransform = 0x3E8;
    constexpr uintptr_t Player_TeamModeID          = 0x434;
    constexpr uintptr_t Player_IsClientBot         = 0x4A0;
    constexpr uintptr_t Player_CurrentAimRotation  = 0x1AA4;

    // CameraControllerManager
    constexpr uintptr_t CameraControllerMgr_GameCamera = 0x20;

    // Entity
    constexpr uintptr_t Entity_CachedTransform = 0x58;

    // REMOVIDOS: Camera_ViewMatrix, Camera_ProjectionMatrix, Player_HP_Candidate
    // Não são campos; são getters. Ver namespace RVAs abaixo.
}

namespace RVAs {
    // Player — métodos
    constexpr uintptr_t Player_get_CurHP_RVA         = 0x56AA7C8;
    constexpr uintptr_t Player_set_CurHP_RVA         = 0x565CE20;
    constexpr uintptr_t Player_get_MaxHP_RVA         = 0x5613110;
    constexpr uintptr_t Player_get_OriginalMaxHP_RVA = 0x561B600;

    // UnityEngine.Camera — getters de matriz
    constexpr uintptr_t Camera_get_ViewMatrix_RVA       = 0x8CFE220;
    constexpr uintptr_t Camera_get_ProjectionMatrix_RVA = 0x8CFE3C8;
}
