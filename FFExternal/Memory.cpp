#include "hack.hpp"
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <mach-o/loader.h>
#include <string>

char g_LogPath[512] = "";

extern "C" void set_log_path(const char* path) {
    strncpy(g_LogPath, path, sizeof(g_LogPath) - 1);
    g_LogPath[sizeof(g_LogPath) - 1] = '\0';
}

void Log(const std::string& msg) {
    if (strlen(g_LogPath) == 0) return;
    FILE* f = fopen(g_LogPath, "a");
    if (f) { fprintf(f, "%s\n", msg.c_str()); fclose(f); }
}

mach_port_t get_task_port(const char* process_name) {
    Log("[*] Buscando processo: " + std::string(process_name));
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t size = 0;
    if (sysctl(mib, 4, NULL, &size, NULL, 0) < 0) return MACH_PORT_NULL;

    struct kinfo_proc* info = (struct kinfo_proc*)malloc(size);
    if (!info) return MACH_PORT_NULL;

    if (sysctl(mib, 4, info, &size, NULL, 0) < 0) { free(info); return MACH_PORT_NULL; }

    pid_t pid = -1;
    int count = (int)(size / sizeof(struct kinfo_proc));
    for (int i = 0; i < count; i++) {
        if (strstr(info[i].kp_proc.p_comm, process_name) != NULL) {
            pid = info[i].kp_proc.p_pid;
            Log("[+] Processo encontrado! PID: " + std::to_string(pid));
            break;
        }
    }
    free(info);
    if (pid == -1) return MACH_PORT_NULL;

    mach_port_t task = MACH_PORT_NULL;
    if (task_for_pid(mach_task_self(), pid, &task) != KERN_SUCCESS) return MACH_PORT_NULL;
    Log("[+] task_for_pid SUCESSO!");
    return task;
}

uintptr_t FindUnityBase() {
    mach_port_t tp = GetTaskPort();
    if (tp == MACH_PORT_NULL) return 0;

    Log("[*] Buscando UnityBase...");
    vm_address_t address = 0;
    vm_size_t size = 0;
    natural_t depth = 0;
    struct vm_region_submap_info_64 info;
    mach_msg_type_number_t count;

    while (true) {
        count = VM_REGION_SUBMAP_INFO_COUNT_64;
        kern_return_t kr = vm_region_recurse_64(tp, &address, &size, &depth,
                                                (vm_region_recurse_info_t)&info, &count);
        if (kr != KERN_SUCCESS) break;

        if (info.is_submap) { depth++; }
        else if (info.protection & VM_PROT_READ) {
            struct mach_header_64 header;
            vm_size_t readSize = sizeof(header);
            if (vm_read_overwrite(tp, address, readSize,
                                  (vm_address_t)&header, &readSize) == KERN_SUCCESS
                && readSize == sizeof(header) && header.magic == MH_MAGIC_64) {

                vm_size_t cmdsSize = header.sizeofcmds;
                if (cmdsSize > 0 && cmdsSize < 0x10000) {
                    char* cmds = (char*)malloc(cmdsSize);
                    if (cmds && vm_read_overwrite(tp, address + sizeof(header), cmdsSize,
                                                  (vm_address_t)cmds, &cmdsSize) == KERN_SUCCESS) {
                        char* ptr = cmds;
                        bool found = false; // <-- AQUI: Declarado fora do loop pra ter escopo
                        for (uint32_t i = 0; i < header.ncmds && !found; i++) {
                            if (ptr + sizeof(struct load_command) > cmds + cmdsSize) break;
                            struct load_command* lc = (struct load_command*)ptr;
                            if (lc->cmdsize == 0 || ptr + lc->cmdsize > cmds + cmdsSize) break;
                            if (lc->cmd == LC_ID_DYLIB) {
                                struct dylib_command* dy = (struct dylib_command*)lc;
                                char* name = (char*)lc + dy->dylib.name.offset;
                                if (name >= (char*)lc && name < (char*)lc + lc->cmdsize
                                    && strstr(name, "UnityFramework")) { found = true; }
                            }
                            ptr += lc->cmdsize;
                        }
                        free(cmds);
                        if (found) {
                            Log("[+] UnityFramework (self-id) em: 0x" + std::to_string(address));
                            return address;
                        }
                    } else if (cmds) {
                        free(cmds);
                    }
                }
            }
        }
        address += size;
    }
    Log("[-] UnityFramework nao encontrado.");
    return 0;
}

