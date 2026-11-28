#include "hack.hpp"
#include <thread>
#include <chrono>
#include <cmath>

Matrix4x4 MultiplyMatrix(const Matrix4x4& a, const Matrix4x4& b) {
    Matrix4x4 r = {};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            r.m[i][j] = a.m[i][0] * b.m[0][j] +
                        a.m[i][1] * b.m[1][j] +
                        a.m[i][2] * b.m[2][j] +
                        a.m[i][3] * b.m[3][j];
        }
    }
    return r;
}

Matrix4x4 GetViewProjectionMatrix() {
    Matrix4x4 vp = {};
    uintptr_t cam  = GetGameCamera();
    uintptr_t base = GetUnityBase();
    if (cam == 0 || base == 0) return vp;

    using GetMatrix_t = void(*)(void* __this, Matrix4x4* retval);
    auto getView = (GetMatrix_t)(base + RVAs::Camera_get_ViewMatrix_RVA);
    auto getProj = (GetMatrix_t)(base + RVAs::Camera_get_ProjectionMatrix_RVA);

    Matrix4x4 view = {}, proj = {};
    getView((void*)cam, &view);
    getProj((void*)cam, &proj);

    return MultiplyMatrix(proj, view);
}

bool WorldToScreen(Vector3 pos, Vector2& screen, Matrix4x4 vp, int width, int height) {
    if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z)) return false;
    if (width <= 0 || height <= 0) return false;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (!std::isfinite(vp.m[i][j])) return false;
        }
    }

    double px = (double)pos.x, py = (double)pos.y, pz = (double)pos.z;
    double cx = px * vp.m[0][0] + py * vp.m[1][0] + pz * vp.m[2][0] + vp.m[3][0];
    double cy = px * vp.m[0][1] + py * vp.m[1][1] + pz * vp.m[2][1] + vp.m[3][1];
    double cz = px * vp.m[0][2] + py * vp.m[1][2] + pz * vp.m[2][2] + vp.m[3][2];
    double cw = px * vp.m[0][3] + py * vp.m[1][3] + pz * vp.m[2][3] + vp.m[3][3];

    if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(cz) || !std::isfinite(cw)) return false;

    if (cw < 0.001) return false;
    if (cx < -cw || cx > cw) return false;
    if (cy < -cw || cy > cw) return false;
    if (cz < 0.0 || cz > cw) return false;

    double sx = ((double)width / 2.0) * (cx / cw + 1.0);
    double sy = ((double)height / 2.0) * (1.0 - (cy / cw));

    if (!std::isfinite(sx) || !std::isfinite(sy)) return false;
    if (sx < -width || sx > width * 2 || sy < -height || sy > height * 2) return false;

    screen.x = (float)sx;
    screen.y = (float)sy;
    return true;
}

void EspThread() {
    while (g_Running.load(std::memory_order_acquire)) {
        Config cfg = GetConfig();
        if (!cfg.espEnabled) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        uintptr_t localPlayer;
        std::vector<uintptr_t> localPlayers;
        {
            std::lock_guard<std::mutex> lk(g_PlayerMutex);
            localPlayer = GetLocalPlayer();
            localPlayers = g_Players;
        }

        uintptr_t cam = GetGameCamera();
        if (cam == 0 || localPlayer == 0 || localPlayers.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        Matrix4x4 vp = GetViewProjectionMatrix();
        if (vp.m[0][0] == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        uintptr_t base = GetUnityBase();
        using GetInt_t = int32_t(*)(void* __this, const void* methodInfo);
        auto getHP = (base != 0) ? (GetInt_t)(base + RVAs::Player_get_CurHP_RVA) : nullptr;

        std::vector<EspEntity> tempEntities;

        for (uintptr_t p : localPlayers) {
            if (p == localPlayer) continue;

            uintptr_t bot_field = 0;
            bool isBot = false;
            if (!SafeAdd(p, Offsets::Player_IsClientBot, bot_field) || !ReadMemory<bool>(bot_field, isBot) || isBot) continue;

            uintptr_t myTeamField = 0;
            uint32_t myTeam = 0;
            if (!SafeAdd(localPlayer, Offsets::Player_TeamModeID, myTeamField) || !ReadMemory<uint32_t>(myTeamField, myTeam)) continue;

            uintptr_t pTeamField = 0;
            uint32_t enemyTeam = 0;
            if (!SafeAdd(p, Offsets::Player_TeamModeID, pTeamField) || !ReadMemory<uint32_t>(pTeamField, enemyTeam)) continue;

            if (myTeam != 0 && myTeam == enemyTeam) continue;

            Vector3 pos = GetEntityPosition(p);
            if (pos.x == 0 && pos.y == 0 && pos.z == 0) continue;

            Vector3 head = pos; head.y += 1.65f;
            Vector2 hScreen, fScreen;

            if (WorldToScreen(head, hScreen, vp, g_ScreenWidth, g_ScreenHeight) &&
                WorldToScreen(pos,  fScreen, vp, g_ScreenWidth, g_ScreenHeight)) {

                if (hScreen.y < fScreen.y && (fScreen.y - hScreen.y) < 1000) {
                    EspEntity e;
                    e.headPos = hScreen;
                    e.footPos = fScreen;

                    if (getHP) {
                        e.health = (float)getHP((void*)p, nullptr);
                    } else {
                        e.health = 100.0f;
                    }
                    tempEntities.push_back(e);
                }
            }
        }

        {
            std::lock_guard<std::mutex> espLk(g_EspMutex);
            g_EspEntities = std::move(tempEntities);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}
