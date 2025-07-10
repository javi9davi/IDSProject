#include <iostream>
#include <thread>
#include <utility>
#include "MonitorThread.h"

#include <regex>

#include "processMonitor/processMonitor.h"
#include "monitor/monitor.h"
#include "vm_utils.h"
#include "networkMonitor/networkMonitor.h"

MonitorThread::MonitorThread(std::string vmName, const virConnectPtr conn,
                             std::shared_ptr<FileMonitor> fileMonitor,
                             std::shared_ptr<ProcessMonitor> processMonitor,
                             const Config& config)
    : vmName(std::move(vmName)), fileMonitor(std::move(fileMonitor)), processMonitor(std::move(processMonitor)),
      conn(conn), config(config),
      shouldContinue(true)
{
}

void MonitorThread::run() const
{
    Monitor monitor(vmName, conn);
    if (!monitor.initialize())
    {
        std::cerr << "Error initializing monitor for VM: " << vmName << std::endl;
        return;
    }

    std::cout << "Monitoring VM: " << vmName << std::endl;

    // Initial setup: files and agent
    if (config.isMonitorFiles() && fileMonitor->requiresInitialization())
    {
        std::cout << "New VM detected: " << vmName
            << ". Applying guest-agent channel and network interface..." << std::endl;

        ensureBridgeExists();
        configureNewVM(vmName, conn);

        std::cout << "Calculating initial hashes..." << std::endl;
        fileMonitor->initializeVMHashes();
    }

    // Launch file monitoring thread
    std::thread fileThread([&]()
    {
        while (shouldContinue)
        {
            std::this_thread::sleep_for(std::chrono::seconds(config.getMonitoringInterval()));
            if (config.isMonitorFiles() && fileMonitor->hasFileChanged())
            {
                std::cout << "[ALERT] File change detected in VM: " << vmName << std::endl;
                fileMonitor->storeFileHash();
            }
        }
    });

    std::thread processThread([&]()
    {
        while (shouldContinue)
        {
            std::this_thread::sleep_for(std::chrono::seconds(config.getProcessInterval()));

            if (!config.isMonitorProcesses())
                continue;

            // Obtener todos los procesos activos de la VM
            auto processes = processMonitor->listGuestProcesses(); // Devuelve vector<GuestProcessInfo>

            for (const auto& proc : processes)
            {
                std::string vmBinPath = proc.binaryPath; // Ruta dentro de la VM
                std::string basePath = config.getGuestFilePath();
                if (!basePath.empty() && basePath.back() == '/')
                    basePath.pop_back();

                std::string localBinPath = basePath + "/" + vmName + "_" + std::to_string(proc.pid) + ".bin";

                // Asegurar que el directorio local existe
                std::filesystem::path localPath(localBinPath);
                ensureDirectoryExists(localPath.parent_path().string());

                if (!processMonitor->dumpGuestFile(vmBinPath, localBinPath))
                {
                    std::cerr << "[ERROR] No se pudo extraer binario de proceso " << proc.pid
                        << " (" << vmBinPath << ") de VM " << vmName << std::endl;
                    continue;
                }

                // Comprobar contra MalwareBazaar
                if (processMonitor->checkGuestFileWithBazaarAPI(localBinPath))
                {
                    std::cerr << "[ALERT] Malicious binary in VM " << vmName
                        << " [PID: " << proc.pid << "] Path: " << vmBinPath << std::endl;
                }
                else
                {
                    std::cout << "[INFO] Proceso limpio [PID: " << proc.pid
                        << "] en VM " << vmName << std::endl;
                }

                // Borrar archivo local
                std::error_code ec;
                std::filesystem::remove(localBinPath, ec);
                if (ec)
                {
                    std::cerr << "[WARN] No se pudo borrar " << localBinPath
                        << ": " << ec.message() << std::endl;
                }
            }
        }
    });

    std::thread networkThread([&]() {
        const NetworkMonitor netMon(config, shouldContinue);
        netMon.monitorSyscalls(vmName, conn);
        netMon.run(vmName);
});

    while (shouldContinue)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Clean up
    if (fileThread.joinable()) fileThread.join();
    if (processThread.joinable()) processThread.join();
    if (networkThread.joinable()) networkThread.join();
}

void MonitorThread::stop()
{
    shouldContinue = false;
}
