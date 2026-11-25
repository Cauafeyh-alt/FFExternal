#include "hack.hpp"
#include <thread>
#include <chrono>

template <typename T> T ReadMemory(uintptr_t address);

void HackLoop() {
    while (g_Running) {
        if (g_TaskPort == MACH_PORT_NULL || g_UnityBase == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        using GetLocalPlayerFn = uintptr_t(*)();
        auto getLocalPlayer = reinterpret_cast<GetLocalPlayerFn>(g_UnityBase + Offsets::GF_CurrentLocalPlayer);
        g_LocalPlayer = getLocalPlayer();

        using GetCameraMgrFn = uintptr_t(*)();
        auto getCameraMgr = reinterpret_cast<GetCameraMgrFn>(g_UnityBase + Offsets::GF_CurrentCameraMgr);
        uintptr_t cameraMgr = getCameraMgr();
        if (cameraMgr) {
            g_GameCamera = ReadMemory<uintptr_t>(cameraMgr + Offsets::CameraControllerMgr_GameCamera);
        }

        using GetMatchFn = uintptr_t(*)();
        auto getMatch = reinterpret_cast<GetMatchFn>(g_UnityBase + Offsets::GF_CurrentMatch);
        uintptr_t match = getMatch();

        if (match) {
            uintptr_t playerList = ReadMemory<uintptr_t>(match + Offsets::JM_PlayerList);
            if (playerList) {
                uintptr_t playerArray = ReadMemory<uintptr_t>(playerList + 0x10);
                int playerCount = ReadMemory<int>(playerList + 0x18);
                
                std::lock_guard<std::mutex> lk(g_PlayerMutex);
                g_Players.clear();
                
                for (int i = 0; i < playerCount; i++) {
                    uintptr_t player = ReadMemory<uintptr_t>(playerArray + 0x20 + (i * 0x8));
                    if (!player) continue;
                    g_Players.push_back(player);
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
