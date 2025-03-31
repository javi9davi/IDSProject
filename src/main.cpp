#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <unordered_set>
#include "core/config.h"
#include "nlohmann/json.hpp"
#include "initializeTest/initialize.h"
#include "monitor/monitor.h"
#include "monitor/fileMonitor/fileMonitor.h"

class MonitorThread {
public:
    MonitorThread(std::string vmName, const virConnectPtr conn, FileMonitor& fileMonitor, const Config& config)
        : vmName(std::move(vmName)), conn(conn), fileMonitor(fileMonitor), config(config) {}

    void operator()() const {
        if (Monitor monitor(vmName, conn); !monitor.initialize()) {
            std::cerr << "Error al inicializar el monitor para la VM: " << vmName << std::endl;
            return;
        }

        std::cout << "Monitoreando la VM: " << vmName << std::endl;
            const std::string vmFilePath = "/home/javierdr/IDS-TFG/hashStore";

        if (config.getUpdateInterval() && !fileMonitor.isHashStored(vmFilePath)) {
            std::cout << "VM nueva detectada: " << vmName << ". Calculando hashes iniciales..." << std::endl;
            fileMonitor.initializeVMHashes(vmFilePath);
        }

        while (shouldContinue) {
            std::this_thread::sleep_for(std::chrono::seconds(config.getMonitoringInterval()));

            if (config.isMonitorFiles() && fileMonitor.hasFileChanged(vmFilePath)) {
                std::cout << "Cambio detectado en " << vmName << std::endl;
                fileMonitor.storeFileHash(vmFilePath);
            }

            if (config.isMonitorProcesses()) {
                std::cout << "Monitoreando procesos en la VM: " << vmName << std::endl;
                // Aquí se puede llamar a una función para monitorear procesos
            }
        }
    }

    void stop() {
        shouldContinue = false;
    }

private:
    std::string vmName;
    virConnectPtr conn;
    FileMonitor& fileMonitor;
    const Config& config;
    bool shouldContinue = true;
};

[[noreturn]] int main() {
    Config config;

    if (!config.loadFromFile("../config/config.json")) {
        std::cerr << "Error: No se pudo cargar la configuración." << std::endl;
        exit(1);
    }

    VMManager vmManager;
    FileMonitor fileMonitor(config);
    std::vector<std::thread> threads;
    std::unordered_set<size_t> monitoredVMs;
    std::mutex mtx;
    std::hash<std::string> hasher;

    while (true) {
        std::vector<std::string> vm_names = vmManager.get_vm_names();

        if (vm_names.empty()) {
            std::cout << "No hay MVs disponibles" << std::endl;
        }

        for (const auto& vm_name : vm_names) {
            size_t vm_hash = hasher(vm_name);
            if (monitoredVMs.find(vm_hash) == monitoredVMs.end()) {
                std::cout << "Inicializando el thread para la VM: " << vm_name << std::endl;
                threads.emplace_back(MonitorThread(vm_name, vmManager.get_connection(), fileMonitor, config));
                monitoredVMs.insert(vm_hash);
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(config.getUpdateInterval()));
    }
}