uintptr_t FindStaticMatchGame() {
    mach_port_t tp = GetTaskPort();
    uintptr_t ub = GetUnityBase();
    if (tp == MACH_PORT_NULL || ub == 0) return 0;

    uintptr_t cachedFacade = GetGameFacadeStatic();
    if (cachedFacade != 0) {
        return ReadMemory<uintptr_t>(cachedFacade + 0x8);
    }

    Log("[*] Pattern scan 1/2: string \"GameFacade\"...");
    uintptr_t string_addr = 0;
    {
        vm_address_t address = 0x100000000;
        vm_size_t size = 0;
        natural_t depth = 0;
        struct vm_region_submap_info_64 info;
        mach_msg_type_number_t count;

        while (true) {
            count = VM_REGION_SUBMAP_INFO_COUNT_64;
            if (vm_region_recurse_64(tp, &address, &size, &depth,
                                     (vm_region_recurse_info_t)&info, &count) != KERN_SUCCESS) break;
            if (info.is_submap) { depth++; }
            else if ((info.protection & VM_PROT_READ) == VM_PROT_READ) {
                const vm_size_t CHUNK = 4 * 1024 * 1024;
                char* buf = (char*)malloc(CHUNK);
                if (buf) {
                    for (vm_address_t chunk = address; chunk < address + size; chunk += CHUNK) {
                        vm_size_t want = (address + size - chunk > CHUNK) ? CHUNK : (address + size - chunk);
                        vm_size_t got  = want;
                        if (vm_read_overwrite(tp, chunk, want, (vm_address_t)buf, &got) == KERN_SUCCESS && got > 15) {
                            for (vm_size_t i = 0; i < got - 15; i++) {
                                if (memcmp(buf + i, "GameFacade", 10) == 0 && buf[i + 10] == '\0') {
                                    string_addr = chunk + i;
                                    break;
                                }
                            }
                        }
                        if (string_addr) break;
                    }
                    free(buf);
                }
            }
            if (string_addr) break;
            address += size;
        }
    }
    if (string_addr == 0) { Log("[-] GameFacade nao encontrada."); return 0; }
    Log("[+] GameFacade string: 0x" + std::to_string(string_addr));

    Log("[*] Pattern scan 2/2: Il2CppClass...");
    uintptr_t class_ptr = 0;
    uintptr_t static_fields = 0;
    {
        vm_address_t address = 0x100000000;
        vm_size_t size = 0;
        natural_t depth = 0;
        struct vm_region_submap_info_64 info;
        mach_msg_type_number_t count;

        while (true) {
            count = VM_REGION_SUBMAP_INFO_COUNT_64;
            if (vm_region_recurse_64(tp, &address, &size, &depth,
                                     (vm_region_recurse_info_t)&info, &count) != KERN_SUCCESS) break;
            if (info.is_submap) { depth++; }
            else if ((info.protection & VM_PROT_READ) == VM_PROT_READ) {
                const vm_size_t CHUNK = 4 * 1024 * 1024;
                uintptr_t* buf = (uintptr_t*)malloc(CHUNK);
                if (buf) {
                    const uintptr_t name_offsets[]   = {0x10, 0x08, 0x00};
                    const uintptr_t static_offsets[] = {0xB8, 0xC0, 0xA8, 0xB0, 0x90, 0x98};

                    for (vm_address_t chunk = address; chunk < address + size; chunk += CHUNK) {
                        vm_size_t want = (address + size - chunk > CHUNK) ? CHUNK : (address + size - chunk);
                        vm_size_t got  = want;
                        if (vm_read_overwrite(tp, chunk, want, (vm_address_t)buf, &got) != KERN_SUCCESS) continue;

                        for (size_t i = 0; i < got / sizeof(uintptr_t) && !class_ptr; i++) {
                            if (buf[i] != string_addr) continue;
                            uintptr_t ptr_addr = chunk + i * sizeof(uintptr_t);
                            for (uintptr_t name_off : name_offsets) {
                                if (ptr_addr < name_off) continue;
                                uintptr_t cand = ptr_addr - name_off;
                                if (cand < chunk) continue;
                                uintptr_t image = ReadMemory<uintptr_t>(cand + 0x00);
                                if (image < 0x100000000 || image > 0x1000000000000) continue;
                                for (uintptr_t sf_off : static_offsets) {
                                    uintptr_t sf = ReadMemory<uintptr_t>(cand + sf_off);
                                    if (sf > 0x100000000 && sf < 0x1000000000000) {
                                        class_ptr = cand;
                                        static_fields = sf;
                                        Log("[+] Il2CppClass name_off=0x" + std::to_string(name_off)
                                            + " sf_off=0x" + std::to_string(sf_off));
                                        break;
                                    }
                                }
                                if (class_ptr) break;
                            }
                        }
                        if (class_ptr) break;
                    }
                    free(buf);
                }
            }
            if (class_ptr) break;
            address += size;
        }
    }
    if (class_ptr == 0) { Log("[-] Il2CppClass nao validada."); return 0; }

    SetGameFacadeStatic(static_fields);
    uintptr_t matchGame = ReadMemory<uintptr_t>(static_fields + 0x8);
    if (matchGame == 0) { Log("[-] MatchGame NULL (fora de partida)."); return 0; }
    Log("[+] MatchGame: 0x" + std::to_string(matchGame));
    return matchGame;
}
