#ifndef FILEMONITOR_H
#define FILEMONITOR_H

#include <string>
#include <unordered_map>
#include <openssl/evp.h>
#include <filesystem>

namespace fs = std::filesystem;

class FileMonitor {
public:
    // Calcula el hash de un archivo
    static std::string calculateFileHash(const std::string& file_path);

    // Guarda el hash de un archivo
    void storeFileHash(const std::string& file_path);

    // Verifica si un archivo ha cambiado
    bool hasFileChanged(const std::string& file_path);

private:
    std::unordered_map<std::string, std::string> file_hashes; // Almacén de hashes
};

#endif // FILEMONITOR_H
