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

static_assert(sizeof(vm_size_t) == sizeof(uintptr_t), "ARM64 only: vm_size_t must match uintptr_t");
static_assert(sizeof(vm_address_t) == sizeof(uintptr_t), "ARM64 only: vm_address_t must match uintptr_t");

inline bool ToVmAddress(uintptr_t v, vm_address_t& out) {
    out = (vm_address_t)v;
    return (uintptr_t)out == v;
}
inline bool ToVmSize(uintptr_t v, vm_size_t& out) {
    out = (vm_size_t)v;
    return (uintptr_t)out == v;
}
inline uintptr_t FromVmSize(vm_size_t v) { return (uintptr_t)v; }
inline uintptr_t FromVmAddress(vm_address_t v) { return (uintptr_t)v; }

inline bool RangeInBounds(uintptr_t addr, uintptr_t len, uintptr_t base, uintptr_t total) {
    uintptr_t addr_end, base_end;
    if (!SafeAdd(addr, len, addr_end)) return false;
    if (!SafeAdd(base, total, base_end)) return false;
    return addr >= base && addr_end <= base_end;
}

inline const char* KrName(kern_return_t kr) {
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

struct RegionState {
    uintptr_t base;
    uintptr_t size;
    natural_t depth;
    bool operator==(const RegionState& o) const {
        return base == o.base && size == o.size && depth == o.depth;
    }
};

struct RegionStateHash {
    std::size_t operator()(const RegionState& s) const {
        return std::hash<uintptr_t>()(s.base) ^ 
               (std::hash<uintptr_t>()(s.size) << 1) ^ 
               (std::hash<natural_t>()(s.depth) << 2);
    }
};

struct WalkGuard {
    std::unordered_set<RegionState, RegionStateHash> visited;
    static constexpr size_t MAX_VISITED = 4096; 
    
    bool CheckAndAdd(uintptr_t base, uintptr_t size, natural_t depth) {
        if (visited.size() >= MAX_VISITED) {
            Log("[-] Limite de nos visitados atingido. Abortando scan.");
            return false;
        }
        RegionState state{base, size, depth};
        auto res = visited.insert(state);
        if (!res.second) {
            Log("[-] Estado de regiao repetido (loop). Abortando scan.");
            return false;
        }
        return true;
    }
};

struct RegionInfo {
    uintptr_t base;
    uintptr_t size;
    vm_prot_t protection;
    bool is_submap;
    natural_t depth;
};

bool WalkRegion(mach_port_t tp, uintptr_t& address, natural_t& depth, RegionInfo& out, WalkGuard& guard) {
    vm_address_t addr = (vm_address_t)address;
    vm_size_t sz = 0;
    struct vm_region_submap_info_64 info;
    mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
    
    kern_return_t kr = vm_region_recurse_64(tp, &addr, &sz, &depth, (vm_region_recurse_info_t)&info, &count);
    if (kr != KERN_SUCCESS) {
        if (kr != KERN_INVALID_ADDRESS) {
            Log(std::string("[-] vm_region_recurse_64 kr=") + KrName(kr) + " at=0x" + std::to_string(address));
        }
        return false;
    }

    uintptr_t curr_base = FromVmAddress(addr);
    uintptr_t curr_size = FromVmSize(sz);

    if (!guard.CheckAndAdd(curr_base, curr_size, depth)) return false;

    out.base = curr_base;
    out.size = curr_size;
    out.protection = info.protection;
    out.is_submap = info.is_submap;
    out.depth = depth;
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
    natural_t depth = 0;
    RegionInfo ri;
    WalkGuard guard;
    if (!WalkRegion(tp, probe_addr, depth, ri, guard)) return false;
    if (ri.is_submap) return false;
    if (!RangeInBounds(addr, 1, ri.base, ri.size)) return false;
    return (ri.protection & required_prot) == required_prot;
}

uintptr_t FindUnityBase() {
    mach_port_t tp = GetTaskPort();
    if (tp == MACH_PORT_NULL) return 0;

    uintptr_t address = 0;
    natural_t depth = 0;
    WalkGuard guard;
    
    while (true) {
        RegionInfo ri;
        if (!WalkRegion(tp, address, depth, ri, guard)) break;
        
        if (ri.is_submap) {
            depth++;
            address = ri.base;
            continue;
        }

        if ((ri.protection & VM_PROT_READ) == VM_PROT_READ) {
            struct mach_header_64 header;
            vm_address_t vm_addr;
            if (!ToVmAddress(address, vm_addr)) break;

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
                    if (!SafeAdd(address, sizeof(header), addr_cmds)) { free(cmds); break; }
                    
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
                            if (SafeSub(address, (uintptr_t)text_vmaddr, base)) {
                                if (IsAddressValid(base, VM_PROT_EXECUTE)) {
                                    char hex_buf[64];
                                    snprintf(hex_buf, sizeof(hex_buf), "0x%" PRIxPTR, base);
                                    Log("[+] UnityBase: " + std::string(hex_buf));
                                    free(cmds);
                                    return base;
                                }
                            }
                        }
                    } else {
                        Log(std::string("[-] vm_read_overwrite(cmds) kr=") + KrName(kr) + " got=" + std::to_string(actual_read) + "/" + std::to_string(cmdsSize));
                    }
                    free(cmds);
                }
            }
        }
    next_region:
        {
            uintptr_t next_addr;
            if (!SafeAdd(address, ri.size, next_addr)) break;
            address = next_addr;
        }
    }
    return 0;
}

