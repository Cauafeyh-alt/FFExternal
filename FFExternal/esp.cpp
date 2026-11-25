#include "hack.hpp"
#include <thread>
#include <chrono>

Matrix4x4 GetViewMatrix() {
    Matrix4x4 mat = {};
    if (g_GameCamera == 0) return mat;
    return ReadMemory<Matrix4x4>(g_GameCamera + Offsets::Camera_ViewMatrix);
}

bool WorldToScreen(Vector3 pos, Vector2& screen, Matrix4x4 matrix, int width, int height) {
    Vector4 clipCoords;
    clipCoords.x = pos.x * matrix.m[0][0] + pos.y * matrix.m[1][0] + pos.z * matrix.m[2][0] + matrix.m[3][0];
    clipCoords.y = pos.x * matrix.m[0][1] + pos.y * matrix.m[1][1] + pos.z * matrix.m[2][1] + matrix.m[3][1];
    clipCoords.z = pos.x * matrix.m[0][2] + pos.y * matrix.m[1][2] + pos.z * matrix.m[2][2] + matrix.m[3][2];
    clipCoords.w = pos.x * matrix.m[0][3] + pos.y * matrix.m[1][3] + pos.z * matrix.m[2][3] + matrix.m[3][3];

    if (clipCoords.w < 0.1f) return false;

    Vector3 NDC;
    NDC.x = clipCoords.x / clipCoords.w;
    NDC.y = clipCoords.y / clipCoords.w;
    NDC.z = clipCoords.z / clipCoords.w;

    screen.x = (width / 2.0f) * (NDC.x + 1.0f);
    screen.y = (height / 2.0f) * (1.0f - NDC.y); 
    return true;
}

void EspThread() {
    while (g_Running) {
        if (!g_Config.espEnabled || g_GameCamera == 0 || g_LocalPlayer == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        Matrix4x4 viewMatrix = GetViewMatrix();
        if (viewMatrix.m[0][0] == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        std::vector<EspEntity> tempEntities;

        std::lock_guard<std::mutex> lk(g_PlayerMutex);
        for (uintptr_t p : g_Players) {
            if (p == g_LocalPlayer) continue;

            bool isBot = ReadMemory<bool>(p + Offsets::Player_IsClientBot);
            if (isBot) continue;
            
            uintptr_t myTeam = ReadMemory<uintptr_t>(g_LocalPlayer + Offsets::Player_TeamModeID);
            uintptr_t enemyTeam = ReadMemory<uintptr_t>(p + Offsets::Player_TeamModeID);
            if (myTeam == enemyTeam) continue;

            Vector3 pos = GetEntityPosition(p);
            if (pos.x == 0 && pos.y == 0 && pos.z == 0) continue;
            
            Vector3 headWorld = pos; headWorld.y += 1.65f;
            Vector3 footWorld = pos;

            Vector2 headScreen, footScreen;
            // Ajusta as resolucoes pro teu iPhone
            if (WorldToScreen(headWorld, headScreen, viewMatrix, 430, 932) && 
                WorldToScreen(footWorld, footScreen, viewMatrix, 430, 932)) {
                
                EspEntity e;
                e.headPos = headScreen;
                e.footPos = footScreen;
                e.health = ReadMemory<int>(p + Offsets::Player_HP_Candidate);
                tempEntities.push_back(e);
            }
        }

        std::lock_guard<std::mutex> espLk(g_EspMutex);
        g_EspEntities = std::move(tempEntities);

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}
