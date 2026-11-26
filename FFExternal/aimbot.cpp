#include "hack.hpp"
#include <thread>
#include <chrono>
#include <cmath>

static Quaternion EulerToQuaternion(float pitch, float yaw, float roll) {
    float cx = cosf(pitch * 0.5f * (float)M_PI / 180.0f), sx = sinf(pitch * 0.5f * (float)M_PI / 180.0f);
    float cy = cosf(yaw   * 0.5f * (float)M_PI / 180.0f), sy = sinf(yaw   * 0.5f * (float)M_PI / 180.0f);
    float cz = cosf(roll  * 0.5f * (float)M_PI / 180.0f), sz = sinf(roll  * 0.5f * (float)M_PI / 180.0f);
    return { sx*cy*cz - cx*sy*sz, cx*sy*cz + sx*cy*sz,
             cx*cy*sz - sx*sy*cz, cx*cy*cz + sx*sy*sz };
}

// Normalize both branches. Guard zero-length and near-parallel inputs.
static Quaternion Slerp(Quaternion q1, Quaternion q2, float t) {
    float dot = q1.x*q2.x + q1.y*q2.y + q1.z*q2.z + q1.w*q2.w;
    if (dot < 0.0f) { q2.x = -q2.x; q2.y = -q2.y; q2.z = -q2.z; q2.w = -q2.w; dot = -dot; }

    Quaternion r;
    if (dot > 0.9995f) {
        r = { q1.x + t*(q2.x - q1.x),
              q1.y + t*(q2.y - q1.y),
              q1.z + t*(q2.z - q1.z),
              q1.w + t*(q2.w - q1.w) };
    } else {
        float theta_0 = acosf(dot);
        float theta   = theta_0 * t;
        float sin_t   = sinf(theta);
        float sin_t0  = sinf(theta_0);
        if (sin_t0 == 0.0f) {
            r = q1;
        } else {
            float s0 = cosf(theta) - dot * sin_t / sin_t0;
            float s1 = sin_t / sin_t0;
            r = { q1.x*s0 + q2.x*s1,
                  q1.y*s0 + q2.y*s1,
                  q1.z*s0 + q2.z*s1,
                  q1.w*s0 + q2.w*s1 };
        }
    }

    float len = std::sqrt(r.x*r.x + r.y*r.y + r.z*r.z + r.w*r.w);
    if (len > 1e-6f) { r.x /= len; r.y /= len; r.z /= len; r.w /= len; }
    else { r = {0.0f, 0.0f, 0.0f, 1.0f}; }
    return r;
}

void AimbotThread() {
    while (g_Running.load(std::memory_order_acquire)) {
        Config cfg = GetConfig();                        // one snapshot per tick
        if (!cfg.aimbotEnabled) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        uintptr_t localPlayer;
        std::vector<uintptr_t> players;
        {
            std::lock_guard<std::mutex> lk(g_PlayerMutex);
            localPlayer = GetLocalPlayer();
            players     = g_Players;
        }
        if (localPlayer == 0 || players.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        Vector3 localPos = GetEntityPosition(localPlayer);
        if (localPos.x == 0 && localPos.y == 0 && localPos.z == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        uintptr_t bestTarget = 0;
        float     bestDist   = cfg.aimRange;
        Vector3   bestTargetPos{};

        Matrix4x4 vm = GetViewMatrix();
        bool hasVM = (vm.m[0][0] != 0);

        for (uintptr_t p : players) {
            if (p == localPlayer) continue;
            if (ReadMemory<bool>(p + Offsets::Player_IsClientBot)) continue;

            uintptr_t myTeam    = ReadMemory<uintptr_t>(localPlayer + Offsets::Player_TeamModeID);
            uintptr_t enemyTeam = ReadMemory<uintptr_t>(p + Offsets::Player_TeamModeID);
            if (myTeam != 0 && myTeam == enemyTeam) continue;

            Vector3 enemyPos = GetEntityPosition(p);
            if (enemyPos.x == 0 && enemyPos.y == 0 && enemyPos.z == 0) continue;

            float alt = cfg.aimHead ? 1.65f : (cfg.aimNeck ? 1.4f : (cfg.aimChest ? 1.2f : 1.65f));
            Vector3 headPos = enemyPos;
            headPos.y += alt;

            if (hasVM) {
                Vector2 sp;
                if (!WorldToScreen(headPos, sp, vm, g_ScreenWidth, g_ScreenHeight)) continue;
                float dx = sp.x - (g_ScreenWidth  / 2.0f);
                float dy = sp.y - (g_ScreenHeight / 2.0f);
                if (std::sqrt(dx*dx + dy*dy) > cfg.aimFOV) continue;
            }

            float dist = (headPos - localPos).Length();
            if (dist < bestDist) { bestDist = dist; bestTarget = p; bestTargetPos = headPos; }
        }

        if (bestTarget != 0) {
            Vector3 dir = bestTargetPos - localPos;
            float horiz = std::sqrt(dir.x*dir.x + dir.z*dir.z);
            float pitch = std::atan2(dir.y, horiz) * (180.0f / (float)M_PI);
            float yaw   = std::atan2(dir.x, dir.z)   * (180.0f / (float)M_PI);

            Quaternion target  = EulerToQuaternion(-pitch, yaw, 0.0f);
            Quaternion current = ReadMemory<Quaternion>(localPlayer + Offsets::Player_CurrentAimRotation);

            // Guard current against the all-zeros default from a failed read.
            if (current.x == 0 && current.y == 0 && current.z == 0 && current.w == 0) {
                current = {0.0f, 0.0f, 0.0f, 1.0f};
            }

            Quaternion next = Slerp(current, target, 0.15f);
            WriteMemory<Quaternion>(localPlayer + Offsets::Player_CurrentAimRotation, next);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
