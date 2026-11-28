#include "hack.hpp"
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <mach-o/loader.h>
#include <sys/sysctl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>

char g_LogPath[512] = "";
extern "C" void set_log_path(const char* path) { strncpy(g_LogPath, path, 511); g_LogPath[511] = 0; }
void Log(const std::string& msg) { if(strlen(g_LogPath)) { FILE* f = fopen(g_LogPath, "a"); if(f){fprintf(f,"%s\n",msg.c_str());fclose(f);} } }

static const char* KrName(kern_return_t kr) {
    switch (kr) {
        case KERN_SUCCESS: return "SUCCESS";
        case KERN_INVALID_ADDRESS: return "INVALID_ADDRESS";
        case KERN_PROTECTION_FAILURE: return "PROTECTION_FAILURE";
        case KERN_NO_SPACE: return "NO_SPACE";
        case KERN_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case KERN_FAILURE: return "FAILURE";
        default: return "OTHER";
    }
}

static_assert(sizeof(vm_size_t) == sizeof(uintptr_t), "ARM64 only: vm_size_t must match uintptr_t");
static_assert(sizeof(vm_address_t) == sizeof(uintptr_t), "ARM64 only: vm_address_t must match uintptr_t");

inline bool ToVmAddress(uintptr_t v, vm_address_t& out) { out = (vm_address_t)v; return (uintptr_t)out == v; }
inline bool ToVmSize(uintptr_t v, vm_size_t& out) { out = (vm_size_t)v; return (uintptr_t)out == v; }
inline uintptr_t FromVmSize(vm_size_t v) { return (uintptr_t)v; }
inline uintptr_t FromVmAddress(vm_address_t v) { return (uintptr_t)v; }

inline bool RangeInBounds(uintptr_t addr, uintptr_t len, uintptr_t base, uintptr_t total) {
    uintptr_t addr_end, base_end;
    if (!SafeAdd(addr, len, addr_end)) return false;
    if (!SafeAdd(base, total, base_end)) return false;
    return addr >= base && addr_end <= base_end;
}

struct RegionInfo {
    uintptr_t base;
    uintptr_t size;
    vm_prot_t protection;
};

bool WalkRegion(mach_port_t tp, uintptr_t& address, RegionInfo& out) {
    vm_address_t addr = (vm_address_t)address;
    vm_size_t sz = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object_name = MACH_PORT_NULL;

    kern_return_t kr = vm_region_64(tp, &addr, &sz, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &count, &object_name);
    if (object_name != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), object_name);

    if (kr != KERN_SUCCESS || sz == 0) {
        address += 0x100000;
        if (address == 0) address = 1;
        return false;
    }

    out.base = (uintptr_t)addr;
    out.size = (uintptr_t)sz;
    out.protection = info.protection;
    address = out.base;
    return true;
}

const char* my_strnstr(const char* haystack, size_t haystack_len, const char* needle) {
    size_t needle_len = strlen(needle);
    if (needle_len == 0) return haystack;
    if (haystack_len < needle_len) return NULL;
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (strncmp(&haystack[i], needle, needle_len) == 0) return &haystack[i];
    }
    return NULL;
}

mach_port_t get_task_port(const char* process_name) {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t size = 0;
    if (sysctl(mib, 4, NULL, &size, NULL, 0) < 0) return MACH_PORT_NULL;
    struct kinfo_proc* info = (struct kinfo_proc*)malloc(size);
    if (!info) return MACH_PORT_NULL;
    if (sysctl(mib, 4, info, &size, NULL, 0) < 0) { free(info); return MACH_PORT_NULL; }
    pid_t pid = -1;
    for (int i = 0; i < (int)(size / sizeof(struct kinfo_proc)); i++) {
        if (strstr(info[i].kp_proc.p_comm, process_name) != NULL) {
            pid = info[i].kp_proc.p_pid; break;
        }
    }
    free(info);
    if (pid == -1) return MACH_PORT_NULL;
    mach_port_t task = MACH_PORT_NULL;
    if (task_for_pid(mach_task_self(), pid, &task) != KERN_SUCCESS) return MACH_PORT_NULL;
    return task;
}

bool IsAddressValid(uintptr_t addr, vm_prot_t required_prot) {
    mach_port_t tp = GetTaskPort();
    if (tp == MACH_PORT_NULL || addr == 0) return false;

    uintptr_t probe_addr = addr;
    RegionInfo ri;
    if (!WalkRegion(tp, probe_addr, ri)) return false;
    if (!RangeInBounds(addr, 1, ri.base, ri.size)) return false;
    return (ri.protection & required_prot) == required_prot;
}

