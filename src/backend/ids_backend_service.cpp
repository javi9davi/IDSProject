#include "../backend/ids_backend_service.h"
#include <iostream>
#include <chrono>
#include <filesystem>

IDSBackendService::IDSBackendService(QObject* parent)
    : QObject(parent), running(true) {
    if (!config.loadFromFile("../config/config.json")) {
        std::cerr << "Error al cargar la configuración del IDS." << std::endl;
        std::exit(1);
    }

    std::filesystem::path currentPath = std::filesystem::current_path().parent_path();
    std::filesystem::path hashFolder = currentPath / "hashStore";
    config.setHashPath(hashFolder);
}

IDSBackendService::~IDSBackendService() {
    running = false;
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
}

void IDSBackendService::start() {
    threads.emplace_back(&IDSBackendService::monitoringLoop, this);
}

void IDSBackendService::monitoringLoop() {
    while (running) {
        std::vector<std::string> vm_names = vmManager.get_vm_names();

        for (const auto& vm_name : vm_names) {
            std::hash<std::string> hasher;
            size_t vm_hash = hasher(vm_name);

            if (monitoredVMs.find(vm_hash) == monitoredVMs.end()) {
                std::cout << "Iniciando monitoreo para: " << vm_name << std::endl;
                monitoredVMs.insert(vm_hash);

                threads.emplace_back([this, vm_name]() {
                    monitorVM(vm_name);
                });
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(config.getUpdateInterval()));
    }
}

void IDSBackendService::monitorVM(const std::string& vmName) {
    auto fileMonitorPtr = std::make_shared<FileMonitor>(config, vmName, vmManager.get_connection());

    Monitor monitor(vmName, vmManager.get_connection());
    if (!monitor.initialize()) {
        std::cerr << "Error al inicializar monitor para: " << vmName << std::endl;
        return;
    }

    bool needsInit = false;
    for (const auto& path : config.getFiles2Watch()) {
        if (!fileMonitorPtr->isHashStored(path)) {
            needsInit = true;
            break;
        }
    }

    if (needsInit) {
        fileMonitorPtr->initializeVMHashes();
    }

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(config.getMonitoringInterval()));

        if (config.isMonitorFiles() && fileMonitorPtr->hasFileChanged()) {
            fileMonitorPtr->storeFileHash();
            emit alertGenerated(QString::fromStdString(vmName),
                                "[ALERTA] Cambio en archivos detectado.");
        }

        if (config.isMonitorProcesses()) {
            // Lógica de monitoreo de procesos (opcional)
        }
    }
}
