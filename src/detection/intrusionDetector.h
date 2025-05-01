#ifndef INTRUSIONDETECTOR_H
#define INTRUSIONDETECTOR_H

#include <string>
#include <unordered_map>
#include <memory>
#include <libvirt/libvirt.h>

#include "../monitor/processMonitor/processMonitor.h"

class ProcessMonitor;

class IntrusionDetector {
public:
    IntrusionDetector(std::shared_ptr<ProcessMonitor> processMonitor, std::string vmName, const virConnectPtr &conn);

    [[nodiscard]] bool initialize() const;

    [[nodiscard]] std::unordered_map<int, std::string> getProcesses() const;

    void detectHiddenProcesses() const;
    void detectProcessAnomalies() const;

private:
    std::shared_ptr<ProcessMonitor> processMonitor;
    std::string vmName;
    virConnectPtr conn;

    static std::unordered_map<int, std::string> getGuestProcesses();
    static void reportHiddenProcess(int pid, const std::string& name);
    static void reportAnomaly(const std::string& message);
};

#endif // INTRUSIONDETECTOR_H
