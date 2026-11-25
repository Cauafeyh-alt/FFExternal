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
uintptr_t g_UnityBase = 0;
uintptr_t g_GameCamera = 0;
uintptr_t g_StaticMatchGame = 0;

char g_LogPath[512] = "";

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
    Log("[*] Procurando GameFacade class...");

    vm_address_t address = g_UnityBase;
    vm_size_t size = 0;
    natural_t depth = 0;
    struct vm_region_submap_info_64 info;
    mach_msg_type_number_t count;

    uintptr_t string_addr = 0;

    // 1. Acha a string "GameFacade" na memoria
    while (true) {
        count = VM_REGION_SUBMAP_INFO_COUNT_64;
        if (vm_region_recurse_64(g_TaskPort, &address, &size, &depth, (vm_region_recurse_info_t)&info, &count) != KERN_SUCCESS) break;
        if (info.is_submap) { depth++; }
        else {
            if ((info.protection & VM_PROT_READ) == VM_PROT_READ) {
                char* buffer = (char*)malloc(size);
                if (buffer) {
                    vm_size_t readSize = size;
                    if (vm_read_overwrite(g_TaskPort, address, size, (vm_address_t)buffer, &readSize) == KERN_SUCCESS) {
                        for (vm_size_t i = 0; i < readSize - 11; i++) {
                            if (memcmp(buffer + i, "GameFacade", 10) == 0 && buffer[i+10] == '\0') {
                                string_addr = address + i;
                                break;
                            }
                        }
                    }
                    free(buffer);
                }
            }
        }
        if (string_addr != 0) break;
        address += size;
    }

    if (string_addr == 0) {
        Log("[-] String GameFacade nao encontrada.");
        return 0;
    }

    // 2. Acha o ponteiro que aponta pra string (Il2CppClass)
    uintptr_t class_ptr = 0;
    address = g_UnityBase;
    while (true) {
        count = VM_REGION_SUBMAP_INFO_COUNT_64;
        if (vm_region_recurse_64(g_TaskPort, &address, &size, &depth, (vm_region_recurse_info_t)&info, &count) != KERN_SUCCESS) break;
        if (info.is_submap) { depth++; }
        else {
            if ((info.protection & VM_PROT_READ) == VM_PROT_READ) {
                uintptr_t* buffer = (uintptr_t*)malloc(size);
                if (buffer) {
                    vm_size_t readSize = size;
                    if (vm_read_overwrite(g_TaskPort, address, size, (vm_address_t)buffer, &readSize) == KERN_SUCCESS) {
                        for (vm_size_t i = 0; i < readSize / sizeof(uintptr_t); i++) {
                            if (buffer[i] == string_addr) {
                                class_ptr = address + (i * sizeof(uintptr_t)) - 0x10; // Offset do nome no Il2CppClass
                                break;
                            }
                        }
                    }
                    free(buffer);
                }
            }
        }
        if (class_ptr != 0) break;
        address += size;
    }

    if (class_ptr == 0) {
        Log("[-] Il2CppClass do GameFacade nao encontrado.");
        return 0;
    }

    // 3. Le o static_fields (offset 0xB8 no iOS arm64)
    uintptr_t static_fields = ReadMemory<uintptr_t>(class_ptr + 0xB8);
    if (static_fields == 0) {
        Log("[-] static_fields NULL.");
        return 0;
    }

    // 4. Le o MatchGame (offset 0x8)
    uintptr_t matchGame = ReadMemory<uintptr_t>(static_fields + 0x8);
    Log("[+] GameFacade encontrado! MatchGame: 0x" + std::to_string(matchGame));
    return matchGame;
}