uintptr_t FindUnityBase() {
    mach_port_t tp = GetTaskPort();
    if (tp == MACH_PORT_NULL) return 0;

    uintptr_t address = 0x100000000ULL;
    while (address < 0x400000000ULL) {
        RegionInfo ri;
        if (!WalkRegion(tp, address, ri)) continue;

        if ((ri.protection & VM_PROT_READ) == VM_PROT_READ) {
            struct mach_header_64 header;
            vm_address_t vm_addr;
            if (!ToVmAddress(ri.base, vm_addr)) break;

            vm_size_t readSize = sizeof(header);
            kern_return_t kr = vm_read_overwrite(tp, vm_addr, sizeof(header), (vm_address_t)&header, &readSize);

            if (kr == KERN_SUCCESS && readSize == sizeof(header) &&
                header.magic == MH_MAGIC_64 && header.filetype == MH_DYLIB) {

                if (header.ncmds == 0 || header.ncmds > header.sizeofcmds / 4) goto next_region;

                vm_size_t cmdsSize = header.sizeofcmds;
                if (cmdsSize > 0 && cmdsSize < 0x10000) {
                    char* cmds = (char*)malloc(cmdsSize);
                    if (!cmds) goto next_region;

                    uintptr_t addr_cmds;
                    if (!SafeAdd(ri.base, sizeof(header), addr_cmds)) { free(cmds); break; }

                    vm_address_t vm_cmds;
                    if (!ToVmAddress(addr_cmds, vm_cmds)) { free(cmds); break; }

                    vm_size_t actual_read = cmdsSize;
                    kr = vm_read_overwrite(tp, vm_cmds, cmdsSize, (vm_address_t)cmds, &actual_read);

                    if (kr == KERN_SUCCESS && actual_read == cmdsSize) {
                        char* ptr = cmds;
                        uintptr_t cmds_base = (uintptr_t)cmds;
                        uintptr_t cmds_end;
                        if (!SafeAdd(cmds_base, (uintptr_t)cmdsSize, cmds_end)) { free(cmds); break; }

                        bool is_unity = false;
                        uint64_t text_vmaddr = 0;
                        uint64_t text_vmsize = 0;
                        unsigned char uuid[16] = {0};
                        bool has_uuid = false;
                        uint32_t parsed_cmds = 0;
                        uintptr_t accumulated = 0;

                        for (uint32_t i = 0; i < header.ncmds; i++) {
                            if (!RangeInBounds((uintptr_t)ptr, sizeof(struct load_command), cmds_base, (uintptr_t)cmdsSize)) break;
                            struct load_command* lc = (struct load_command*)ptr;

                            if (lc->cmdsize == 0 || lc->cmdsize % 4 != 0) break;
                            if (!RangeInBounds((uintptr_t)ptr, lc->cmdsize, cmds_base, (uintptr_t)cmdsSize)) break;

                            if (lc->cmd == LC_ID_DYLIB && lc->cmdsize >= sizeof(struct dylib_command)) {
                                struct dylib_command* dy = (struct dylib_command*)lc;
                                if (dy->dylib.name.offset < lc->cmdsize) {
                                    uintptr_t name_ptr;
                                    if (SafeAdd((uintptr_t)ptr, dy->dylib.name.offset, name_ptr)) {
                                        size_t max_len = lc->cmdsize - dy->dylib.name.offset;
                                        if (my_strnstr((const char*)name_ptr, max_len, "UnityFramework")) is_unity = true;
                                    }
                                }
                            } else if (lc->cmd == LC_UUID && lc->cmdsize >= sizeof(struct uuid_command)) {
                                struct uuid_command* uc = (struct uuid_command*)lc;
                                memcpy(uuid, uc->uuid, 16);
                                has_uuid = true;
                            } else if (lc->cmd == LC_SEGMENT_64 && lc->cmdsize >= sizeof(struct segment_command_64)) {
                                struct segment_command_64* seg = (struct segment_command_64*)lc;
                                if (strncmp(seg->segname, "__TEXT", 16) == 0) {
                                    text_vmaddr = seg->vmaddr;
                                    text_vmsize = seg->vmsize;
                                }
                            }

                            uintptr_t next;
                            if (!SafeAdd((uintptr_t)ptr, lc->cmdsize, next)) break;
                            ptr = (char*)next;

                            uintptr_t new_acc;
                            if (!SafeAdd(accumulated, lc->cmdsize, new_acc)) break;
                            accumulated = new_acc;
                            parsed_cmds++;
                        }

                        if (is_unity && has_uuid && text_vmsize > 0
                            && parsed_cmds == header.ncmds
                            && accumulated == (uintptr_t)header.sizeofcmds) {
                            uintptr_t base;
                            if (SafeSub(ri.base, (uintptr_t)text_vmaddr, base)) {
                                if (IsAddressValid(base, VM_PROT_EXECUTE)) {
                                    char hex_buf[64];
                                    snprintf(hex_buf, sizeof(hex_buf), "0x%" PRIxPTR, base);
                                    Log("[+] UnityBase: " + std::string(hex_buf));
                                    free(cmds);
                                    return base;
                                }
                            }
                        }
                    }
                    free(cmds);
                }
            }
        }
    next_region:
        {
            uintptr_t next_addr;
            if (!SafeAdd(ri.base, ri.size, next_addr)) break;
            address = next_addr;
        }
    }
    return 0;
}

