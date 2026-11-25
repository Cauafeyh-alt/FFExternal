#include "hack.hpp"
#include <thread>
#include <chrono>
#include <cmath>

Vector3 GetEntityPosition(uintptr_t player) {
    uintptr_t trans = ReadMemory<uintptr_t>(player + Offsets::Entity_CachedTransform);
    if (!trans) return {0, 0, 0};
    // O offset da posicao no Transform no Unity iOS eh geralmente 0x90
    return ReadMemory<Vector3>(trans + 0x90); 
}

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

        Vector3 localPos = GetEntityPosition(g_LocalPlayer);

        uintptr_t bestTarget = 0;
        float bestDist = g_Config.aimRange;
        Vector3 bestTargetPos;

        std::lock_guard<std::mutex> lk(g_PlayerMutex);
        for (uintptr_t p : g_Players) {
            if (p == g_LocalPlayer) continue;
            
            // Como nao podemos chamar o isDead RVA, lemos o HP ou uma flag direta se tivermos.
            // Vamos pular o isDead por enquanto e confiar no filtro de HP.
            
            bool isBot = ReadMemory<bool>(p + Offsets::Player_IsClientBot);
            if (isBot) continue;
            
            uintptr_t myTeam = ReadMemory<uintptr_t>(g_LocalPlayer + Offsets::Player_TeamModeID);
            uintptr_t enemyTeam = ReadMemory<uintptr_t>(p + Offsets::Player_TeamModeID);
            if (myTeam == enemyTeam) continue;

            Vector3 enemyPos = GetEntityPosition(p);
            if (enemyPos.x == 0 && enemyPos.y == 0 && enemyPos.z == 0) continue;
            
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
