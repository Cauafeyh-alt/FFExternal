#include "hack.hpp"
#include <thread>
#include <chrono>
#include <string>

static bool s_teamDumpLogged = false;

void HackLoop() {
    Log("=== HackLoop Iniciado ===");
    { std::lock_guard<std::mutex> lk(g_PlayerMutex); g_DebugInfo = "Status: Iniciando Threads..."; }
    
    while (g_Running.load(std::memory_order_acquire)) {
        mach_port_t tp = GetTaskPort();
        if (tp == MACH_PORT_NULL) {
            tp = get_task_port("FreeFire");
            if (tp == MACH_PORT_NULL) {
                { std::lock_guard<std::mutex> lk(g_PlayerMutex); g_DebugInfo = "Status: TaskPort NULL"; }
                std::this_thread::sleep_for(std::chrono::seconds(2)); continue;
            }
            SetTaskPort(tp);
        }
        
        uintptr_t ub = GetUnityBase();
        if (ub == 0) {
            ub = FindUnityBase();
            if (ub == 0) {
                { std::lock_guard<std::mutex> lk(g_PlayerMutex); g_DebugInfo = "Status: UnityBase nao achado"; }
                std::this_thread::sleep_for(std::chrono::seconds(2)); continue;
            }
            SetUnityBase(ub);
        }
        
        uintptr_t matchGame = GetStaticMatchGame();
        if (matchGame == 0) {
            matchGame = FindStaticMatchGame();
            if (matchGame == 0) {
                { std::lock_guard<std::mutex> lk(g_PlayerMutex); g_DebugInfo = "Status: MatchGame nao achado"; }
                std::this_thread::sleep_for(std::chrono::seconds(2)); continue; // AUMENTADO AQUI
            }
            SetStaticMatchGame(matchGame);
        }

        uintptr_t camMgr_field = 0;
        uintptr_t camMgr = 0;
        if (SafeAdd(matchGame, Offsets::MatchGame_CameraModeMgr, camMgr_field) && ReadMemory<uintptr_t>(camMgr_field, camMgr) && camMgr) {
            uintptr_t cam_field = 0;
            uintptr_t cam = 0;
            if (SafeAdd(camMgr, Offsets::CameraControllerMgr_GameCamera, cam_field) && ReadMemory<uintptr_t>(cam_field, cam) && cam) {
                SetGameCamera(cam);
            }
        }

        std::vector<uintptr_t> tempPlayers;
        uintptr_t tempLocal = 0;

        uintptr_t matchObj_field = 0;
        uintptr_t matchObj = 0;
        
        if (SafeAdd(matchGame, Offsets::MatchGame_M_Match, matchObj_field) && ReadMemory<uintptr_t>(matchObj_field, matchObj) && matchObj) {
            uintptr_t playerList_field = 0;
            uintptr_t playerList = 0;
            if (SafeAdd(matchObj, Offsets::JM_PlayerList, playerList_field) && ReadMemory<uintptr_t>(playerList_field, playerList) && playerList) {
                uintptr_t array_field = 0, count_field = 0;
                uintptr_t playerArray = 0;
                int playerCount = 0;
                
                if (SafeAdd(playerList, 0x10, array_field) && ReadMemory<uintptr_t>(array_field, playerArray) &&
                    SafeAdd(playerList, 0x18, count_field) && ReadMemory<int>(count_field, playerCount) &&
                    playerArray && playerCount > 0 && playerCount < 100) {
                    
                    for (int i = 0; i < playerCount; i++) {
                        uintptr_t offset = 0, entry = 0, player = 0;
                        if (!SafeMul((uintptr_t)i, 0x8, offset)) continue;
                        if (!SafeAdd(playerArray, 0x20, entry)) continue;
                        if (!SafeAdd(entry, offset, entry)) continue;
                        if (!ReadMemory<uintptr_t>(entry, player) || !player) continue;
                        
                        uintptr_t pKlass = 0;
                        if (ReadMemory<uintptr_t>(player, pKlass) && pKlass > 0x100000000ULL && pKlass < 0x7FFFFFFFFFFFULL) {
                            tempPlayers.push_back(player);
                            uintptr_t camTrans_field = 0, camTrans = 0;
                            if (SafeAdd(player, Offsets::Player_MainCameraTransform, camTrans_field) && ReadMemory<uintptr_t>(camTrans_field, camTrans) && camTrans != 0) {
                                tempLocal = player;
                            }
                        }
                    }
                }
            }
        }

        if (!s_teamDumpLogged && tempLocal != 0) {
            uintptr_t teamField = 0;
            if (SafeAdd(tempLocal, Offsets::Player_TeamModeID, teamField)) {
                uint32_t team32 = 0;
                ReadMemory<uint32_t>(teamField, team32);
                char buf[256];
                snprintf(buf, sizeof(buf), "[DUMP] TeamModeID @0x%llx: 32=0x%x", (unsigned long long)teamField, (unsigned)team32);
                Log(buf);
            }
            s_teamDumpLogged = true;
        }

        {
            std::lock_guard<std::mutex> lk(g_PlayerMutex);
            g_Players = std::move(tempPlayers);
            SetLocalPlayer(tempLocal);
            
            if (!g_Players.empty()) {
                g_DebugInfo = "Players: " + std::to_string(g_Players.size()) + " | Local: " + std::to_string(GetLocalPlayer() != 0);
            } else {
                g_DebugInfo = "Players: 0";
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
