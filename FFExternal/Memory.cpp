#include <mach/mach.h>
#include <mach/vm_map.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <mach-o/loader.h>

mach_port_t g_TaskPort = MACH_PORT_NULL;
uintptr_t g_UnityBase = 0;
uintptr_t g_GameCamera = 0; // <--- DEFINIDO AQUI

mach_port_t get_task_port(const char* process_name) {
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
        if (strcmp(info[i].kp_proc.p_comm, process_name) == 0) {
            pid = info[i].kp_proc.p_pid;
            break;
        }
    }
    free(info);

    if (pid == -1) return MACH_PORT_NULL;

    mach_port_t task;
    if (task_for_pid(mach_task_self(), pid, &task) != KERN_SUCCESS) {
        return MACH_PORT_NULL;
    }
    return task;
}

uintptr_t GetUnityBase() {
    if (g_TaskPort == MACH_PORT_NULL) return 0;
    
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
            uint32_t magic = 0;
            vm_size_t readSize = sizeof(magic);
            if (vm_read_overwrite(g_TaskPort, address, readSize, (vm_address_t)&magic, &readSize) == KERN_SUCCESS) {
                if (magic == MH_MAGIC_64) {
                    char path[256];
                    if (vm_read_overwrite(g_TaskPort, address + 8, 256, (vm_address_t)path, &readSize) == KERN_SUCCESS) {
                        if (strstr(path, "UnityFramework")) {
                            return address;
                        }
                    }
                }
            }
        }
        address += size;
    }
    return 0;
}
