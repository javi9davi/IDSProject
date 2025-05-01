#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <unordered_set>
#include <memory>

#include "core/config.h"
#include "nlohmann/json.hpp"
#include "initializeTest/initialize.h"
#include "monitor/monitor.h"
#include "monitor/fileMonitor/fileMonitor.h"
#include "monitor/processMonitor/processMonitor.h"
#include "/usr/include/libvirt/libvirt-qemu.h"
#include "libvmiApi/libvmiCalls.h"

class MonitorThread {
public:
    MonitorThread(std::string vmName, const virConnectPtr &conn, std::shared_ptr<FileMonitor> fileMonitor, std::shared_ptr<ProcessMonitor> processMonitor, const Config& config)
        : vmName(std::move(vmName)), fileMonitor(std::move(fileMonitor)), processMonitor(std::move(processMonitor)), conn(conn), config(config) {}

    void operator()() const {
        Monitor monitor(vmName, conn);

        if (!monitor.initialize()) {
            std::cerr << "Error al inicializar el monitor para la VM: " << vmName << std::endl;
            return;
        }

        std::cout << "Monitoreando la VM: " << vmName << std::endl;

        // Aquí obtenemos el VMI y lo configuramos en ProcessMonitor
        const vmi_instance_t &vmi = monitor.get_vmi();
        processMonitor->setVMI(vmi);

        const virDomainPtr &domain = virDomainLookupByName(conn, vmName.c_str());
        const std::string kernelName = monitor.getKernelName(domain);
        if (!generateProfileAndConfigureLibVMI(kernelName, vmName)) {
            std::cerr << "[ERROR] No se pudo generar el perfil para " << vmName << ". Abortando monitoreo." << std::endl;
            return;
        }

        bool needsInit = false;
        for (const auto& path : config.getFiles2Watch()) {
            if (!fileMonitor->isHashStored(path)) {
                needsInit = true;
                break;
            }
        }

        if (needsInit) {
            std::cout << "VM nueva detectada: " << vmName << ". Calculando hashes iniciales..." << std::endl;
            fileMonitor->initializeVMHashes();
        }

        processMonitor->storeProcessSnapshot();

        while (shouldContinue) {
            std::this_thread::sleep_for(std::chrono::seconds(config.getMonitoringInterval()));

            if (config.isMonitorFiles() && fileMonitor->hasFileChanged()) {
                std::cout << "[ALERTA] Cambio detectado en archivos de la VM: " << vmName << std::endl;
                fileMonitor->storeFileHash();
            }

            if (config.isMonitorProcesses()) {
                if (processMonitor->hasProcessChanged()) {
                    std::cout << "[ALERTA] Cambio detectado en procesos de la VM: " << vmName << std::endl;
                    processMonitor->storeProcessSnapshot();
                }
            }
        }
    }

    void stop() {
        shouldContinue = false;
    }

private:
    std::string vmName;
    std::shared_ptr<FileMonitor> fileMonitor;
    std::shared_ptr<ProcessMonitor> processMonitor;
    virConnectPtr conn;
    const Config& config;
    bool shouldContinue = true;
};

[[noreturn]] int main() {
    Config config;

    if (!config.loadFromFile("../config/config.json")) {
        std::cerr << "Error: No se pudo cargar la configuración." << std::endl;
        std::exit(1);
    }

    VMManager vmManager;
    std::vector<std::thread> threads;
    std::unordered_set<size_t> monitoredVMs;

    std::filesystem::path currentPath = std::filesystem::current_path().parent_path();
    std::filesystem::path projectFolder = currentPath / "hashStore";
    config.setHashPath(projectFolder);

    while (true) {
        std::vector<std::string> vm_names = vmManager.get_vm_names();

        if (vm_names.empty()) {
            std::cout << "No hay MVs disponibles" << std::endl;
        }

        for (const auto& vm_name : vm_names) {
            std::hash<std::string> hasher;

            if (std::size_t vm_hash = hasher(vm_name); monitoredVMs.find(vm_hash) == monitoredVMs.end()) {
                std::cout << "Inicializando el thread para la VM: " << vm_name << std::endl;

                auto fileMonitorPtr = std::make_shared<FileMonitor>(config, vm_name, vmManager.get_connection());
                auto processMonitorPtr = std::make_shared<ProcessMonitor>(vm_name, vmManager.get_connection(), nullptr);

                threads.emplace_back(MonitorThread(vm_name, vmManager.get_connection(), fileMonitorPtr, processMonitorPtr, config));
                monitoredVMs.insert(vm_hash);
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(config.getUpdateInterval()));
    }
}
