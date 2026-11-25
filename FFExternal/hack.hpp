#pragma once
#include <cstdint>
#include <cmath>
#include <vector>
#include <mutex>
#include <mach/mach.h>
#include "offset.hpp"

struct Vector2 { float x, y; };
struct Vector3 {
    float x, y, z;
    Vector3 operator+(const Vector3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vector3 operator-(const Vector3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vector3 operator*(float s) const { return {x*s, y*s, z*s}; }
    float Length() const { return std::sqrt(x*x + y*y + z*z); }
};
struct Quaternion { float x, y, z, w; };

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
    float espMaxDist = 300.0f;
};

inline Config g_Config;
inline uintptr_t g_LocalPlayer = 0;
inline std::vector<uintptr_t> g_Players;
inline bool g_Running = true;
inline std::mutex g_PlayerMutex;

extern mach_port_t g_TaskPort;
extern uintptr_t g_UnityBase;
extern uintptr_t g_GameCamera;

struct EspEntity {
    Vector2 headPos;
    Vector2 footPos;
    float health;
    float distance;
};
inline std::vector<EspEntity> g_EspEntities;
inline std::mutex g_EspMutex;

void HackLoop();
void AimbotThread();
void EspThread();
