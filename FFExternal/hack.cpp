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

        // Camera: MatchGame -> m_CameraControllerManager (+0xD8) -> GameCamera (+0x20)
        uintptr_t camMgr = ReadMemory<uintptr_t>(g_StaticMatchGame + Offsets::MatchGame_CameraModeMgr);
        if (camMgr) {
            g_GameCamera = ReadMemory<uintptr_t>(camMgr + Offsets::CameraControllerMgr_GameCamera);
        }

        // PlayerList: MatchGame -> m_Match (+0x90) -> PlayerList (+0x158)
        uintptr_t matchObj = ReadMemory<uintptr_t>(g_StaticMatchGame + Offsets::MatchGame_M_Match);
        if (matchObj) {
            uintptr_t playerList = ReadMemory<uintptr_t>(matchObj + Offsets::JM_PlayerList);
            
            if (playerList) {
                uintptr_t playerArray = ReadMemory<uintptr_t>(playerList + 0x10);
                int playerCount = ReadMemory<int>(playerList + 0x18);
                
                std::lock_guard<std::mutex> lk(g_PlayerMutex);
                g_Players.clear();
                g_LocalPlayer = 0; // Reseta
                
                if (playerArray && playerCount > 0 && playerCount < 100) {
                    for (int i = 0; i < playerCount; i++) {
                        uintptr_t player = ReadMemory<uintptr_t>(playerArray + 0x20 + (i * 0x8));
                        if (player) {
                            g_Players.push_back(player);
                            
                            // Acha o LocalPlayer: quem tem a Camera Transform setada
                            uintptr_t camTrans = ReadMemory<uintptr_t>(player + Offsets::Player_MainCameraTransform);
                            if (camTrans != 0) {
                                g_LocalPlayer = player;
                            }
                        }
                    }
                    g_DebugInfo = "Players: " + std::to_string(g_Players.size()) + " | Local: " + std::to_string(g_LocalPlayer != 0);
                } else {
                    g_DebugInfo = "Lista Vazia | Count: " + std::to_string(playerCount);
                }
            } else {
                g_DebugInfo = "PlayerList NULL (m_Match ok)";
            }
        } else {
            g_DebugInfo = "m_Match NULL (Fora de partida?)";
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
