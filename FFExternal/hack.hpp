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

// ReadMemory agora retorna status
template <typename T>
bool ReadMemory(uintptr_t address, T& outData) {
    outData = T();
    if (address == 0) return false;
    mach_port_t tp = g_TaskPortAtomic.load(std::memory_order_acquire);
    if (tp == MACH_PORT_NULL) return false;
    vm_size_t size = sizeof(T);
    if (vm_read_overwrite(tp, (vm_address_t)address, size, (vm_address_t)&outData, &size) != KERN_SUCCESS) return false;
    return size == sizeof(T);
}

// Wrapper antigo pra compatibilidade com o código atual
template <typename T>
T ReadMemory(uintptr_t address) {
    T data;
    ReadMemory(address, data);
    return data;
}

// WriteMemory agora retorna status
template <typename T>
bool WriteMemory(uintptr_t address, const T& data) {
    if (address == 0) return false;
    mach_port_t tp = g_TaskPortAtomic.load(std::memory_order_acquire);
    if (tp == MACH_PORT_NULL) return false;
    vm_size_t size = sizeof(T);
    return vm_write(tp, (vm_address_t)address, (vm_address_t)&data, size) == KERN_SUCCESS;
}

inline Vector3 GetEntityPosition(uintptr_t player) {
    uintptr_t trans;
    if (!ReadMemory<uintptr_t>(player + Offsets::Entity_CachedTransform, trans) || !trans) return {0, 0, 0};
    Vector3 pos;
    if (!ReadMemory<Vector3>(trans + 0x90, pos)) return {0, 0, 0};
    return pos;
}

mach_port_t get_task_port(const char* process_name);
uintptr_t   FindUnityBase();
uintptr_t   FindStaticMatchGame();

Matrix4x4 GetViewMatrix();
bool      WorldToScreen(Vector3 pos, Vector2& screen, Matrix4x4 matrix, int width, int height);

void HackLoop();
void AimbotThread();
void EspThread();
