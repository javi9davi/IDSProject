#ifndef MONITORTHREAD_H
#define MONITORTHREAD_H

#include "MonitorThread.h"
#include "./fileMonitor/fileMonitor.h"
#include "./processMonitor/processMonitor.h"
#include "config.h"
#include <atomic>
#include <memory>
#include <string>
#include <libvirt/libvirt.h>

class MonitorThread {
public:
    MonitorThread(std::string vmName,
                 virConnectPtr conn,
                 std::shared_ptr<FileMonitor>    fileMonitor,
                 std::shared_ptr<ProcessMonitor> processMonitor,
                 const Config& config);

    struct GuestProcessInfo {
        int pid;
        std::string name;
        double cpu;
        std::string binaryPath;
    };

    void run() const;
    void stop();

private:
    std::string vmName;
    std::shared_ptr<FileMonitor> fileMonitor;
    std::shared_ptr<ProcessMonitor> processMonitor;
    virConnectPtr conn;
    const Config& config;
    std::atomic<bool> shouldContinue;
};

#endif // MONITORTHREAD_H
