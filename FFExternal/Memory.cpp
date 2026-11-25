#include <mach/mach.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

mach_port_t g_TaskPort = MACH_PORT_NULL;
uintptr_t g_UnityBase = 0;

mach_port_t get_task_port(const char* process_name) {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t size;
    
    // Pega o tamanho da lista de processos
    if (sysctl(mib, 4, NULL, &size, NULL, 0) < 0) return MACH_PORT_NULL;
    
    struct kinfo_proc *info = (struct kinfo_proc *)malloc(size);
    if (!info) return MACH_PORT_NULL;
    
    // Pega a lista de verdade
    if (sysctl(mib, 4, info, &size, NULL, 0) < 0) {
        free(info);
        return MACH_PORT_NULL;
    }

    pid_t pid = -1;
    int count = size / sizeof(struct kinfo_proc);
    
    for (int i = 0; i < count; i++) {
        // Compara o nome do processo (p_comm) com o nome que a gente passou
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

template <typename T>
T ReadMemory(uintptr_t address) {
    T data;
    mach_vm_size_t size = sizeof(T);
    mach_vm_address_t addr = (mach_vm_address_t)address;
    if (mach_vm_read_overwrite(g_TaskPort, addr, size, (mach_vm_address_t)&data, &size) != KERN_SUCCESS) {
        return T();
    }
    return data;
}
