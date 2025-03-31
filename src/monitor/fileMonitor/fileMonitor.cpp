#include "fileMonitor.h"
#include <iostream>
#include <openssl/evp.h>
#include <fstream>
#include <sstream>
#include <iomanip>

std::string FileMonitor::calculateFileHash(const std::string& file_path) {
    if (!fs::exists(file_path)) {
        std::cerr << "Error: Archivo no encontrado - " << file_path << std::endl;
        return "";
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: No se pudo abrir el archivo - " << file_path << std::endl;
        return "";
    }

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        std::cerr << "Error: No se pudo crear el contexto de hash" << std::endl;
        return "";
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        std::cerr << "Error: No se pudo inicializar el digest" << std::endl;
        return "";
    }

    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
        if (EVP_DigestUpdate(mdctx, buffer, file.gcount()) != 1) {
            EVP_MD_CTX_free(mdctx);
            std::cerr << "Error: No se pudo actualizar el hash" << std::endl;
            return "";
        }
    }

    // Últimos bytes si quedaron
    if (EVP_DigestUpdate(mdctx, buffer, file.gcount()) != 1) {
        EVP_MD_CTX_free(mdctx);
        std::cerr << "Error: No se pudo actualizar el hash" << std::endl;
        return "";
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        std::cerr << "Error: No se pudo finalizar el hash" << std::endl;
        return "";
    }

    EVP_MD_CTX_free(mdctx);

    std::stringstream ss;
    for (unsigned int i = 0; i < hash_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return ss.str();
}

void FileMonitor::storeFileHash(const std::string& file_path) {
    if (const std::string hash = calculateFileHash(file_path); !hash.empty()) {
        file_hashes[file_path] = hash;
        std::cout << "Hash almacenado para " << file_path << ": " << hash << std::endl;
    }
}

bool FileMonitor::hasFileChanged(const std::string& file_path) {
    const std::string new_hash = calculateFileHash(file_path);
    if (file_hashes.find(file_path) == file_hashes.end()) {
        std::cerr << "Advertencia: No se ha almacenado previamente un hash para " << file_path << std::endl;
        return true;
    }
    return file_hashes[file_path] != new_hash;
}

bool FileMonitor::isHashStored(const std::string& file_path) {

    return file_hashes.find(file_path) != file_hashes.end();
}

void FileMonitor::initializeVMHashes(const std::string& vm_directory) {
    // Verificar si el directorio existe y es válido

    if (!fs::exists(vm_directory)) {
        std::cerr << "Error: El directorio no existe -> " << vm_directory << std::endl;
        return;
    }

    // Recorrer el directorio y calcular el hash de cada archivo
    for (const auto& entry : fs::directory_iterator(vm_directory)) {
        if (fs::is_regular_file(entry.path())) {
            std::cout << "Procesando archivo: " << entry.path() << std::endl;
            storeFileHash(entry.path().string());
        }
    }
}


