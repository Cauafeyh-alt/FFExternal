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

struct Config {
    // Aimbot
    bool aimbotEnabled = false;
    bool aimHead = true;
    bool aimNeck = false;
    bool aimChest = false;
    float aimFOV = 120.0f;
    float aimRange = 500.0f;
    bool drawFOV = false;

    // ESP
    bool espEnabled = false;
    bool espBox = true;
    bool espHealth = true;
    float espMaxDist = 300.0f;
};

inline Config g_Config;
// A variável g_UnityBase está no offset.hpp, não precisa repetir aqui.
inline uintptr_t g_LocalPlayer = 0;
inline std::vector<uintptr_t> g_Players;
inline bool g_Running = true;
inline std::mutex g_PlayerMutex;

extern mach_port_t g_TaskPort;
