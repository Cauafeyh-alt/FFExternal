#pragma once
#include <cstdint>
#include <cmath>
#include <vector>
#include <mutex>
#include <string>
#include <mach/mach.h>
#include <signal.h>
#include <setjmp.h>
#include "offset.hpp"

void Log(const std::string& msg);

inline sigjmp_buf jump_buffer;

// O FIX ESTA AQUI: static inline
static inline void segv_handler(int sig) {
    siglongjmp(jump_buffer, 1);
}

// SafeCall: executa a RVA, se der crash, volta e retorna 0
template <typename Ret, typename Func, typename... Args>
Ret SafeCall(uintptr_t address, Args... args) {
    if (address == 0) return Ret{};
    
    struct sigaction sa, old_segv, old_bus;
    sa.sa_handler = segv_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER;
    
    sigaction(SIGSEGV, &sa, &old_segv);
    sigaction(SIGBUS, &sa, &old_bus);
    
    Ret result{};
    if (sigsetjmp(jump_buffer, 1) == 0) {
        auto func = reinterpret_cast<Func>(address);
        result = func(args...);
    } else {
        Log("[-] CRASH na RVA: 0x" + std::to_string(address));
    }
    
    sigaction(SIGSEGV, &old_segv, NULL);
    sigaction(SIGBUS, &old_bus, NULL);
    
    return result;
}

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

mach_port_t get_task_port(const char* process_name);
uintptr_t GetUnityBase();

void HackLoop();
void AimbotThread();
void EspThread();
