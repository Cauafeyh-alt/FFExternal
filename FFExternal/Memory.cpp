#include <mach/mach.h>
#include <libproc.h>
#include <string.h>
#include <stdio.h>

mach_port_t g_TaskPort = MACH_PORT_NULL;
uintptr_t g_UnityBase = 0;

mach_port_t get_task_port(const char* process_name) {
    pid_t pid = -1;
    int proc_count = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    pid_t pids[proc_count];
    proc_listpids(PROC_ALL_PIDS, 0, pids, sizeof(pids));

    for (int i = 0; i < proc_count; i++) {
        char name[256];
        proc_name(pids[i], name, sizeof(name));
        if (strcmp(name, process_name) == 0) {
            pid = pids[i];
            break;
        }
    }
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