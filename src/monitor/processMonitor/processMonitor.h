#ifndef PROCESS_MONITOR_H
#define PROCESS_MONITOR_H

#include <memory>
#include <string>
#include <unordered_map>
#include <libvirt/libvirt.h>

class ProcessMonitor {
public:
    ProcessMonitor(std::string vmName, const virConnectPtr &conn);

    std::unique_ptr<virDomain, int(*)(virDomainPtr domain)> getDomain() const;
    bool fetchProcessList(std::unordered_map<int, std::string>& snapshot) const;
    bool waitForExecutionCompletion(virDomainPtr dom, int pid, std::string& output) const;
    void parseProcessList(const std::string& output, std::unordered_map<int, std::string>& snapshot) const;
    bool checkProcessesWithBazaarAPI();
    std::string generateSnapshotPath() const;

private:
    static std::string generateSnapshotPath(const std::string& vmName);

    std::string vmName;
    virConnectPtr conn;
    std::unordered_map<int, std::string> lastSnapshot;
};

#endif // PROCESS_MONITOR_H
