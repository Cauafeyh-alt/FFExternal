#include "hack.hpp"
#include <thread>
#include <chrono>
#include <cmath>

Quaternion EulerToQuaternion(float pitch, float yaw, float roll) {
    float cx = cos(pitch * 0.5f * M_PI / 180.0f), sx = sin(pitch * 0.5f * M_PI / 180.0f);
    float cy = cos(yaw * 0.5f * M_PI / 180.0f), sy = sin(yaw * 0.5f * M_PI / 180.0f);
    float cz = cos(roll * 0.5f * M_PI / 180.0f), sz = sin(roll * 0.5f * M_PI / 180.0f);
    return { sx * cy * cz - cx * sy * sz, cx * sy * cz + sx * cy * sz, cx * cy * sz - sx * sy * cz, cx * cy * cz + sx * sy * sz };
}

Quaternion Slerp(Quaternion q1, Quaternion q2, float t) {
    float dot = q1.x*q2.x + q1.y*q2.y + q1.z*q2.z + q1.w*q2.w;
    if (dot < 0.0f) { q2.x = -q2.x; q2.y = -q2.y; q2.z = -q2.z; q2.w = -q2.w; dot = -dot; }
    
    // FIX FATAL: Previne a divisao por zero se a mira ja estiver no alvo
    if (dot > 0.9995f) return q2; 
    
    float theta_0 = acosf(dot), theta = theta_0 * t;
    float sin_theta = sinf(theta), sin_theta_0 = sinf(theta_0);
    float s0 = cosf(theta) - dot * sin_theta / sin_theta_0;
    float s1 = sin_theta / sin_theta_0;
    return { (q1.x * s0) + (q2.x * s1), (q1.y * s0) + (q2.y * s1), (q1.z * s0) + (q2.z * s1), (q1.w * s0) + (q2.w * s1) };
}

void AimbotThread() {
    while (g_Running) {
        if (!g_Config.aimbotEnabled) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }
        
        uintptr_t localPlayer;
        std::vector<uintptr_t> localPlayers;
        {
            std::lock_guard<std::mutex> lk(g_PlayerMutex);
            localPlayer = g_LocalPlayer;
            localPlayers = g_Players; // Copia segura
        }
        
        if (localPlayer == 0 || localPlayers.empty()) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }
        
        Vector3 localPos = GetEntityPosition(localPlayer);
        uintptr_t bestTarget = 0;
        float bestDist = g_Config.aimRange;
        Vector3 bestTargetPos;

        Matrix4x4 vm = GetViewMatrix();
        bool hasVM = (vm.m[0][0] != 0);
        
        for (uintptr_t p : localPlayers) {
            if (p == localPlayer) continue;
            if (ReadMemory<bool>(p + Offsets::Player_IsClientBot)) continue;
            
            uintptr_t myTeam = ReadMemory<uintptr_t>(localPlayer + Offsets::Player_TeamModeID);
            uintptr_t enemyTeam = ReadMemory<uintptr_t>(p + Offsets::Player_TeamModeID);
            if (myTeam != 0 && myTeam == enemyTeam) continue; 

            Vector3 enemyPos = GetEntityPosition(p);
            if (enemyPos.x == 0 && enemyPos.y == 0 && enemyPos.z == 0) continue;
            
            float alturaCabeca = 1.65f; 
            if (g_Config.aimNeck) alturaCabeca = 1.4f;
            if (g_Config.aimChest) alturaCabeca = 1.2f;
            Vector3 headPos = enemyPos;
            headPos.y += alturaCabeca;

            if (hasVM) {
                Vector2 screenPos;
                if (WorldToScreen(headPos, screenPos, vm, g_ScreenWidth, g_ScreenHeight)) {
                    float dx = screenPos.x - (g_ScreenWidth / 2.0f);
                    float dy = screenPos.y - (g_ScreenHeight / 2.0f);
                    if (std::sqrt(dx*dx + dy*dy) > g_Config.aimFOV) continue; 
                } else continue;
            }

            float dist = (headPos - localPos).Length();
            if (dist < bestDist) { bestDist = dist; bestTarget = p; bestTargetPos = headPos; }
        }

        if (bestTarget != 0) {
            Vector3 dir = bestTargetPos - localPos;
            float distance = std::sqrt(dir.x * dir.x + dir.z * dir.z);
            float pitch = std::atan2(dir.y, distance) * (180.0f / M_PI);
            float yaw = std::atan2(dir.x, dir.z) * (180.0f / M_PI);

            Quaternion targetRot = EulerToQuaternion(-pitch, yaw, 0.0f);
            Quaternion currentRot = ReadMemory<Quaternion>(localPlayer + Offsets::Player_CurrentAimRotation);
            
            Quaternion newRot = Slerp(currentRot, targetRot, 0.15f);
            WriteMemory<Quaternion>(localPlayer + Offsets::Player_CurrentAimRotation, newRot);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
