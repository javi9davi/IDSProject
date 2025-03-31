#ifndef FILEMONITOR_H
#define FILEMONITOR_H

#include <string>
#include <unordered_map>
#include <filesystem>
#include "../../core/config.h"

namespace fs = std::filesystem;

class FileMonitor {
public:
    explicit FileMonitor(const Config& config) : config(config) {}

    // Calcula el hash de un archivo
    static std::string calculateFileHash(const std::string& file_path);

    // Guarda el hash de un archivo
    void storeFileHash(const std::string& file_path);

    // Verifica si un archivo ha cambiado
    bool hasFileChanged(const std::string& file_path);

    bool isHashStored(const std::string &file_path);

    void initializeVMHashes(const std::string &vm_directory);

private:
    const Config& config;
    std::unordered_map<std::string, std::string> file_hashes;
};

#endif // FILEMONITOR_H