void CollectRegions(mach_port_t tp, std::vector<std::pair<uintptr_t, uintptr_t>>& regions) {
    uintptr_t address = 0x100000000ULL;
    uint64_t walk_ok = 0;
    uint64_t walk_miss = 0;
    uint64_t readable_count = 0;
    uint64_t unreadable_count = 0;
    char msg[320];

    Log("[REGIONS] coleta: inicio; faixa=[0x100000000,0x400000000).");

    while (address < 0x400000000ULL) {
        const uintptr_t probe = address;
        RegionInfo ri;
        if (!WalkRegion(tp, address, ri)) {
            ++walk_miss;
            continue;
        }

        ++walk_ok;
        const bool readable = (ri.protection & VM_PROT_READ) == VM_PROT_READ;
        snprintf(msg, sizeof(msg),
                 "[REGIONS] #%" PRIu64 " base=0x%" PRIxPTR " size=0x%" PRIxPTR
                 " end=0x%" PRIxPTR " prot=0x%x readable=%s",
                 walk_ok, ri.base, ri.size,
                 (ri.base <= UINTPTR_MAX - ri.size) ? ri.base + ri.size : 0,
                 (unsigned int)ri.protection, readable ? "yes" : "no");
        Log(msg);

        if (readable) {
            regions.push_back({ri.base, ri.size});
            ++readable_count;
        } else {
            ++unreadable_count;
        }

        uintptr_t next;
        if (!SafeAdd(ri.base, ri.size, next)) {
            snprintf(msg, sizeof(msg),
                     "[REGIONS] interrupcao: overflow em base=0x%" PRIxPTR
                     " size=0x%" PRIxPTR, ri.base, ri.size);
            Log(msg);
            break;
        }
        if (next <= probe) {
            Log("[REGIONS] interrupcao: proximo endereco nao avancou.");
            break;
        }
        address = next;
    }

    snprintf(msg, sizeof(msg),
             "[REGIONS] coleta: fim walk_ok=%" PRIu64 " walk_miss=%" PRIu64
             " readable=%" PRIu64 " unreadable=%" PRIu64 " guardadas=%zu",
             walk_ok, walk_miss, readable_count, unreadable_count, regions.size());
    Log(msg);
}

void DumpCameraOffsets() {}

