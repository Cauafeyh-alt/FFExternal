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

// --- Safe Math Helpers ---
inline bool SafeAdd(uintptr_t a, uintptr_t b, uintptr_t& out) {
    if (b > UINTPTR_MAX - a) return false;
    out = a + b; return true;
}
inline bool SafeSub(uintptr_t a, uintptr_t b, uintptr_t& out) {
    if (b > a) return false;
    out = a - b; return true;
}
inline bool SafeMul(uintptr_t a, uintptr_t b, uintptr_t& out) {
    if (a != 0 && b > UINTPTR_MAX / a) return false;
    out = a * b; return true;
}
// -------------------------

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

struct FacadeCache {
    uintptr_t static_fields = 0;
    uintptr_t class_ptr = 0;
};
inline FacadeCache g_FacadeCache;
inline std::mutex  g_FacadeMtx;

inline FacadeCache GetFacadeCache() {
    std::lock_guard<std::mutex> lk(g_FacadeMtx);
    return g_FacadeCache;
}
inline void SetFacadeCache(uintptr_t sf, uintptr_t klass) {
    std::lock_guard<std::mutex> lk(g_FacadeMtx);
    g_FacadeCache.static_fields = sf;
    g_FacadeCache.class_ptr = klass;
}

inline std::atomic<uintptr_t>   g_LocalPlayerAtomic{0};
inline std::atomic<uintptr_t>   g_UnityBaseAtomic{0};
inline std::atomic<uintptr_t>   g_GameCameraAtomic{0};
inline std::atomic<uintptr_t>   g_StaticMatchGameAtomic{0};
inline std::atomic<mach_port_t> g_TaskPortAtomic{MACH_PORT_NULL};

inline uintptr_t GetLocalPlayer()    { return g_LocalPlayerAtomic.load(std::memory_order_acquire); }
inline void      SetLocalPlayer(uintptr_t v)   { g_LocalPlayerAtomic.store(v, std::memory_order_release); }
inline uintptr_t GetUnityBase()      { return g_UnityBaseAtomic.load(std::memory_order_acquire); }
inline void      SetUnityBase(uintptr_t v)     { g_UnityBaseAtomic.store(v, std::memory_order_release); }
inline uintptr_t GetGameCamera()     { return g_GameCameraAtomic.load(std::memory_order_acquire); }
inline void      SetGameCamera(uintptr_t v)    { g_GameCameraAtomic.store(v, std::memory_order_release); }
inline uintptr_t GetStaticMatchGame(){ return g_StaticMatchGameAtomic.load(std::memory_order_acquire); }
inline void      SetStaticMatchGame(uintptr_t v){ g_StaticMatchGameAtomic.store(v, std::memory_order_release); }
inline mach_port_t GetTaskPort()     { return g_TaskPortAtomic.load(std::memory_order_acquire); }
inline void      SetTaskPort(mach_port_t p)    { g_TaskPortAtomic.store(p, std::memory_order_release); }

inline std::vector<uintptr_t> g_Players;
inline std::mutex             g_PlayerMutex;
inline std::atomic<bool>      g_Running{true};
inline std::string            g_DebugInfo = "Aguardando...";

inline int g_ScreenWidth  = 1920;
inline int g_ScreenHeight = 1080;

struct EspEntity { Vector2 headPos; Vector2 footPos; float health; };
inline std::vector<EspEntity> g_EspEntities;
inline std::mutex             g_EspMutex;

template <typename T>
bool ReadMemory(uintptr_t address, T& outData) {
    outData = T();
    if (address == 0) return false;
    mach_port_t tp = GetTaskPort();
    if (tp == MACH_PORT_NULL) return false;
    vm_size_t size = sizeof(T);
    if (vm_read_overwrite(tp, (vm_address_t)address, size, (vm_address_t)&outData, &size) != KERN_SUCCESS) return false;
    return size == sizeof(T);
}

template <typename T>
T ReadMemory(uintptr_t address) {
    T data;
    ReadMemory(address, data);
    return data;
}

template <typename T>
bool WriteMemory(uintptr_t address, const T& data) {
    if (address == 0) return false;
    mach_port_t tp = GetTaskPort();
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
bool        IsAddressValid(uintptr_t addr, vm_prot_t required_prot);
void        DumpCameraOffsets();

Matrix4x4 GetViewMatrix();
Matrix4x4 GetViewProjectionMatrix();
bool      WorldToScreen(Vector3 pos, Vector2& screen, Matrix4x4 vp, int width, int height);

void HackLoop();
void AimbotThread();
void EspThread();
