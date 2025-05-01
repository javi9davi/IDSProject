#ifndef PROCESS_MONITOR_H
#define PROCESS_MONITOR_H

#include <string>
#include <unordered_map>
#include <libvirt/libvirt.h>
#include <libvmi/libvmi.h>

class ProcessMonitor {
public:
    ProcessMonitor(std::string vmName, const virConnectPtr &conn, const vmi_instance_t &vmi=nullptr);
    ~ProcessMonitor();

    bool fetchProcessList(std::unordered_map<int, std::string>& snapshot) const;
    bool hasProcessChanged();
    void storeProcessSnapshot();
    void loadStoredSnapshot();
    void printCurrentProcesses() const;
    void setVMI(const vmi_instance_t &vmi);

private:
    static std::string generateSnapshotPath(const std::string& vmName);

    std::string vmName;
    vmi_instance_t vmi;
    virConnectPtr conn;
    std::unordered_map<int, std::string> lastSnapshot;
};

#endif // PROCESS_MONITOR_H
