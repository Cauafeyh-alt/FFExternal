#include "hack.hpp"
#include <thread>
#include <chrono>

Matrix4x4 GetViewMatrix() {
    Matrix4x4 mat = {};
    if (g_GameCamera == 0) return mat;
    return ReadMemory<Matrix4x4>(g_GameCamera + Offsets::Camera_ViewMatrix);
}

bool WorldToScreen(Vector3 pos, Vector2& screen, Matrix4x4 matrix, int width, int height) {
    Vector4 clip;
    clip.x = pos.x * matrix.m[0][0] + pos.y * matrix.m[1][0] + pos.z * matrix.m[2][0] + matrix.m[3][0];
    clip.y = pos.x * matrix.m[0][1] + pos.y * matrix.m[1][1] + pos.z * matrix.m[2][1] + matrix.m[3][1];
    clip.z = pos.x * matrix.m[0][2] + pos.y * matrix.m[1][2] + pos.z * matrix.m[2][2] + matrix.m[3][2];
    clip.w = pos.x * matrix.m[0][3] + pos.y * matrix.m[1][3] + pos.z * matrix.m[2][3] + matrix.m[3][3];
    if (clip.w < 0.1f) return false;
    screen.x = (width / 2.0f) * (clip.x / clip.w + 1.0f);
    screen.y = (height / 2.0f) * (1.0f - (clip.y / clip.w)); 
    return true;
}

void EspThread() {
    while (g_Running) {
        if (!g_Config.espEnabled) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue;
        }
        
        uintptr_t localPlayer;
        std::vector<uintptr_t> localPlayers;
        {
            std::lock_guard<std::mutex> lk(g_PlayerMutex);
            localPlayer = g_LocalPlayer;
            localPlayers = g_Players; // Copia a lista
        }

        if (g_GameCamera == 0 || localPlayer == 0 || localPlayers.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue;
        }
        
        Matrix4x4 viewMatrix = GetViewMatrix();
        if (viewMatrix.m[0][0] == 0) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }

        std::vector<EspEntity> tempEntities;

        for (uintptr_t p : localPlayers) {
            if (p == localPlayer) continue;
            if (ReadMemory<bool>(p + Offsets::Player_IsClientBot)) continue;
            
            uintptr_t myTeam = ReadMemory<uintptr_t>(localPlayer + Offsets::Player_TeamModeID);
            uintptr_t enemyTeam = ReadMemory<uintptr_t>(p + Offsets::Player_TeamModeID);
            if (myTeam != 0 && myTeam == enemyTeam) continue;

            Vector3 pos = GetEntityPosition(p);
            if (pos.x == 0 && pos.y == 0 && pos.z == 0) continue;
            
            Vector3 head = pos; head.y += 1.65f;
            Vector2 hScreen, fScreen;
            
            if (WorldToScreen(head, hScreen, viewMatrix, g_ScreenWidth, g_ScreenHeight) && 
                WorldToScreen(pos, fScreen, viewMatrix, g_ScreenWidth, g_ScreenHeight)) {
                EspEntity e;
                e.headPos = hScreen; e.footPos = fScreen;
                e.health = ReadMemory<int>(p + Offsets::Player_HP_Candidate);
                tempEntities.push_back(e);
            }
        }
        
        std::lock_guard<std::mutex> espLk(g_EspMutex);
        g_EspEntities = std::move(tempEntities);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}
