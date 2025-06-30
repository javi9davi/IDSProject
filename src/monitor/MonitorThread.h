#ifndef MONITORTHREAD_H
#define MONITORTHREAD_H

#include <atomic>
#include <memory>
#include <string>
#include <libvirt/libvirt.h>
#include "./fileMonitor/fileMonitor.h"
#include "./processMonitor/processMonitor.h"
#include "monitor.h"


class MonitorThread {
public:
    MonitorThread(const std::string& vmName, virConnectPtr conn,
                  std::shared_ptr<FileMonitor> fileMonitor,
                  std::shared_ptr<ProcessMonitor> processMonitor,
                  const Config& config);

    void run();
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
