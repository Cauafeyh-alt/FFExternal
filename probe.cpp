// probe.cpp
#include "hack.hpp"
#include "probe.hpp"

#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace probe {

// Candidate offsets from dump.cs. All remain UNVERIFIED.
static constexpr uintptr_t CAND_CUR_HP    = 0x1BC;
static constexpr uintptr_t CAND_MAX_HP    = 0x1C0;
static constexpr uintptr_t CAND_IS_DEAD   = 0x07C;
static constexpr uintptr_t CAND_UNIQUE_ID = 0x060;
static constexpr uintptr_t CAND_TEAM_MODE = 0x434;

static std::atomic<bool> g_probeAlive{false};
static std::atomic<bool> g_dumpList{false};
static std::thread       g_thread;
static std::mutex        g_lifecycleMx;

static bool readI8At(uintptr_t base, uintptr_t off,
                     int8_t& out, bool& ok) {
    uintptr_t addr = 0;
    if (!SafeAdd(base, off, addr)) {
        ok = false;
        return false;
    }
    ok = ReadMemory<int8_t>(addr, out);
    return ok;
}

static bool readI32At(uintptr_t base, uintptr_t off,
                      int32_t& out, bool& ok) {
    uintptr_t addr = 0;
    if (!SafeAdd(base, off, addr)) {
        ok = false;
        return false;
    }
    ok = ReadMemory<int32_t>(addr, out);
    return ok;
}

static inline uint64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}

// Read one four-byte sample; derive integer and float views from identical bits.
static void logFieldRaw(const char* tag, uintptr_t base,
                        uintptr_t off, const char* label) {
    uintptr_t addr = 0;
    if (!SafeAdd(base, off, addr)) {
        Log(std::string("[probe] ") + tag + " " + label + " SafeAdd falhou");
        return;
    }

    uint32_t bits = 0;
    const bool ok = ReadMemory<uint32_t>(addr, bits);

    int32_t asInt = 0;
    float asF = 0.0f;
    std::memcpy(&asInt, &bits, sizeof(asInt));
    std::memcpy(&asF, &bits, sizeof(asF));

    char b[256];
    std::snprintf(b, sizeof(b),
        "[probe] %s %s addr=0x%" PRIxPTR
        " ok=%d int=%d float=%.4f bits=0x%08X",
        tag, label, addr, ok ? 1 : 0, asInt, asF, bits);
    Log(std::string(b));
}

// This function is the single owner of all local-player transition messages.
static void dumpLocal(uintptr_t lp) {
    static uintptr_t lastLP = 0;
    static bool seenAny = false;

    if (!lp) {
        if (lastLP != 0) {
            Log("[probe] lp=0 (perdeu jogador)");
            lastLP = 0;
        }
        return;
    }

    if (!seenAny) {
        Log("[probe] lp primeiro ponteiro observado");
        seenAny = true;
    } else if (lastLP == 0) {
        Log("[probe] lp recuperado");
    } else if (lastLP != lp) {
        char t[128];
        std::snprintf(t, sizeof(t),
            "[probe] lp trocou 0x%" PRIxPTR " -> 0x%" PRIxPTR,
            lastLP, lp);
        Log(std::string(t));
    }
    lastLP = lp;

    logFieldRaw("lp", lp, CAND_CUR_HP, "CUR_HP?");
    logFieldRaw("lp", lp, CAND_MAX_HP, "MAX_HP?");

    int8_t dead = 0;
    bool okD = false;
    readI8At(lp, CAND_IS_DEAD, dead, okD);

    int32_t uid = 0;
    bool okU = false;
    readI32At(lp, CAND_UNIQUE_ID, uid, okU);

    int32_t team = 0;
    bool okT = false;
    readI32At(lp, CAND_TEAM_MODE, team, okT);

    char b[224];
    std::snprintf(b, sizeof(b),
        "[probe] lp=0x%" PRIxPTR
        " dead=%s%d uid=%s%d team=%s%d ts=%" PRIu64,
        lp,
        okD ? "" : "?", static_cast<int>(dead),
        okU ? "" : "?", uid,
        okT ? "" : "?", team,
        now_ms());
    Log(std::string(b));
}

// Optional entity-list diagnostics. Disabled by default; copy under lock,
// perform all remote reads and logging after releasing the lock.
static void dumpPlayerList() {
    if (!g_dumpList.load(std::memory_order_relaxed)) return;

    std::vector<uintptr_t> snap;
    {
        std::lock_guard<std::mutex> lk(g_PlayerMutex);
        snap.assign(g_Players.begin(), g_Players.end());
    }

    char hdr[96];
    std::snprintf(hdr, sizeof(hdr), "[probe] g_Players n=%zu", snap.size());
    Log(std::string(hdr));

    const size_t n = snap.size() < 32 ? snap.size() : 32;
    for (size_t i = 0; i < n; ++i) {
        const uintptr_t p = snap[i];
        if (!p) continue;

        logFieldRaw("ent", p, CAND_CUR_HP, "CUR_HP?");
        logFieldRaw("ent", p, CAND_MAX_HP, "MAX_HP?");

        int8_t d = 0;
        bool okD = false;
        readI8At(p, CAND_IS_DEAD, d, okD);

        int32_t u = 0;
        bool okU = false;
        readI32At(p, CAND_UNIQUE_ID, u, okU);

        char b[160];
        std::snprintf(b, sizeof(b),
            "[probe] ent[%zu]=0x%" PRIxPTR " dead=%s%d uid=%s%d",
            i, p,
            okD ? "" : "?", static_cast<int>(d),
            okU ? "" : "?", u);
        Log(std::string(b));
    }
}

static void loop() {
    Log("[probe] thread iniciada");
    while (g_probeAlive.load(std::memory_order_relaxed) &&
           g_Running.load(std::memory_order_relaxed)) {
        const uintptr_t lp = GetLocalPlayer();
        dumpLocal(lp);
        dumpPlayerList();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    Log("[probe] thread encerrada");
}

bool Start() {
    std::lock_guard<std::mutex> lk(g_lifecycleMx);
    if (g_probeAlive.load(std::memory_order_relaxed)) return true;
    if (g_thread.joinable()) g_thread.join();

    g_probeAlive.store(true, std::memory_order_relaxed);
    try {
        g_thread = std::thread(loop);
    } catch (const std::system_error& e) {
        g_probeAlive.store(false, std::memory_order_relaxed);
        Log(std::string("[probe] Start falhou: ") + e.what());
        return false;
    }
    return true;
}

void Stop() {
    std::lock_guard<std::mutex> lk(g_lifecycleMx);
    if (!g_probeAlive.exchange(false, std::memory_order_relaxed)) return;
    if (g_thread.joinable()) g_thread.join();
    Log("[probe] Stop() join limpo");
}

void SetDumpList(bool on) {
    g_dumpList.store(on, std::memory_order_relaxed);
}

} // namespace probe
