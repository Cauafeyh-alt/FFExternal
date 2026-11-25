#include "hack.hpp"
#include <thread>
#include <chrono>

enum MonoOrStereoscopicEye : int { Left = 0, Right = 1, Both = 2 };

void EspThread() {
    while (g_Running) {
        if (!g_Config.espEnabled || g_GameCamera == 0 || g_LocalPlayer == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        using W2SFn = Vector3(*)(uintptr_t, Vector3, MonoOrStereoscopicEye);
        uintptr_t addrW2S = g_UnityBase + Offsets::Camera_WorldToScreen_RVA;

        using GetPosFn = Vector3(*)(uintptr_t);
        uintptr_t addrGetPos = g_UnityBase + Offsets::Entity_GetPosition_RVA;

        using IsDeadFn = bool(*)(uintptr_t);
        uintptr_t addrIsDead = g_UnityBase + Offsets::Player_IsDead_RVA;

        std::vector<EspEntity> tempEntities;

        std::lock_guard<std::mutex> lk(g_PlayerMutex);
        for (uintptr_t p : g_Players) {
            if (p == g_LocalPlayer) continue;
            
            bool dead = SafeCall<bool, IsDeadFn>(addrIsDead, p);
            if (dead) continue;

            bool isBot = ReadMemory<bool>(p + Offsets::Player_IsClientBot);
            if (isBot) continue;
            
            uintptr_t myTeam = ReadMemory<uintptr_t>(g_LocalPlayer + Offsets::Player_TeamModeID);
            uintptr_t enemyTeam = ReadMemory<uintptr_t>(p + Offsets::Player_TeamModeID);
            if (myTeam == enemyTeam) continue;

            Vector3 pos = SafeCall<Vector3, GetPosFn>(addrGetPos, p);
            if (pos.x == 0 && pos.y == 0 && pos.z == 0) continue;
            
            Vector3 headWorld = pos; headWorld.y += 1.65f;
            Vector3 footWorld = pos;

            Vector3 headScreen = SafeCall<Vector3, W2SFn>(addrW2S, g_GameCamera, headWorld, MonoOrStereoscopicEye::Left);
            Vector3 footScreen = SafeCall<Vector3, W2SFn>(addrW2S, g_GameCamera, footWorld, MonoOrStereoscopicEye::Left);

            if (headScreen.z > 0.0f && footScreen.z > 0.0f) {
                EspEntity e;
                e.headPos = {headScreen.x, headScreen.y};
                e.footPos = {footScreen.x, footScreen.y};
                e.health = ReadMemory<int>(p + Offsets::Player_HP_Candidate);
                tempEntities.push_back(e);
            }
        }

        std::lock_guard<std::mutex> espLk(g_EspMutex);
        g_EspEntities = std::move(tempEntities);

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}
