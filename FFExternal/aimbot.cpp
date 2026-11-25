#include "hack.hpp"
#include <chrono>
#include <thread>
#include <cmath>

// Memória
template<typename T>
static inline T Read(uintptr_t addr) { return *reinterpret_cast<T*>(addr); }

template<typename T>
static inline void Write(uintptr_t addr, T value) { *reinterpret_cast<T*>(addr) = value; }

// Loop do Aimbot
void AimbotThread() {
    while (g_Running) {
        if (!g_Config.aimbotEnabled) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        std::lock_guard<std::mutex> lk(g_PlayerMutex);
        
        if (g_LocalPlayer == 0 || g_Players.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Pega posição local (usando a RVA do Entity)
        using GetPosFn = Vector3(*)(uintptr_t);
        auto getPos = reinterpret_cast<GetPosFn>(g_UnityBase + Offsets::Entity_GetPosition_RVA);
        Vector3 localPos = getPos(g_LocalPlayer);

        uintptr_t bestTarget = 0;
        float bestDist = g_Config.aimRange;

        for (uintptr_t p : g_Players) {
            if (p == g_LocalPlayer) continue;
            
            // Checa se tá morto
            using IsDeadFn = bool(*)(uintptr_t);
            auto isDead = reinterpret_cast<IsDeadFn>(g_UnityBase + Offsets::Player_IsDead_RVA);
            if (isDead(p)) continue;

            Vector3 enemyPos = getPos(p);
            float dist = (enemyPos - localPos).Length();

            if (dist < bestDist) {
                bestDist = dist;
                bestTarget = p;
            }
        }

        if (bestTarget != 0) {
            Vector3 targetPos = getPos(bestTarget);
            
            // Ajuste de altura (Head) - aproximado por enquanto
            float alturaCabeca = 1.65f; 
            if (g_Config.aimNeck) alturaCabeca = 1.4f;
            if (g_Config.aimChest) alturaCabeca = 1.2f;
            targetPos.y += alturaCabeca;

            Vector3 dir = targetPos - localPos;
            
            // Calcula os ângulos (Euler) em graus
            float distance = std::sqrt(dir.x * dir.x + dir.z * dir.z);
            float pitch = std::atan2(dir.y, distance) * (180.0f / M_PI);
            float yaw = std::atan2(dir.x, dir.z) * (180.0f / M_PI);

            // Escreve na memória do Player
            Vector3 newRot;
            newRot.x = -pitch;
            newRot.y = yaw;
            newRot.z = 0.f;

            Write<Vector3>(g_LocalPlayer + Offsets::Player_CurrentAimRotation, newRot);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // 200 FPS
    }
}
