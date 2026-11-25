#pragma once
#include <cstdint>

namespace Offsets {
    // Player
    constexpr uintptr_t Player_CurrentAimRotation = 0x1A8C; // Achado no dump do Player
    constexpr uintptr_t Player_MainCameraTransform= 0x3E8;
    constexpr uintptr_t Player_IsDead_RVA         = 0x561C768; // get_IsDieing

    // Entity
    constexpr uintptr_t Entity_CachedTransform    = 0x58;
    constexpr uintptr_t Entity_GetPosition_RVA    = 0x6B49D04; // get_Position

    // Camera
    constexpr uintptr_t Camera_GetMain_RVA        = 0x8CFF0B4; // get_main
    constexpr uintptr_t Camera_WorldToScreen_RVA  = 0x8CFE5C0; // WorldToScreenPoint

    // Animator
    constexpr uintptr_t Animator_GetBoneTrans_RVA = 0x8CDDFEC; // GetBoneTransform
}
// Endereço base do UnityFramework em runtime
inline uintptr_t g_UnityBase = 0;
