// processMonitor.cpp
#include "processMonitor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <utility>
#include <libvmi/libvmi.h>

ProcessMonitor::ProcessMonitor(std::string  vmName, const virConnectPtr &conn, const vmi_instance_t &vmi)
    : vmName(std::move(vmName)), vmi(vmi), conn(conn) {}

ProcessMonitor::~ProcessMonitor() {
    if (vmi) {
        vmi_destroy(vmi);
    }
}

void ProcessMonitor::setVMI(const vmi_instance_t &vmi) {
    this->vmi = vmi;
}

bool ProcessMonitor::fetchProcessList(std::unordered_map<int, std::string>& snapshot) const {
    snapshot.clear();

    addr_t list_head = 0;
    addr_t current_process = 0;
    addr_t next_process = 0;

    unsigned long tasks_offset = 0, pid_offset = 0, name_offset = 0;

    if ((vmi_get_offset(vmi, "linux_tasks", &tasks_offset)) != VMI_SUCCESS) {
        std::cerr << "[ERROR] No se pudo obtener el offset de tasks" << std::endl;
        return false;
    }
    if ((vmi_get_offset(vmi, "linux_pid", &pid_offset)) != VMI_SUCCESS) {
        std::cerr << "[ERROR] No se pudo obtener el offset de pid" << std::endl;
        return false;
    }
    if ((vmi_get_offset(vmi, "linux_name", &name_offset)) != VMI_SUCCESS) {
        std::cerr << "[ERROR] No se pudo obtener el offset de name" << std::endl;
        return false;
    }

    if (vmi_translate_ksym2v(vmi, "init_task", &list_head) != VMI_SUCCESS) {
        std::cerr << "[ERROR] No se pudo traducir init_task." << std::endl;
        return false;
    }

    current_process = list_head;
    if (vmi_read_addr_va(vmi, current_process + tasks_offset, 0, &next_process) != VMI_SUCCESS)
        return false;

    do {
        const addr_t task_struct = current_process - tasks_offset;
        int pid = 0;
        char* name = vmi_read_str_va(vmi, task_struct + name_offset, 0);
        if (!name) {
            break;
        }
        snapshot[pid] = name;
        free(name);


        current_process = next_process;
        if (vmi_read_addr_va(vmi, current_process + tasks_offset, 0, &next_process) != VMI_SUCCESS)
            break;

    } while (current_process != list_head);

    return true;
}

bool ProcessMonitor::hasProcessChanged() {
    std::unordered_map<int, std::string> currentSnapshot;
    if (!fetchProcessList(currentSnapshot)) return false;

    if (currentSnapshot != lastSnapshot) {
        lastSnapshot = currentSnapshot;
        return true;
    }

    return false;
}

void ProcessMonitor::storeProcessSnapshot() {
    std::unordered_map<int, std::string> snapshot;
    if (!fetchProcessList(snapshot)) return;

    std::string path = generateSnapshotPath(vmName);
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "[ERROR] No se pudo abrir el archivo de snapshot: " << path << std::endl;
        return;
    }

    for (const auto& [pid, name] : snapshot) {
        out << pid << ":" << name << "\n";
    }

    lastSnapshot = snapshot;
    std::cout << "[INFO] Snapshot de procesos almacenado para VM: " << vmName << std::endl;
}

void ProcessMonitor::loadStoredSnapshot() {
    lastSnapshot.clear();
    std::string path = generateSnapshotPath(vmName);
    std::ifstream in(path);
    if (!in.is_open()) {
        std::cerr << "[WARN] No se encontró snapshot previo: " << path << std::endl;
        return;
    }

    std::string line;
    while (std::getline(in, line)) {
        std::istringstream iss(line);
        std::string pid_str, name;
        if (std::getline(iss, pid_str, ':') && std::getline(iss, name)) {
            int pid = std::stoi(pid_str);
            lastSnapshot[pid] = name;
        }
    }

    std::cout << "[INFO] Snapshot previo cargado para VM: " << vmName << std::endl;
}

void ProcessMonitor::printCurrentProcesses() const {
    std::unordered_map<int, std::string> snapshot;
    if (!fetchProcessList(snapshot)) return;

    std::cout << "\n[Procesos actuales en " << vmName << "]\n";
    for (const auto& [pid, name] : snapshot) {
        std::cout << "PID: " << pid << "\tNombre: " << name << std::endl;
    }
}

std::string ProcessMonitor::generateSnapshotPath(const std::string& vmName) {
    return "snapshots/" + vmName + "_processes.snapshot";
}