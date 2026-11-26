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

mach_port_t g_TaskPort = MACH_PORT_NULL;
uintptr_t g_GameCamera = 0;
uintptr_t g_StaticMatchGame = 0;

char g_LogPath[512] = "";

uintptr_t g_GameFacadeStatic = 0;

extern "C" void set_log_path(const char* path) {
    strncpy(g_LogPath, path, 511);
}

void Log(const std::string& msg) {
    if (strlen(g_LogPath) == 0) return;
    FILE* f = fopen(g_LogPath, "a");
    if (f) {
        fprintf(f, "%s\n", msg.c_str());
        fclose(f);
    }
}

mach_port_t get_task_port(const char* process_name) {
    Log("[*] Buscando processo: " + std::string(process_name));
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t size;
    if (sysctl(mib, 4, NULL, &size, NULL, 0) < 0) return MACH_PORT_NULL;

    struct kinfo_proc *info = (struct kinfo_proc *)malloc(size);
    if (!info) return MACH_PORT_NULL;

    if (sysctl(mib, 4, info, &size, NULL, 0) < 0) {
        free(info);
        return MACH_PORT_NULL;
    }

    pid_t pid = -1;
    int count = size / sizeof(struct kinfo_proc);
    for (int i = 0; i < count; i++) {
        if (strstr(info[i].kp_proc.p_comm, process_name) != NULL) {
            pid = info[i].kp_proc.p_pid;
            Log("[+] Processo encontrado! PID: " + std::to_string(pid));
            break;
        }
    }
    free(info);

    if (pid == -1) return MACH_PORT_NULL;

    mach_port_t task;
    if (task_for_pid(mach_task_self(), pid, &task) != KERN_SUCCESS) return MACH_PORT_NULL;
    Log("[+] task_for_pid SUCESSO!");
    return task;
}

uintptr_t GetUnityBase() {
    if (g_TaskPort == MACH_PORT_NULL) return 0;
    Log("[*] Buscando UnityBase...");

    vm_address_t address = 0;
    vm_size_t size = 0;
    natural_t depth = 0;
    struct vm_region_submap_info_64 info;
    mach_msg_type_number_t count;

    while (true) {
        count = VM_REGION_SUBMAP_INFO_COUNT_64;
        kern_return_t kr = vm_region_recurse_64(g_TaskPort, &address, &size, &depth, (vm_region_recurse_info_t)&info, &count);
        if (kr != KERN_SUCCESS) break;

        if (info.is_submap) {
            depth++;
        } else {
            if (info.protection & VM_PROT_READ) {
                struct mach_header_64 header;
                vm_size_t readSize = sizeof(header);
                if (vm_read_overwrite(g_TaskPort, address, readSize, (vm_address_t)&header, &readSize) == KERN_SUCCESS) {
                    if (header.magic == MH_MAGIC_64) {
                        vm_size_t cmdsSize = header.sizeofcmds;
                        if (cmdsSize > 0 && cmdsSize < 0x10000) {
                            char* cmds = (char*)malloc(cmdsSize);
                            if (cmds) {
                                if (vm_read_overwrite(g_TaskPort, address + sizeof(header), cmdsSize, (vm_address_t)cmds, &cmdsSize) == KERN_SUCCESS) {
                                    uint32_t ncmds = header.ncmds;
                                    char* ptr = cmds;
                                    bool found = false;
                                    for (uint32_t i = 0; i < ncmds; i++) {
                                        if (ptr + sizeof(struct load_command) > cmds + cmdsSize) break;
                                        struct load_command* lc = (struct load_command*)ptr;
                                        if (lc->cmdsize == 0 || ptr + lc->cmdsize > cmds + cmdsSize) break;
                                        if (lc->cmd == LC_ID_DYLIB || lc->cmd == LC_LOAD_DYLIB) {
                                            struct dylib_command* dylib = (struct dylib_command*)lc;
                                            char* name = (char*)lc + dylib->dylib.name.offset;
                                            if (name >= (char*)lc && name < (char*)lc + lc->cmdsize) {
                                                if (strstr(name, "UnityFramework")) { found = true; break; }
                                            }
                                        }
                                        ptr += lc->cmdsize;
                                    }
                                    free(cmds);
                                    if (found) {
                                        Log("[+] UnityFramework encontrado em: 0x" + std::to_string(address));
                                        return address;
                                    }
                                } else free(cmds);
                            }
                        }
                    }
                }
            }
        }
        address += size;
    }
    return 0;
}

