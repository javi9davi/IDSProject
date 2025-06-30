#ifndef FILEMONITOR_H
#define FILEMONITOR_H

#include <string>
#include <unordered_map>
#include <utility>
#include <libvirt/libvirt.h>

#include "../../core/config.h"

class FileMonitor {
public:
    explicit FileMonitor(Config  config, std::string vmName, virConnectPtr conn)
        : config(std::move(config)), vmName(std::move(vmName)), conn(conn) {}

    std::string calculateFileHash() const;

    void storeFileHash() const;

    bool hasFileChanged();

    bool isHashStored(const std::string &file_path);

    bool requiresInitialization() const;

    void initializeVMHashes();

private:
    Config config;
    std::string vmName;
    virConnectPtr conn;
    std::unordered_map<std::string, std::string> file_hashes;

    void loadStoredHashes();

    static std::string readFileFromVM(virDomainPtr dom, const std::string& guestPath);
};

#endif // FILEMONITOR_H
