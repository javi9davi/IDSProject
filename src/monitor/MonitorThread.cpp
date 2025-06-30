#include "MonitorThread.h"
#include <iostream>
#include <thread>

MonitorThread::MonitorThread(const std::string& vmName, virConnectPtr conn,
                             std::shared_ptr<FileMonitor> fileMonitor,
                             std::shared_ptr<ProcessMonitor> processMonitor,
                             const Config& config)
    : vmName(vmName), conn(conn), fileMonitor(std::move(fileMonitor)),
      processMonitor(std::move(processMonitor)), config(config),
      shouldContinue(true) {}

void MonitorThread::run() {
    Monitor monitor(vmName, conn);
    if (!monitor.initialize()) {
        std::cerr << "Error initializing monitor for VM: " << vmName << std::endl;
        return;
    }

    std::cout << "Monitoring VM: " << vmName << std::endl;

    if (config.isMonitorFiles() && fileMonitor->requiresInitialization()) {
        std::cout << "New VM detected: " << vmName << ". Calculating initial hashes..." << std::endl;
        fileMonitor->initializeVMHashes();
    }

    while (shouldContinue) {
        std::this_thread::sleep_for(std::chrono::seconds(config.getMonitoringInterval()));

        if (config.isMonitorFiles() && fileMonitor->hasFileChanged()) {
            std::cout << "[ALERT] File change detected in VM: " << vmName << std::endl;
            fileMonitor->storeFileHash();
        }
    }
}

void MonitorThread::stop() {
    shouldContinue = false;
}
