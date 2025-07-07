#ifndef NETWORK_MONITOR_H
#define NETWORK_MONITOR_H

#include <string>
#include <atomic>
#include <libvirt/libvirt.h>

#include "config.h"

class NetworkMonitor {
public:
    NetworkMonitor(const Config& config, const std::atomic<bool>& shouldRun);

    void run(const std::string& vmName) const;
    void monitorSyscalls(const std::string& vmName, virConnectPtr conn) const;

private:
    const Config& config;
    const std::atomic<bool>& shouldContinue;
};

#endif // NETWORK_MONITOR_H
