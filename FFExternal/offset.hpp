#pragma once
#include <cstdint>

namespace Offsets {
    // Player
    constexpr uintptr_t Player_TeamModeID        = 0x434;    // Time do jogador
    constexpr uintptr_t Player_IsClientBot       = 0x4A0;    // Bot check
    constexpr uintptr_t Player_MainCameraTransform= 0x3E8;    // Transform da câmera
    constexpr uintptr_t Player_CurrentAimRotation= 0x1A8C;   // Rotação atual
    constexpr uintptr_t Player_IsDead_RVA        = 0x561C768; // get_IsDieing

    // MatchGame
    constexpr uintptr_t MatchGame_LevelObjManager= 0xB0;      // Gerenciador de Objetos (Players, Veículos, etc)
    constexpr uintptr_t MatchGame_CameraModeMgr  = 0xA8;      // Gerenciador de Câmera

    // Entity
    constexpr uintptr_t Entity_CachedTransform    = 0x58;
    constexpr uintptr_t Entity_GetPosition_RVA    = 0x6B49D04; // get_Position

    // Camera
    constexpr uintptr_t Camera_GetMain_RVA        = 0x8CFF0B4; // get_main
    constexpr uintptr_t Camera_WorldToScreen_RVA  = 0x8CFE5C0; // WorldToScreenPoint

    // Animator
    constexpr uintptr_t Animator_GetBoneTrans_RVA = 0x8CDDFEC; // GetBoneTransform
}

inline uintptr_t g_UnityBase = 0;