uintptr_t FindStaticMatchGame() {
    mach_port_t tp = GetTaskPort();
    if (tp == MACH_PORT_NULL) return 0;

    FacadeCache cache = GetFacadeCache();
    if (cache.static_fields != 0 && cache.class_ptr != 0) {
        uintptr_t mg_field, matchGame = 0;
        if (SafeAdd(cache.static_fields, 0x8, mg_field) && ReadMemory<uintptr_t>(mg_field, matchGame) && matchGame != 0) {
            uintptr_t objKlass = 0;
            if (ReadMemory<uintptr_t>(matchGame, objKlass) && objKlass == cache.class_ptr) {
                return matchGame;
            }
        }
        SetFacadeCache(0, 0);
    }

    std::vector<std::pair<uintptr_t, uintptr_t>> regions;
    Log("[SCAN] First-pass: iniciando coleta de regioes legiveis.");
    CollectRegions(tp, regions);

    char log_buf[384];
    snprintf(log_buf, sizeof(log_buf),
             "[SCAN] First-pass: coleta concluida; regioes=%zu.", regions.size());
    Log(log_buf);

    uintptr_t string_addr = 0;
    const char* target_name = "GameFacade";

    const uintptr_t CHUNK = 1ULL * 1024 * 1024;
    const uintptr_t OVERLAP = 15;
    uintptr_t READ_SIZE = 0;
    if (!SafeAdd(CHUNK, OVERLAP, READ_SIZE)) {
        Log("[SCAN] First-pass: erro de overflow calculando READ_SIZE.");
        return 0;
    }

    uint64_t total_reads = 0;
    uint64_t total_exact = 0;
    uint64_t total_partial = 0;
    uint64_t total_failed = 0;
    uint64_t total_requested = 0;
    uint64_t total_received = 0;
    size_t region_index = 0;

    snprintf(log_buf, sizeof(log_buf),
             "[SCAN] First-pass: inicio da leitura; chunk=0x%" PRIxPTR
             " overlap=0x%" PRIxPTR " read_size=0x%" PRIxPTR
             " alvos=GameFacade|MatchGame.",
             CHUNK, OVERLAP, READ_SIZE);
    Log(log_buf);

    for (const auto& reg : regions) {
        ++region_index;
        uintptr_t reg_end = 0;
        uint64_t region_reads = 0;
        uint64_t region_exact = 0;
        uint64_t region_partial = 0;
        uint64_t region_failed = 0;
        uint64_t region_requested = 0;
        uint64_t region_received = 0;

        snprintf(log_buf, sizeof(log_buf),
                 "[SCAN] region[%zu/%zu] begin base=0x%" PRIxPTR
                 " size=0x%" PRIxPTR,
                 region_index, regions.size(), reg.first, reg.second);
        Log(log_buf);

        if (!SafeAdd(reg.first, reg.second, reg_end)) {
            snprintf(log_buf, sizeof(log_buf),
                     "[SCAN] region[%zu] skip: overflow base=0x%" PRIxPTR
                     " size=0x%" PRIxPTR,
                     region_index, reg.first, reg.second);
            Log(log_buf);
            continue;
        }

        snprintf(log_buf, sizeof(log_buf),
                 "[SCAN] region[%zu] range=[0x%" PRIxPTR ",0x%" PRIxPTR
                 ") bytes=0x%" PRIxPTR,
                 region_index, reg.first, reg_end, reg.second);
        Log(log_buf);

        char* buf = (char*)malloc(READ_SIZE);
        if (!buf) {
            snprintf(log_buf, sizeof(log_buf),
                     "[SCAN] region[%zu] skip: malloc falhou size=0x%" PRIxPTR,
                     region_index, READ_SIZE);
            Log(log_buf);
            continue;
        }

        uintptr_t address = reg.first;
        while (address < reg_end) {
            const uintptr_t remaining = reg_end - address;
            const uintptr_t want = (remaining > READ_SIZE) ? READ_SIZE : remaining;

            vm_address_t vm_addr;
            vm_size_t vm_size;
            if (!ToVmAddress(address, vm_addr) || !ToVmSize(want, vm_size)) {
                snprintf(log_buf, sizeof(log_buf),
                         "[SCAN] read-skip region[%zu] addr=0x%" PRIxPTR
                         " want=0x%" PRIxPTR " reason=conversion-failed",
                         region_index, address, want);
                Log(log_buf);

                uintptr_t next;
                if (!SafeAdd(address, CHUNK, next) || next <= address) {
                    Log("[SCAN] read-skip: overflow ou ausencia de progresso.");
                    break;
                }
                address = next;
                continue;
            }

            ++total_reads;
            ++region_reads;
            total_requested += (uint64_t)want;
            region_requested += (uint64_t)want;

            snprintf(log_buf, sizeof(log_buf),
                     "[SCAN] read-begin region[%zu] seq=%" PRIu64
                     " addr=0x%" PRIxPTR " requested=%" PRIuPTR
                     " (0x%" PRIxPTR ")",
                     region_index, region_reads, address, want, want);
            Log(log_buf);

            vm_size_t got = vm_size;
            const kern_return_t kr = vm_read_overwrite(
                tp, vm_addr, vm_size, (vm_address_t)buf, &got);
            const uint64_t got_bytes = (uint64_t)got;
            total_received += got_bytes;
            region_received += got_bytes;

            const bool exact_read =
                (kr == KERN_SUCCESS && (uintptr_t)got == want);
            const bool partial_read =
                (kr == KERN_SUCCESS && (uintptr_t)got != want);

            if (exact_read) {
                ++total_exact;
                ++region_exact;
            } else if (partial_read) {
                ++total_partial;
                ++region_partial;
            } else {
                ++total_failed;
                ++region_failed;
            }

            snprintf(log_buf, sizeof(log_buf),
                     "[SCAN] read-end region[%zu] seq=%" PRIu64
                     " addr=0x%" PRIxPTR " result=%s kr=%d(%s)"
                     " bytes=%" PRIu64 "/%" PRIuPTR,
                     region_index, region_reads, address,
                     exact_read ? "EXACT" : (partial_read ? "PARTIAL" : "ERROR"),
                     (int)kr, KrName(kr), got_bytes, want);
            Log(log_buf);

            if (exact_read && got > OVERLAP) {
                for (vm_size_t i = 0; i < got - OVERLAP; ++i) {
                    const bool is_facade =
                        (memcmp(buf + i, "GameFacade", 10) == 0 && buf[i + 10] == '\0');
                    const bool is_match_game =
                        (memcmp(buf + i, "MatchGame", 9) == 0 && buf[i + 9] == '\0');

                    if (is_facade || is_match_game) {
                        uintptr_t found_addr = 0;
                        if (!SafeAdd(address, (uintptr_t)i, found_addr)) {
                            Log("[SCAN] match ignorado: overflow calculando endereco.");
                            break;
                        }

                        string_addr = found_addr;
                        target_name = is_match_game ? "MatchGame" : "GameFacade";
                        snprintf(log_buf, sizeof(log_buf),
                                 "[SCAN] match region[%zu] addr=0x%" PRIxPTR
                                 " name=%s buffer_offset=0x%" PRIxPTR,
                                 region_index, string_addr, target_name, (uintptr_t)i);
                        Log(log_buf);
                        break;
                    }
                }
            } else if (partial_read) {
                snprintf(log_buf, sizeof(log_buf),
                         "[SCAN] read-nao-pesquisada region[%zu] addr=0x%" PRIxPTR
                         " motivo=leitura-parcial got=%" PRIu64 " want=%" PRIuPTR,
                         region_index, address, got_bytes, want);
                Log(log_buf);
            } else {
                snprintf(log_buf, sizeof(log_buf),
                         "[SCAN] read-nao-pesquisada region[%zu] addr=0x%" PRIxPTR
                         " motivo=erro-de-leitura kr=%d(%s)",
                         region_index, address, (int)kr, KrName(kr));
                Log(log_buf);
            }

            if (string_addr != 0) break;

            uintptr_t next;
            if (!SafeAdd(address, CHUNK, next) || next <= address) {
                Log("[SCAN] First-pass: overflow ou ausencia de progresso ao avancar chunk.");
                break;
            }
            address = next;
        }

        snprintf(log_buf, sizeof(log_buf),
                 "[SCAN] region[%zu] end reads=%" PRIu64
                 " exact=%" PRIu64 " partial=%" PRIu64 " failed=%" PRIu64
                 " requested=%" PRIu64 " received=%" PRIu64 " match=%s",
                 region_index, region_reads, region_exact, region_partial,
                 region_failed, region_requested, region_received,
                 string_addr ? "yes" : "no");
        Log(log_buf);

        free(buf);
        if (string_addr != 0) break;
    }

    snprintf(log_buf, sizeof(log_buf),
             "[SCAN] First-pass: fim regions_visited=%zu reads=%" PRIu64
             " exact=%" PRIu64 " partial=%" PRIu64 " failed=%" PRIu64
             " requested=%" PRIu64 " received=%" PRIu64 " match=%s",
             region_index, total_reads, total_exact, total_partial, total_failed,
             total_requested, total_received, string_addr ? "yes" : "no");
    Log(log_buf);

    if (string_addr == 0) {
        Log("[-] Falhou: String 'GameFacade' ou 'MatchGame' nao encontrada.");
        return 0;
    }

    snprintf(log_buf, sizeof(log_buf), "[SCAN] String '%s' achada em 0x%" PRIxPTR, target_name, string_addr);
    Log(log_buf);

    uintptr_t class_ptr = 0;
    uintptr_t static_fields = 0;

    Log("[SCAN] Second-pass: buscando Il2CppClass (pode demorar).");
    for (auto& reg : regions) {
        uintptr_t reg_end;
        if (!SafeAdd(reg.first, reg.second, reg_end)) continue;

        uintptr_t* buf2 = (uintptr_t*)malloc(CHUNK);
        if (!buf2) continue;

        uintptr_t addr2 = reg.first;
        while (addr2 < reg_end) {
            uintptr_t rem2 = reg_end - addr2;
            uintptr_t want2 = (rem2 > CHUNK) ? CHUNK : rem2;

            vm_address_t vm_addr2;
            vm_size_t vm_size2;
            if (!ToVmAddress(addr2, vm_addr2) || !ToVmSize(want2, vm_size2)) {
                uintptr_t next;
                if (!SafeAdd(addr2, CHUNK, next)) break;
                addr2 = next;
                continue;
            }

            vm_size_t got2 = vm_size2;
            if (vm_read_overwrite(tp, vm_addr2, vm_size2, (vm_address_t)buf2, &got2) == KERN_SUCCESS && (uintptr_t)got2 == want2) {
                for (size_t j = 0; j < got2 / sizeof(uintptr_t); j++) {
                    if (buf2[j] == string_addr) {
                        uintptr_t ptr_loc;
                        if (!SafeAdd(addr2, j * sizeof(uintptr_t), ptr_loc)) continue;

                        for (int64_t off = -0x40; off <= 0x20; off += 8) {
                            uintptr_t cand = 0;
                            if (off < 0) {
                                if (!SafeSub(ptr_loc, (uintptr_t)(-off), cand)) continue;
                            } else {
                                if (!SafeAdd(ptr_loc, (uintptr_t)off, cand)) continue;
                            }

                            uintptr_t name_field, name_ptr;
                            if (!SafeAdd(cand, 0x10, name_field)) continue;
                            if (!ReadMemory<uintptr_t>(name_field, name_ptr)) continue;
                            if (name_ptr != string_addr) continue;

                            const uintptr_t sf_offsets[] = {
                                0xB8, 0xC0, 0xA8, 0xB0, 0x90, 0x98, 0xE0, 0xE8, 
                                0x88, 0x80, 0xD0, 0x70, 0x78, 0xF0, 0xF8, 0x100,
                                0x108, 0x110, 0x118, 0x120, 0x128, 0x130, 0x138
                            };
                            
                            for (uintptr_t sf_off : sf_offsets) {
                                uintptr_t sf_field, sf;
                                if (!SafeAdd(cand, sf_off, sf_field)) continue;
                                if (!ReadMemory<uintptr_t>(sf_field, sf)) continue;
                                if (!IsAddressValid(sf, VM_PROT_READ)) continue;

                                uintptr_t mg_field, matchGame;
                                if (!SafeAdd(sf, 0x8, mg_field)) continue;
                                if (!ReadMemory<uintptr_t>(mg_field, matchGame) || matchGame == 0) continue;

                                uintptr_t objKlass = 0;
                                if (ReadMemory<uintptr_t>(matchGame, objKlass) && objKlass == cand) {
                                    class_ptr = cand;
                                    static_fields = sf;
                                    break;
                                }
                            }
                            if (class_ptr) break;
                        }
                    }
                    if (class_ptr) break;
                }
            }
            if (class_ptr) break;

            uintptr_t next;
            if (!SafeAdd(addr2, CHUNK, next)) break;
            addr2 = next;
        }
        free(buf2);
        if (class_ptr) break;
    }

    if (class_ptr == 0) {
        Log("[-] Falhou: Nenhum Il2CppClass apontando pra string foi encontrado.");
        return 0;
    }

    uintptr_t mg_field, matchGame;
    if (!SafeAdd(static_fields, 0x8, mg_field)) return 0;
    if (!ReadMemory<uintptr_t>(mg_field, matchGame) || matchGame == 0) {
        Log("[-] Falhou: static_fields + 0x8 deu null.");
        return 0;
    }

    uintptr_t objKlass = 0;
    if (ReadMemory<uintptr_t>(matchGame, objKlass) && objKlass == class_ptr) {
        SetFacadeCache(static_fields, class_ptr);
        snprintf(log_buf, sizeof(log_buf), "[+] MatchGame Encontrado: 0x%" PRIxPTR " (klass 0x%" PRIxPTR ")", matchGame, class_ptr);
        Log(log_buf);
        return matchGame;
    } else {
        Log("[-] Falhou: O objeto achado nao e um MatchGame (klass mismatch).");
    }
    return 0;
}
