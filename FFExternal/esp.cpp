#include "hack.hpp"
#include <thread>
#include <chrono>
#include <UIKit/UIKit.h>

template <typename T> T ReadMemory(uintptr_t address);

enum MonoOrStereoscopicEye : int { Left = 0, Right = 1, Both = 2 };

void EspThread() {
    while (g_Running) {
        if (!g_Config.espEnabled || g_GameCamera == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        using W2SFn = Vector3(*)(uintptr_t, Vector3, MonoOrStereoscopicEye);
        auto w2s = reinterpret_cast<W2SFn>(g_UnityBase + Offsets::Camera_WorldToScreen_RVA);

        using GetPosFn = Vector3(*)(uintptr_t);
        auto getPos = reinterpret_cast<GetPosFn>(g_UnityBase + Offsets::Entity_GetPosition_RVA);

        using IsDeadFn = bool(*)(uintptr_t);
        auto isDead = reinterpret_cast<IsDeadFn>(g_UnityBase + Offsets::Player_IsDead_RVA);

        std::vector<EspEntity> tempEntities;

        std::lock_guard<std::mutex> lk(g_PlayerMutex);
        for (uintptr_t p : g_Players) {
            if (p == g_LocalPlayer) continue;
            if (isDead(p)) continue;

            bool isBot = ReadMemory<bool>(p + Offsets::Player_IsClientBot);
            if (isBot) continue;
            
            uintptr_t myTeam = ReadMemory<uintptr_t>(g_LocalPlayer + Offsets::Player_TeamModeID);
            uintptr_t enemyTeam = ReadMemory<uintptr_t>(p + Offsets::Player_TeamModeID);
            if (myTeam == enemyTeam) continue;

            Vector3 pos = getPos(p);
            
            Vector3 headWorld = pos; headWorld.y += 1.65f;
            Vector3 footWorld = pos;

            Vector3 headScreen = w2s(g_GameCamera, headWorld, MonoOrStereoscopicEye::Left);
            Vector3 footScreen = w2s(g_GameCamera, footWorld, MonoOrStereoscopicEye::Left);

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
