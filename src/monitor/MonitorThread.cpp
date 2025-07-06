#include <iostream>
#include <thread>
#include <utility>
#include "MonitorThread.h"

#include <regex>

#include "processMonitor/processMonitor.h"
#include "monitor/monitor.h"
#include "vm_utils.h"

MonitorThread::MonitorThread(std::string  vmName, const virConnectPtr conn,
                             std::shared_ptr<FileMonitor> fileMonitor,
                             std::shared_ptr<ProcessMonitor> processMonitor,
                             const Config& config)
    : vmName(std::move(vmName)), conn(conn), fileMonitor(std::move(fileMonitor)),
      processMonitor(std::move(processMonitor)), config(config),
      shouldContinue(true) {}

void MonitorThread::run() const
{
    Monitor monitor(vmName, conn);
    if (!monitor.initialize()) {
        std::cerr << "Error initializing monitor for VM: " << vmName << std::endl;
        return;
    }

    std::cout << "Monitoring VM: " << vmName << std::endl;

    // Initial setup: files and agent
    if (config.isMonitorFiles() && fileMonitor->requiresInitialization()) {
        std::cout << "New VM detected: " << vmName
                  << ". Applying guest-agent channel and network interface..." << std::endl;

        ensureBridgeExists();
        configureNewVM(vmName, conn);

        std::cout << "Calculating initial hashes..." << std::endl;
        fileMonitor->initializeVMHashes();
    }

    // Launch file monitoring thread
    std::thread fileThread([&]() {
        while (shouldContinue) {
            std::this_thread::sleep_for(std::chrono::seconds(config.getMonitoringInterval()));
            if (config.isMonitorFiles() && fileMonitor->hasFileChanged()) {
                std::cout << "[ALERT] File change detected in VM: " << vmName << std::endl;
                fileMonitor->storeFileHash();
            }
        }
    });

    std::thread processThread([&]() {
    while (shouldContinue) {
        std::this_thread::sleep_for(std::chrono::seconds(config.getProcessInterval()));

        if (!config.isMonitorProcesses())
            continue;

        // Obtener todos los procesos activos de la VM
            auto processes = processMonitor->listGuestProcesses();  // Devuelve vector<GuestProcessInfo>

        for (const auto& proc : processes) {
            std::string vmBinPath = proc.binaryPath;  // Ruta dentro de la VM
            std::string basePath = config.getGuestFilePath();
            if (!basePath.empty() && basePath.back() == '/')
                basePath.pop_back();

            std::string localBinPath = basePath + "/" + vmName + "_" + std::to_string(proc.pid) + ".bin";

            // Asegurar que el directorio local existe
            std::filesystem::path localPath(localBinPath);
            ensureDirectoryExists(localPath.parent_path().string());

            if (!processMonitor->dumpGuestFile(vmBinPath, localBinPath)) {
                std::cerr << "[ERROR] No se pudo extraer binario de proceso " << proc.pid
                          << " (" << vmBinPath << ") de VM " << vmName << std::endl;
                continue;
            }

            // Comprobar contra MalwareBazaar
            bool malicious = processMonitor->checkGuestFileWithBazaarAPI(localBinPath);
            if (malicious) {
                std::cerr << "[ALERT] Malicious binary in VM " << vmName
                          << " [PID: " << proc.pid << "] Path: " << vmBinPath << std::endl;
            } else {
                std::cout << "[INFO] Proceso limpio [PID: " << proc.pid
                          << "] en VM " << vmName << std::endl;
            }

            // Borrar archivo local
            std::error_code ec;
            std::filesystem::remove(localBinPath, ec);
            if (ec) {
                std::cerr << "[WARN] No se pudo borrar " << localBinPath
                          << ": " << ec.message() << std::endl;
            }
        }
    }
});

    std::thread networkThread([&](){
    const std::string iface = config.getBridgeInterface();  // p.ej. "virbr0"
    // -nn: no resolver nombres, -l: línea buffered, -q: salida breve
    const std::string cmd = "sudo tcpdump -i " + iface + " -nn -l -q";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "[ERROR][NETWORK] No se pudo lanzar tcpdump en " << iface << "\n";
        return;
    }

    char* line = nullptr;
    size_t len = 0;
    // expresión simple para extraer IPs y puertos:
    std::regex re(R"(IP\s+(\S+)\.(\d+)\s+>\s+(\S+)\.(\d+))");

    while (shouldContinue && getline(&line, &len, pipe) != -1) {
        std::string s(line);
        std::smatch m;
        if (std::regex_search(s, m, re)) {
            auto srcIP   = m[1].str();
            auto srcPort = std::stoi(m[2].str());
            auto dstIP   = m[3].str();
            auto dstPort = std::stoi(m[4].str());

            // criterio de “inusual”: aquí todo puerto >1024
            if (dstPort > 1024) {
                std::cerr << "[ALERT][NETWORK] Conexión inusual: "
                          << srcIP << ":" << srcPort
                          << " → " << dstIP << ":" << dstPort
                          << "\n";
            }
        }
    }
    if (line) free(line);
    pclose(pipe);
});

    // … al final de MonitorThread::run(), antes de hacer join a fileThread y processThread:
    if (networkThread.joinable()) networkThread.join();



    while (shouldContinue) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Clean up
    if (fileThread.joinable()) fileThread.join();
    if (processThread.joinable()) processThread.join();
}

void MonitorThread::stop() {
    shouldContinue = false;
}