uintptr_t FindStaticMatchGame() {
    if (g_TaskPort == MACH_PORT_NULL || g_UnityBase == 0) return 0;

    if (g_GameFacadeStatic != 0) {
        uintptr_t matchGame = ReadMemory<uintptr_t>(g_GameFacadeStatic + 0x8);
        return matchGame;
    }

    Log("[*] Iniciando Pattern Scan (GameFacade)...");

    vm_address_t address = g_UnityBase;
    vm_size_t size = 0;
    natural_t depth = 0;
    struct vm_region_submap_info_64 info;
    mach_msg_type_number_t count;

    uintptr_t string_addr = 0;
    int regiao_idx = 0;

    // 1. Busca a string "GameFacade" em chunks de 4MB
    while (true) {
        count = VM_REGION_SUBMAP_INFO_COUNT_64;
        if (vm_region_recurse_64(g_TaskPort, &address, &size, &depth, (vm_region_recurse_info_t)&info, &count) != KERN_SUCCESS) break;
        if (info.is_submap) { depth++; }
        else {
            if ((info.protection & VM_PROT_READ) == VM_PROT_READ) {
                vm_size_t chunk_size = 4 * 1024 * 1024;
                char* buffer = (char*)malloc(chunk_size);
                if (buffer) {
                    for (vm_address_t chunk_addr = address; chunk_addr < address + size; chunk_addr += chunk_size) {
                        vm_size_t readSize = (address + size - chunk_addr > chunk_size) ? chunk_size : (address + size - chunk_addr);
                        if (vm_read_overwrite(g_TaskPort, chunk_addr, readSize, (vm_address_t)buffer, &readSize) == KERN_SUCCESS) {
                            for (vm_size_t i = 0; i < readSize - 15; i++) {
                                if (memcmp(buffer + i, "GameFacade", 10) == 0 && buffer[i+10] == '\0') {
                                    string_addr = chunk_addr + i;
                                    break;
                                }
                                if (memcmp(buffer + i, "COW.GameFacade", 14) == 0 && buffer[i+14] == '\0') {
                                    string_addr = chunk_addr + i;
                                    break;
                                }
                            }
                        }
                        if (string_addr != 0) break;
                    }
                    free(buffer);
                }
                regiao_idx++;
                if (regiao_idx % 200 == 0) Log("[*] Regiao lida (Str): " + std::to_string(regiao_idx));
            }
        }
        if (string_addr != 0) break;
        address += size;
    }

    if (string_addr == 0) {
        Log("[-] String GameFacade nao encontrada.");
        return 0;
    }
    Log("[+] String encontrada em: 0x" + std::to_string(string_addr));

    // 2. Busca o ponteiro pra string (Il2CppClass) TAMBEM EM CHUNKS
    uintptr_t class_ptr = 0;
    address = g_UnityBase;
    regiao_idx = 0;
    Log("[*] Procurando Il2CppClass (Fase 2)...");

    while (true) {
        count = VM_REGION_SUBMAP_INFO_COUNT_64;
        if (vm_region_recurse_64(g_TaskPort, &address, &size, &depth, (vm_region_recurse_info_t)&info, &count) != KERN_SUCCESS) break;
        if (info.is_submap) { depth++; }
        else {
            if ((info.protection & VM_PROT_READ) == VM_PROT_READ) {
                vm_size_t chunk_size = 4 * 1024 * 1024;
                uintptr_t* buffer = (uintptr_t*)malloc(chunk_size);
                if (buffer) {
                    for (vm_address_t chunk_addr = address; chunk_addr < address + size; chunk_addr += chunk_size) {
                        vm_size_t readSize = (address + size - chunk_addr > chunk_size) ? chunk_size : (address + size - chunk_addr);
                        if (vm_read_overwrite(g_TaskPort, chunk_addr, readSize, (vm_address_t)buffer, &readSize) == KERN_SUCCESS) {
                            // Fase 2: varredura de offsets para name e static_fields
                            const uintptr_t name_offsets[]   = {0x10, 0x08, 0x00};
                            const uintptr_t static_offsets[] = {0xB8, 0xC0, 0xA8, 0xB0, 0x90, 0x98};

                            for (size_t i = 0; i < readSize / sizeof(uintptr_t); i++) {
                                if (buffer[i] != string_addr) continue;

                                uintptr_t ptr_addr = chunk_addr + (i * sizeof(uintptr_t));

                                for (uintptr_t name_off : name_offsets) {
                                    if (ptr_addr < name_off) continue;
                                    uintptr_t candidate = ptr_addr - name_off;
                                    if (candidate < chunk_addr) continue;

                                    uintptr_t image = ReadMemory<uintptr_t>(candidate + 0x00);
                                    if (image < 0x100000000 || image > 0x1000000000000) continue;

                                    for (uintptr_t sf_off : static_offsets) {
                                        uintptr_t sf = ReadMemory<uintptr_t>(candidate + sf_off);
                                        if (sf > 0x100000000 && sf < 0x1000000000000) {
                                            class_ptr = candidate;
                                            g_GameFacadeStatic = sf;
                                            Log("[+] Il2CppClass VALIDADO: name_off=0x" + std::to_string(name_off)
                                                + " sf_off=0x" + std::to_string(sf_off));
                                            break;
                                        }
                                    }
                                    if (class_ptr != 0) break;
                                }
                                if (class_ptr != 0) break;
                            }
                        }
                        if (class_ptr != 0) break;
                    }
                    free(buffer);
                }
                regiao_idx++;
                if (regiao_idx % 200 == 0) Log("[*] Regiao lida (Class): " + std::to_string(regiao_idx));
            }
        }
        if (class_ptr != 0) break;
        address += size;
    }

    if (class_ptr == 0) {
        Log("[-] Il2CppClass nao encontrado (validacao falhou).");
        return 0;
    }
    Log("[+] Il2CppClass VALIDADO em: 0x" + std::to_string(class_ptr));
    Log("[+] static_fields em: 0x" + std::to_string(g_GameFacadeStatic));

    uintptr_t matchGame = ReadMemory<uintptr_t>(g_GameFacadeStatic + 0x8);
    if (matchGame == 0) {
        Log("[-] MatchGame NULL (Fora de partida, aguardando...).");
        return 0;
    }

    Log("[+] GameFacade OK! MatchGame: 0x" + std::to_string(matchGame));
    return matchGame;
}
