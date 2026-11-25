#include "hack.hpp"
#include <chrono>
#include <thread>
#include <algorithm>

struct BoxRect { float x, y, w, h; };

// Como não temos a matriz da câmera ainda, vamos desenhar um ponto
// no centro do inimigo baseado em cálculos aproximados de tela.
// (O ideal é achar o ViewMatrix no dump depois).
static bool WorldToScreenApprox(const Vector3& world, const Vector3& local, Vector2& screen) {
    // Matemática fake só pra ver o ESP funcionando
    float dx = world.x - local.x;
    float dy = world.y - local.y;
    
    // Assume tela 1080x1920 e FOV de 90
    float screenX = (dx / 10.0f) * 1080 + 540;
    float screenY = (dy / 10.0f) * 1920 + 960;
    
    screen.x = screenX;
    screen.y = screenY;
    return true;
}

static bool GetBoxRect(uintptr_t player, uintptr_t localPlayer, BoxRect& out) {
    using GetPosFn = Vector3(*)(uintptr_t);
    auto getPos = reinterpret_cast<GetPosFn>(g_UnityBase + Offsets::Entity_GetPosition_RVA);
    
    Vector3 pos = getPos(player);
    Vector3 localPos = getPos(localPlayer);
    
    Vector2 screen;
    if (!WorldToScreenApprox(pos, localPos, screen)) return false;
    
    out.w = 50.0f; 
    out.h = 100.0f;
    out.x = screen.x - (out.w / 2);
    out.y = screen.y - out.h;
    return true;
}

struct EspEntity {
    BoxRect box;
    float health;
    float maxHealth;
    float distance;
};

inline std::vector<EspEntity> g_EspEntities;
inline std::mutex g_EspMutex;

void EspThread() {
    while (g_Running) {
        if (!g_Config.espEnabled) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        std::vector<EspEntity> tmp;
        {
            std::lock_guard<std::mutex> lk(g_PlayerMutex);
            
            using GetPosFn = Vector3(*)(uintptr_t);
            auto getPos = reinterpret_cast<GetPosFn>(g_UnityBase + Offsets::Entity_GetPosition_RVA);
            Vector3 localPos = getPos(g_LocalPlayer);

            for (uintptr_t p : g_Players) {
                if (p == g_LocalPlayer) continue;
                
                EspEntity e{};
                if (!GetBoxRect(p, g_LocalPlayer, e.box)) continue;
                
                Vector3 enemyPos = getPos(p);
                e.distance = (enemyPos - localPos).Length();
                
                // Vida chumbada até achar o offset
                e.health = 100.0f; 
                e.maxHealth = 100.0f;
                tmp.push_back(e);
            }
        }
        {
            std::lock_guard<std::mutex> lk(g_EspMutex);
            g_EspEntities = std::move(tmp);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}
