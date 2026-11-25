#include "hack.hpp"
#include <thread>
#include <chrono>
#include <string>

void HackLoop() {
    Log("=== HackLoop Iniciado ===");
    g_DebugInfo = "Status: Iniciando Threads...";
    
    while (g_Running) {
        if (g_TaskPort == MACH_PORT_NULL) {
            // So busca o FreeFire. UnityFramework roda dentro dele.
            g_TaskPort = get_task_port("FreeFire");
            
            if (g_TaskPort == MACH_PORT_NULL) {
                g_DebugInfo = "Status: TaskPort NULL (Falta entitlement)";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
        }
        
        if (g_UnityBase == 0) {
            g_UnityBase = GetUnityBase();
            if (g_UnityBase == 0) {
                g_DebugInfo = "Status: UnityBase nao achado";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
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
                
                if (playerArray && playerCount > 0 && playerCount < 100) {
                    for (int i = 0; i < playerCount; i++) {
                        uintptr_t player = ReadMemory<uintptr_t>(playerArray + 0x20 + (i * 0x8));
                        if (player) g_Players.push_back(player);
                    }
                }
                
                g_DebugInfo = "OK | Players: " + std::to_string(g_Players.size());
            } else {
                g_DebugInfo = "Match OK | List NULL";
            }
        } else {
            g_DebugInfo = "Match NULL (Aguardando...)";
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
