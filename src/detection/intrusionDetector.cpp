#include "intrusionDetector.h"
#include <iostream>
#include <utility>
#include <libvirt/libvirt.h>
#include "../monitor/monitor.h"
#include "../monitor/processMonitor/processMonitor.h"


class ProcessMonitor;

IntrusionDetector::IntrusionDetector(std::shared_ptr<ProcessMonitor> processMonitor, std::string  vmName, const virConnectPtr &conn)
    : processMonitor(std::move(processMonitor)), vmName(std::move(vmName)), conn(conn) {}


std::unordered_map<int, std::string> IntrusionDetector::getProcesses() const {
    std::unordered_map<int, std::string> processes;
    processMonitor->fetchProcessList(TODO);
    return processes;
}



void IntrusionDetector::detectHiddenProcesses() const {
    auto memoryProcesses = getProcesses();
    auto guestProcesses = getGuestProcesses();

    for (const auto& [pid, name] : memoryProcesses) {
        if (guestProcesses.find(pid) == guestProcesses.end()) {
            reportHiddenProcess(pid, name);
        }
    }
}

void IntrusionDetector::detectProcessAnomalies() const {
    auto memoryProcesses = getProcesses();
    for (const auto& [pid, name] : memoryProcesses) {
        if (name.empty() || name == "unknown" || pid <= 1) {
            reportAnomaly("Proceso anómalo detectado: PID " + std::to_string(pid) + ", Nombre: " + name);
        }
    }
}

std::unordered_map<int, std::string> IntrusionDetector::getGuestProcesses() {
    std::unordered_map<int, std::string> guestProcesses;

    // Aquí se debe usar el QEMU guest agent
    // consultar /proc desde guest agent
    // o montar imagen si el agente no está disponible

    std::cerr << "[WARN] Función getGuestProcesses() no implementada en producción.\n";
    return guestProcesses;
}

void IntrusionDetector::reportHiddenProcess(const int pid, const std::string& name) {
    std::cout << "[ALERTA] Proceso oculto detectado: PID=" << pid << ", Nombre=" << name << std::endl;
}

void IntrusionDetector::reportAnomaly(const std::string& message) {
    std::cout << "[ALERTA] Anomalía detectada: " << message << std::endl;
}
