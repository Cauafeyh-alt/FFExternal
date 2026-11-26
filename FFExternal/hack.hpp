#pragma once
#include <cstdint>
#include <cmath>
#include <vector>
#include <mutex>
#include <atomic>
#include <string>
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <mach-o/loader.h>
#include "offset.hpp"

void Log(const std::string& msg);

struct Vector2 { float x, y; };
struct Vector3 {
    float x, y, z;
    Vector3 operator+(const Vector3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vector3 operator-(const Vector3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vector3 operator*(float s) const { return {x*s, y*s, z*s}; }
    float Length() const { return std::sqrt(x*x + y*y + z*z); }
};
struct Vector4 { float x, y, z, w; };
struct Quaternion { float x, y, z, w; };
struct Matrix4x4 { float m[4][4]; };

struct Config {
    bool aimbotEnabled = false;
    bool aimHead = true;
    bool aimNeck = false;
    bool aimChest = false;
    float aimFOV = 120.0f;
    float aimRange = 500.0f;
    bool drawFOV = false;
    bool espEnabled = false;
    bool espBox = true;
    bool espHealth = true;
};

// ---- Config: mutex-protected. All reads via GetConfig(); writes via UpdateConfig.
inline Config        g_Config;
inline std::mutex    g_ConfigMtx;
inline Config GetConfig() {
    std::lock_guard<std::mutex> lk(g_ConfigMtx);
    return g_Config;
}
template <typename F>
inline void UpdateConfig(F&& f) {
    std::lock_guard<std::mutex> lk(g_ConfigMtx);
    f(g_Config);
}

// ---- Shared game state: atomics. Named accessors, release/acquire.
inline std::atomic<uintptr_t>      g_LocalPlayerAtomic{0};
inline std::atomic<uintptr_t>      g_UnityBaseAtomic{0};
inline std::atomic<uintptr_t>      g_GameCameraAtomic{0};
inline std::atomic<uintptr_t>      g_StaticMatchGameAtomic{0};
inline std::atomic<uintptr_t>      g_GameFacadeStaticAtomic{0};
inline std::atomic<mach_port_t>    g_TaskPortAtomic{MACH_PORT_NULL};

inline uintptr_t GetLocalPlayer()   { return g_LocalPlayerAtomic.load(std::memory_order_acquire); }
inline void      SetLocalPlayer(uintptr_t v){ g_LocalPlayerAtomic.store(v, std::memory_order_release); }
inline uintptr_t GetUnityBase()     { return g_UnityBaseAtomic.load(std::memory_order_acquire); }
inline void      SetUnityBase(uintptr_t v)  { g_UnityBaseAtomic.store(v, std::memory_order_release); }
inline uintptr_t GetGameCamera()    { return g_GameCameraAtomic.load(std::memory_order_acquire); }
inline void      SetGameCamera(uintptr_t v) { g_GameCameraAtomic.store(v, std::memory_order_release); }
inline uintptr_t GetStaticMatchGame(){ return g_StaticMatchGameAtomic.load(std::memory_order_acquire); }
inline void      SetStaticMatchGame(uintptr_t v){ g_StaticMatchGameAtomic.store(v, std::memory_order_release); }
inline uintptr_t GetGameFacadeStatic(){ return g_GameFacadeStaticAtomic.load(std::memory_order_acquire); }
inline void      SetGameFacadeStatic(uintptr_t v){ g_GameFacadeStaticAtomic.store(v, std::memory_order_release); }
inline mach_port_t GetTaskPort()    { return g_TaskPortAtomic.load(std::memory_order_acquire); }
inline void      SetTaskPort(mach_port_t p){ g_TaskPortAtomic.store(p, std::memory_order_release); }

inline std::vector<uintptr_t> g_Players;
inline std::mutex             g_PlayerMutex;
inline std::atomic<bool>      g_Running{true};
inline std::mutex             g_DebugMtx;
inline std::string            g_DebugInfo = "Aguardando...";

inline int g_ScreenWidth  = 1920;
inline int g_ScreenHeight = 1080;

struct EspEntity { Vector2 headPos; Vector2 footPos; float health; };
inline std::vector<EspEntity> g_EspEntities;
inline std::mutex             g_EspMutex;

// ---- ReadMemory: verify vm_read wrote the full sizeof(T). Silent on miss.
template <typename T>
T ReadMemory(uintptr_t address) {
    T data = T();
    if (address == 0) return T();
    mach_port_t tp = g_TaskPortAtomic.load(std::memory_order_acquire);
    if (tp == MACH_PORT_NULL) return T();
    vm_size_t size = sizeof(T);
    if (vm_read_overwrite(tp, (vm_address_t)address, size,
                          (vm_address_t)&data, &size) != KERN_SUCCESS) return T();
    if (size != sizeof(T)) return T();   // partial read = miss
    return data;
}

template <typename T>
void WriteMemory(uintptr_t address, T data) {
    if (address == 0) return;
    mach_port_t tp = g_TaskPortAtomic.load(std::memory_order_acquire);
    if (tp == MACH_PORT_NULL) return;
    vm_size_t size = sizeof(T);
    vm_write(tp, (vm_address_t)address, (vm_address_t)&data, size);
}

inline Vector3 GetEntityPosition(uintptr_t player) {
    uintptr_t trans = ReadMemory<uintptr_t>(player + Offsets::Entity_CachedTransform);
    if (!trans) return {0, 0, 0};
    return ReadMemory<Vector3>(trans + 0x90);
}

// ---- Module / finder interfaces (defined in Memory.cpp)
mach_port_t get_task_port(const char* process_name);
uintptr_t   FindUnityBase();
uintptr_t   FindStaticMatchGame();

// ---- Helpers (esp.cpp, aimbot.cpp)
Matrix4x4 GetViewMatrix();
bool      WorldToScreen(Vector3 pos, Vector2& screen, Matrix4x4 matrix, int width, int height);

// ---- Threads
void HackLoop();
void AimbotThread();
void EspThread();
