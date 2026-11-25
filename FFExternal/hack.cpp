#include "hack.hpp"
#include <thread>
#include <chrono>
#include <string>

void HackLoop() {
    Log("=== HackLoop Iniciado ===");
    g_DebugInfo = "Status: Iniciando Threads...";
    
    while (g_Running) {
        if (g_TaskPort == MACH_PORT_NULL) {
            g_TaskPort = get_task_port("FreeFire");
            if (g_TaskPort == MACH_PORT_NULL) {
                g_DebugInfo = "Status: TaskPort NULL";
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

        if (g_StaticMatchGame == 0) {
            g_StaticMatchGame = FindStaticMatchGame();
            if (g_StaticMatchGame == 0) {
                g_DebugInfo = "Status: MatchGame nao achado";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
        }

        // Puxa a Camera
        using GetCameraMgrFn = uintptr_t(*)();
        // Como nao podemos chamar RVAs, a Camera tambem precisa ser lida por ponteiro.
        // Mas por enquanto, vamos usar o MatchGame pra achar a lista de players.

        uintptr_t playerList = ReadMemory<uintptr_t>(g_StaticMatchGame + Offsets::JM_PlayerList);
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
                g_DebugInfo = "OK | Players: " + std::to_string(g_Players.size());
            } else {
                g_DebugInfo = "Lista Vazia | Count: " + std::to_string(playerCount);
            }
        } else {
            g_DebugInfo = "Match OK | List NULL";
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
