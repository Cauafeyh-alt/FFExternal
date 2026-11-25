#pragma once
#include <cstdint>
#include <cmath>
#include <vector>
#include <mutex>
#include <string>
#include <mach/mach.h>
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

inline Config g_Config;
inline uintptr_t g_LocalPlayer = 0;
inline std::vector<uintptr_t> g_Players;
inline bool g_Running = true;
inline std::mutex g_PlayerMutex;
inline std::string g_DebugInfo = "Aguardando...";

extern mach_port_t g_TaskPort;
extern uintptr_t g_UnityBase;
extern uintptr_t g_GameCamera;
extern uintptr_t g_StaticMatchGame;

struct EspEntity {
    Vector2 headPos;
    Vector2 footPos;
    float health;
};
inline std::vector<EspEntity> g_EspEntities;
inline std::mutex g_EspMutex;

template <typename T>
T ReadMemory(uintptr_t address) {
    T data;
    vm_size_t size = sizeof(T);
    if (vm_read_overwrite(g_TaskPort, (vm_address_t)address, size, (vm_address_t)&data, &size) != KERN_SUCCESS) {
        return T();
    }
    return data;
}

template <typename T>
void WriteMemory(uintptr_t address, T data) {
    vm_size_t size = sizeof(T);
    vm_write(g_TaskPort, (vm_address_t)address, (vm_address_t)&data, size);
}

// Fica só aqui no header
inline Vector3 GetEntityPosition(uintptr_t player) {
    uintptr_t trans = ReadMemory<uintptr_t>(player + Offsets::Entity_CachedTransform);
    if (!trans) return {0, 0, 0};
    return ReadMemory<Vector3>(trans + 0x90); 
}

mach_port_t get_task_port(const char* process_name);
uintptr_t GetUnityBase();
uintptr_t FindStaticMatchGame();

void HackLoop();
void AimbotThread();
void EspThread();
