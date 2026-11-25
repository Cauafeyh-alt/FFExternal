#include "hack.hpp"
#include <thread>
#include <chrono>
#include <cmath>

Quaternion EulerToQuaternion(float pitch, float yaw, float roll) {
    float cx = cos(pitch * 0.5f * M_PI / 180.0f);
    float sx = sin(pitch * 0.5f * M_PI / 180.0f);
    float cy = cos(yaw * 0.5f * M_PI / 180.0f);
    float sy = sin(yaw * 0.5f * M_PI / 180.0f);
    float cz = cos(roll * 0.5f * M_PI / 180.0f);
    float sz = sin(roll * 0.5f * M_PI / 180.0f);
    return {
        sx * cy * cz - cx * sy * sz,
        cx * sy * cz + sx * cy * sz,
        cx * cy * sz - sx * sy * cz,
        cx * cy * cz + sx * sy * sz
    };
}

void AimbotThread() {
    while (g_Running) {
        if (!g_Config.aimbotEnabled || g_LocalPlayer == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        using GetPosFn = Vector3(*)(uintptr_t);
        auto getPos = reinterpret_cast<GetPosFn>(g_UnityBase + Offsets::Entity_GetPosition_RVA);
        Vector3 localPos = getPos(g_LocalPlayer);

        using IsDeadFn = bool(*)(uintptr_t);
        auto isDead = reinterpret_cast<IsDeadFn>(g_UnityBase + Offsets::Player_IsDead_RVA);

        uintptr_t bestTarget = 0;
        float bestDist = g_Config.aimRange;
        Vector3 bestTargetPos;

        std::lock_guard<std::mutex> lk(g_PlayerMutex);
        for (uintptr_t p : g_Players) {
            if (p == g_LocalPlayer) continue;
            if (isDead(p)) continue;

            bool isBot = ReadMemory<bool>(p + Offsets::Player_IsClientBot);
            if (isBot) continue;
            
            uintptr_t myTeam = ReadMemory<uintptr_t>(g_LocalPlayer + Offsets::Player_TeamModeID);
            uintptr_t enemyTeam = ReadMemory<uintptr_t>(p + Offsets::Player_TeamModeID);
            if (myTeam == enemyTeam) continue;

            Vector3 enemyPos = getPos(p);
            
            float alturaCabeca = 1.65f; 
            if (g_Config.aimNeck) alturaCabeca = 1.4f;
            if (g_Config.aimChest) alturaCabeca = 1.2f;
            enemyPos.y += alturaCabeca;

            float dist = (enemyPos - localPos).Length();
            if (dist < bestDist) {
                bestDist = dist;
                bestTarget = p;
                bestTargetPos = enemyPos;
            }
        }

        if (bestTarget != 0) {
            Vector3 dir = bestTargetPos - localPos;
            float distance = std::sqrt(dir.x * dir.x + dir.z * dir.z);
            float pitch = std::atan2(dir.y, distance) * (180.0f / M_PI);
            float yaw = std::atan2(dir.x, dir.z) * (180.0f / M_PI);

            Quaternion newRot = EulerToQuaternion(-pitch, yaw, 0.0f);
            WriteMemory<Quaternion>(g_LocalPlayer + Offsets::Player_CurrentAimRotation, newRot);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
