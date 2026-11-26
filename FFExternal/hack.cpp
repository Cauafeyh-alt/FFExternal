#include "hack.hpp"
#include <thread>
#include <chrono>
#include <string>

int g_ScreenWidth = 1920;
int g_ScreenHeight = 1080;

void HackLoop() {
    Log("=== HackLoop Iniciado ===");
    { std::lock_guard<std::mutex> lk(g_PlayerMutex); g_DebugInfo = "Status: Iniciando Threads..."; }
    
    while (g_Running) {
        if (g_TaskPort == MACH_PORT_NULL) {
            g_TaskPort = get_task_port("FreeFire");
            if (g_TaskPort == MACH_PORT_NULL) {
                { std::lock_guard<std::mutex> lk(g_PlayerMutex); g_DebugInfo = "Status: TaskPort NULL"; }
                std::this_thread::sleep_for(std::chrono::seconds(2)); continue;
            }
        }
        if (g_UnityBase == 0) {
            g_UnityBase = GetUnityBase();
            if (g_UnityBase == 0) {
                { std::lock_guard<std::mutex> lk(g_PlayerMutex); g_DebugInfo = "Status: UnityBase nao achado"; }
                std::this_thread::sleep_for(std::chrono::seconds(2)); continue;
            }
        }
        if (g_StaticMatchGame == 0) {
            g_StaticMatchGame = FindStaticMatchGame();
            if (g_StaticMatchGame == 0) {
                { std::lock_guard<std::mutex> lk(g_PlayerMutex); g_DebugInfo = "Status: MatchGame nao achado"; }
                std::this_thread::sleep_for(std::chrono::seconds(2)); continue;
            }
        }

        uintptr_t camMgr = ReadMemory<uintptr_t>(g_StaticMatchGame + Offsets::MatchGame_CameraModeMgr);
        if (camMgr) { g_GameCamera = ReadMemory<uintptr_t>(camMgr + Offsets::CameraControllerMgr_GameCamera); }

        std::vector<uintptr_t> tempPlayers;
        uintptr_t tempLocal = 0;

        uintptr_t matchObj = ReadMemory<uintptr_t>(g_StaticMatchGame + Offsets::MatchGame_M_Match);
        
        if (matchObj) {
            uintptr_t playerList = ReadMemory<uintptr_t>(matchObj + Offsets::JM_PlayerList);
            if (playerList) {
                uintptr_t playerArray = ReadMemory<uintptr_t>(playerList + 0x10); 
                int playerCount = ReadMemory<int>(playerList + 0x18);
                
                if (playerArray && playerCount > 0 && playerCount < 100) {
                    for (int i = 0; i < playerCount; i++) {
                        uintptr_t player = ReadMemory<uintptr_t>(playerArray + 0x20 + (i * 0x8));
                        if (player) {
                            tempPlayers.push_back(player);
                            uintptr_t camTrans = ReadMemory<uintptr_t>(player + Offsets::Player_MainCameraTransform);
                            if (camTrans != 0) tempLocal = player; 
                        }
                    }
                }
            }
        } else {
            uintptr_t dictPtr = ReadMemory<uintptr_t>(g_StaticMatchGame + 0xC0);
            if (dictPtr) {
                uintptr_t entriesPtr = ReadMemory<uintptr_t>(dictPtr + 0x18); 
                int count = ReadMemory<int>(dictPtr + 0x20); 
                
                if (entriesPtr && count > 0 && count < 300) {
                    for (int i = 0; i < count; i++) {
                        uintptr_t entry = entriesPtr + 0x20 + (i * 0x18); 
                        uintptr_t entity = ReadMemory<uintptr_t>(entry + 0x10); 
                        if (entity) {
                            int hp = ReadMemory<int>(entity + Offsets::Player_HP_Candidate);
                            if (hp > 0 && hp <= 1000) {
                                tempPlayers.push_back(entity);
                                uintptr_t camTrans = ReadMemory<uintptr_t>(entity + Offsets::Player_MainCameraTransform);
                                if (camTrans != 0) tempLocal = entity;
                            }
                        }
                    }
                }
            }
        }

        // ATUALIZA GLOBAL COM LOCK RÁPIDO
        {
            std::lock_guard<std::mutex> lk(g_PlayerMutex);
            g_Players = std::move(tempPlayers);
            g_LocalPlayer = tempLocal;
            
            if (!g_Players.empty()) {
                g_DebugInfo = "Players: " + std::to_string(g_Players.size()) + " | Local: " + std::to_string(g_LocalPlayer != 0);
            } else {
                g_DebugInfo = "Players: 0";
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