void CollectRegionsRecursive(mach_port_t tp, uintptr_t start, uintptr_t size, natural_t depth, std::vector<std::pair<uintptr_t, uintptr_t>>& regions, WalkGuard& guard) {
    if (depth > 32) return;
    uintptr_t end;
    if (!SafeAdd(start, size, end)) return;
    uintptr_t address = start;
    natural_t curr_depth = depth;

    while (address < end) {
        RegionInfo ri;
        if (!WalkRegion(tp, address, curr_depth, ri, guard)) break;
        
        uintptr_t next;
        if (!SafeAdd(ri.base, ri.size, next)) break;

        if (next <= address) {
            Log("[-] Loop detectado: endereco nao avancou.");
            break;
        }

        if (ri.is_submap) {
            natural_t nd = (curr_depth < 32) ? (curr_depth + 1) : 32;
            CollectRegionsRecursive(tp, ri.base, ri.size, nd, regions, guard);
        } else if ((ri.protection & VM_PROT_READ) == VM_PROT_READ) {
            regions.push_back({ri.base, ri.size});
        }
        address = next;
    }
}

void DumpCameraOffsets() {
    uintptr_t cam = GetGameCamera();
    if (cam == 0) return;
    Matrix4x4 view = {}, proj = {};
    if (!ReadMemory<Matrix4x4>(cam + Offsets::Camera_ViewMatrix, view)) return;
    if (!ReadMemory<Matrix4x4>(cam + Offsets::Camera_ProjectionMatrix, proj)) return;
    char buf[512];
    snprintf(buf, sizeof(buf), "[DUMP] View[0][0]=%f View[1][1]=%f View[2][2]=%f View[3][0]=%f", 
             view.m[0][0], view.m[1][1], view.m[2][2], view.m[3][0]);
    Log(buf);
    snprintf(buf, sizeof(buf), "[DUMP] Proj[0][0]=%f Proj[1][1]=%f Proj[2][2]=%f Proj[2][3]=%f Proj[3][2]=%f", 
             proj.m[0][0], proj.m[1][1], proj.m[2][2], proj.m[2][3], proj.m[3][2]);
    Log(buf);
}

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

    uintptr_t string_addr = 0;
    {
        std::vector<std::pair<uintptr_t, uintptr_t>> regions;
        WalkGuard guard;
        CollectRegionsRecursive(tp, 0x100000000ULL, 0x700000000000ULL, 0, regions, guard);

        const uintptr_t CHUNK = 4ULL * 1024 * 1024;
        const uintptr_t OVERLAP = 15;
        uintptr_t READ_SIZE;
        if (!SafeAdd(CHUNK, OVERLAP, READ_SIZE)) return 0;

        for (auto& reg : regions) {
            uintptr_t reg_end;
            if (!SafeAdd(reg.first, reg.second, reg_end)) continue;

            char* buf = (char*)malloc(READ_SIZE);
            if (!buf) continue;

            uintptr_t address = reg.first;
            while (address < reg_end) {
                uintptr_t remaining = reg_end - address;
                uintptr_t want = (remaining > READ_SIZE) ? READ_SIZE : remaining;
                
                vm_address_t vm_addr;
                vm_size_t vm_size;
                if (!ToVmAddress(address, vm_addr) || !ToVmSize(want, vm_size)) {
                    uintptr_t next;
                    if (!SafeAdd(address, CHUNK, next)) break;
                    address = next;
                    continue;
                }

                vm_size_t got = vm_size;
                kern_return_t kr = vm_read_overwrite(tp, vm_addr, vm_size, (vm_address_t)buf, &got);
                
                if (kr == KERN_SUCCESS && (uintptr_t)got == want && got > OVERLAP) {
                    for (vm_size_t i = 0; i < got - OVERLAP; i++) {
                        if (memcmp(buf + i, "GameFacade", 10) == 0 && buf[i + 10] == '\0') {
                            if (!SafeAdd(address, i, string_addr)) string_addr = 0;
                            break;
                        }
                    }
                }
                if (string_addr) break;

                uintptr_t next;
                if (!SafeAdd(address, CHUNK, next)) break;
                address = next;
            }
            free(buf);
            if (string_addr) break;
        }
    }
    if (string_addr == 0) return 0;

    uintptr_t class_ptr = 0;
    uintptr_t static_fields = 0;
    {
        std::vector<std::pair<uintptr_t, uintptr_t>> regions;
        WalkGuard guard;
        CollectRegionsRecursive(tp, 0x100000000ULL, 0x700000000000ULL, 0, regions, guard);

        const uintptr_t CHUNK = 4ULL * 1024 * 1024;
        const uintptr_t name_offsets[]   = {0x10, 0x08, 0x00};
        const uintptr_t static_offsets[] = {0xB8, 0xC0, 0xA8, 0xB0, 0x90, 0x98};

        for (auto& reg : regions) {
            uintptr_t reg_end;
            if (!SafeAdd(reg.first, reg.second, reg_end)) continue;

            uintptr_t* buf = (uintptr_t*)malloc(CHUNK);
            if (!buf) continue;

            uintptr_t address = reg.first;
            while (address < reg_end) {
                uintptr_t remaining = reg_end - address;
                uintptr_t want = (remaining > CHUNK) ? CHUNK : remaining;
                
                vm_address_t vm_addr;
                vm_size_t vm_size;
                if (!ToVmAddress(address, vm_addr) || !ToVmSize(want, vm_size)) {
                    uintptr_t next;
                    if (!SafeAdd(address, CHUNK, next)) break;
                    address = next;
                    continue;
                }

                vm_size_t got = vm_size;
                kern_return_t kr = vm_read_overwrite(tp, vm_addr, vm_size, (vm_address_t)buf, &got);
                
                if (kr != KERN_SUCCESS || (uintptr_t)got != want) {
                    uintptr_t next;
                    if (!SafeAdd(address, CHUNK, next)) break;
                    address = next;
                    continue;
                }

                for (size_t i = 0; i < got / sizeof(uintptr_t) && !class_ptr; i++) {
                    if (buf[i] != string_addr) continue;

                    uintptr_t i_bytes, entry_addr;
                    if (!SafeMul((uintptr_t)i, (uintptr_t)sizeof(uintptr_t), i_bytes)) continue;
                    if (!SafeAdd(address, i_bytes, entry_addr)) continue;

                    for (uintptr_t name_off : name_offsets) {
                        if (entry_addr < name_off) continue;
                        uintptr_t cand = entry_addr - name_off;

                        uintptr_t cand_name_field, cand_name_ptr;
                        if (!SafeAdd(cand, 0x10, cand_name_field)) continue;
                        if (!ReadMemory<uintptr_t>(cand_name_field, cand_name_ptr)) continue;
                        if (cand_name_ptr != string_addr) continue;

                        for (uintptr_t sf_off : static_offsets) {
                            uintptr_t sf_field, sf;
                            if (!SafeAdd(cand, sf_off, sf_field)) continue;
                            if (!ReadMemory<uintptr_t>(sf_field, sf)) continue;
                            if (!IsAddressValid(sf, VM_PROT_READ)) continue;
                            class_ptr = cand;
                            static_fields = sf;
                            break;
                        }
                        if (class_ptr) break;
                    }
                }
                if (class_ptr) break;

                uintptr_t next;
                if (!SafeAdd(address, CHUNK, next)) break;
                address = next;
            }
            free(buf);
            if (class_ptr) break;
        }
    }
    if (class_ptr == 0) return 0;

    uintptr_t mg_field, matchGame;
    if (!SafeAdd(static_fields, 0x8, mg_field)) return 0;
    if (!ReadMemory<uintptr_t>(mg_field, matchGame) || matchGame == 0) return 0;

    uintptr_t objKlass = 0;
    if (ReadMemory<uintptr_t>(matchGame, objKlass) && objKlass == class_ptr) {
        SetFacadeCache(static_fields, class_ptr);
        return matchGame;
    }
    return 0;
}
