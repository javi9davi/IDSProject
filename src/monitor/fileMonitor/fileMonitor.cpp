#include "fileMonitor.h"
#include <iostream>
#include <openssl/evp.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <json-c/json.h>
#include "libvirt/libvirt-qemu.h"

std::string decodeBase64(const std::string& input) {
    BIO* bio = BIO_new_mem_buf(input.data(), input.length());
    BIO* b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Do not use newlines to flush buffer

    std::string output(input.length(), '\0');
    int decodedLen = BIO_read(bio, output.data(), input.length());
    output.resize(decodedLen);

    BIO_free_all(bio);
    return output;
}

void FileMonitor::loadStoredHashes() {
    std::string hashFilePath = config.getHashPath() + "/" + vmName + ".hash";
    std::ifstream hashFile(hashFilePath);
    if (!hashFile.is_open()) {
        std::cerr << "[WARN] No se pudo abrir el archivo de hashes: " << hashFilePath << std::endl;
        return;
    }

    std::string line;
    while (std::getline(hashFile, line)) {
        std::istringstream iss(line);
        std::string path, hash;

        size_t sepPos = line.find(": ");
        if (sepPos != std::string::npos) {
            path = line.substr(0, sepPos);
            hash = line.substr(sepPos + 2);  // +2 para saltar ": "
            file_hashes[path] = hash;
        }
    }

    std::cout << "[INFO] Hashes cargados desde disco para VM: " << vmName << std::endl;
}


std::string FileMonitor::readFileFromVM(virDomainPtr dom, const std::string& guestPath) {
    // 1. Comando guest-file-open
    std::string openCmd = R"({
        "execute": "guest-file-open",
        "arguments": {
            "path": ")" + guestPath + R"(",
            "mode": "r"
        }
    })";

    char* openResult = virDomainQemuAgentCommand(dom, openCmd.c_str(), -2, 0);
    if (!openResult) {
        std::cerr << "[ERROR] No se pudo abrir el archivo: " << guestPath << std::endl;
        return "";
    }

    std::string openStr(openResult);
    free(openResult);

    // 2. Parsear JSON para obtener el handle
    json_object* root = json_tokener_parse(openStr.c_str());
    if (!root) {
        std::cerr << "[ERROR] JSON inválido de guest-file-open" << std::endl;
        return "";
    }

    json_object* handleJson = json_object_object_get(root, "return");
    if (!handleJson || json_object_get_type(handleJson) != json_type_int) {
        std::cerr << "[ERROR] Handle no encontrado en JSON" << std::endl;
        json_object_put(root);
        return "";
    }

    int handle = json_object_get_int(handleJson);
    json_object_put(root);

    // 3. Comando guest-file-read
    std::string readCmd = R"({
        "execute": "guest-file-read",
        "arguments": {
            "handle": )" + std::to_string(handle) + R"(
        }
    })";

    char* readResult = virDomainQemuAgentCommand(dom, readCmd.c_str(), -2, 0);
    if (!readResult) {
        std::cerr << "[ERROR] No se pudo leer el archivo con handle: " << handle << std::endl;
        return "";
    }

    std::string readStr(readResult);
    free(readResult);

    // 4. Parsear JSON para obtener el contenido base64
    root = json_tokener_parse(readStr.c_str());
    if (!root) {
        std::cerr << "[ERROR] JSON inválido de guest-file-read" << std::endl;
        return "";
    }

    json_object* buf64Json = json_object_object_get(root, "return");
    if (!buf64Json || json_object_get_type(buf64Json) != json_type_object) {
        std::cerr << "[ERROR] Respuesta de lectura inesperada" << std::endl;
        json_object_put(root);
        return "";
    }

    json_object* base64Content = json_object_object_get(buf64Json, "buf-b64");
    if (!base64Content || json_object_get_type(base64Content) != json_type_string) {
        std::cerr << "[ERROR] No se encontró buf-b64 en la respuesta" << std::endl;
        json_object_put(root);
        return "";
    }

    std::string fileContentBase64 = json_object_get_string(base64Content);
    json_object_put(root);

    // 5. Cerrar el archivo
    std::string closeCmd = R"({
        "execute": "guest-file-close",
        "arguments": {
            "handle": )" + std::to_string(handle) + R"(
        }
    })";
    virDomainQemuAgentCommand(dom, closeCmd.c_str(), -2, 0); // No importa si falla

    return fileContentBase64;
}


std::string FileMonitor::calculateFileHash() const {
    virDomainPtr dom = virDomainLookupByName(conn, vmName.c_str());
    if (!dom) {
        std::cerr << "[ERROR] No se encontró la VM con nombre/hash: " << vmName << std::endl;
        return "";
    }

    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        std::cerr << "[ERROR] No se pudo crear el contexto de digest" << std::endl;
        virDomainFree(dom);
        return "";
    }

    if (EVP_DigestInit_ex(md_ctx, EVP_sha256(), nullptr) != 1) {
        std::cerr << "[ERROR] Falló EVP_DigestInit_ex" << std::endl;
        EVP_MD_CTX_free(md_ctx);
        virDomainFree(dom);
        return "";
    }

    for (const auto& rel_path : config.getFiles2Watch()) {
        std::string content = readFileFromVM(dom, rel_path);
        if (content.empty()) {
            std::cerr << "[WARN] Contenido vacío para archivo: " << rel_path << std::endl;
            continue;
        }

        if (EVP_DigestUpdate(md_ctx, content.data(), content.size()) != 1) {
            std::cerr << "[ERROR] Falló EVP_DigestUpdate con archivo: " << rel_path << std::endl;
            EVP_MD_CTX_free(md_ctx);
            virDomainFree(dom);
            return "";
        }
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;

    if (EVP_DigestFinal_ex(md_ctx, hash, &hash_len) != 1) {
        std::cerr << "[ERROR] Falló EVP_DigestFinal_ex" << std::endl;
        EVP_MD_CTX_free(md_ctx);
        virDomainFree(dom);
        return "";
    }

    EVP_MD_CTX_free(md_ctx);
    virDomainFree(dom);

    std::ostringstream oss;
    for (unsigned int i = 0; i < hash_len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return oss.str();
}

void FileMonitor::storeFileHash() const {
    virDomainPtr dom = virDomainLookupByName(conn, vmName.c_str());
    if (!dom) {
        std::cerr << "[ERROR] No se encontró la VM con nombre/hash: " << vmName << std::endl;
        return;
    }

    std::string hash_path = config.getHashPath() + "/" + vmName + ".hash";
    std::ofstream hash_file(hash_path);
    if (!hash_file.is_open()) {
        std::cerr << "[ERROR] No se pudo abrir el archivo para guardar el hash: " << hash_path << std::endl;
        virDomainFree(dom);
        return;
    }

    for (const auto& rel_path : config.getFiles2Watch()) {
        std::string content = readFileFromVM(dom, rel_path);
        if (content.empty()) {
            std::cerr << "[WARN] Contenido vacío para archivo: " << rel_path << std::endl;
            continue;
        }

        EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
        if (!md_ctx) {
            std::cerr << "[ERROR] No se pudo crear contexto para archivo: " << rel_path << std::endl;
            continue;
        }

        if (EVP_DigestInit_ex(md_ctx, EVP_sha256(), nullptr) != 1 ||
            EVP_DigestUpdate(md_ctx, content.data(), content.size()) != 1) {
            std::cerr << "[ERROR] Falló el cálculo del hash para: " << rel_path << std::endl;
            EVP_MD_CTX_free(md_ctx);
            continue;
        }

        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hash_len = 0;
        if (EVP_DigestFinal_ex(md_ctx, hash, &hash_len) != 1) {
            std::cerr << "[ERROR] Falló EVP_DigestFinal_ex para: " << rel_path << std::endl;
            EVP_MD_CTX_free(md_ctx);
            continue;
        }

        EVP_MD_CTX_free(md_ctx);

        std::ostringstream oss;
        for (unsigned int i = 0; i < hash_len; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }

        hash_file << rel_path << ": " << oss.str() << std::endl;
    }

    virDomainFree(dom);
    std::cout << "[INFO] Hashes individuales guardados en: " << hash_path << std::endl;
}

void FileMonitor::initializeVMHashes() {
    std::cout << "[INFO] Inicializando hashes para VM: " << vmName << std::endl;

    storeFileHash();      // Guardar hash por archivo
    loadStoredHashes();   // Cargar los hashes al mapa
}

bool FileMonitor::hasFileChanged() {
    virDomainPtr dom = virDomainLookupByName(conn, vmName.c_str());
    if (!dom) {
        std::cerr << "[ERROR] No se encontró la VM con nombre/hash: " << vmName << std::endl;
        return false;
    }

    bool cambiosDetectados = false;

    for (const auto& file_path : config.getFiles2Watch()) {
        std::string content = readFileFromVM(dom, file_path);
        if (content.empty()) {
            std::cerr << "[WARN] Contenido vacío o error al leer el archivo: " << file_path << std::endl;
            continue;
        }

        // Calcular hash del archivo
        EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
        if (!md_ctx) {
            std::cerr << "[ERROR] No se pudo crear contexto para: " << file_path << std::endl;
            continue;
        }

        if (EVP_DigestInit_ex(md_ctx, EVP_sha256(), nullptr) != 1 ||
            EVP_DigestUpdate(md_ctx, content.data(), content.size()) != 1) {
            std::cerr << "[ERROR] Fallo al calcular hash de: " << file_path << std::endl;
            EVP_MD_CTX_free(md_ctx);
            continue;
        }

        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hash_len = 0;
        if (EVP_DigestFinal_ex(md_ctx, hash, &hash_len) != 1) {
            std::cerr << "[ERROR] Error finalizando hash de: " << file_path << std::endl;
            EVP_MD_CTX_free(md_ctx);
            continue;
        }

        EVP_MD_CTX_free(md_ctx);

        std::ostringstream oss;
        for (unsigned int i = 0; i < hash_len; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }

        std::string new_hash = oss.str();

        auto it = file_hashes.find(file_path);
        if (it == file_hashes.end()) {
            std::cerr << "[INFO] No se encontró hash previo para: " << file_path << ". Se asumirá como modificado." << std::endl;
            cambiosDetectados = true;
        } else if (it->second != new_hash) {
            std::cout << "[INFO] Cambio detectado en archivo: " << file_path << std::endl;
            cambiosDetectados = true;
        }

        // Actualizar el hash almacenado en memoria
        file_hashes[file_path] = new_hash;
    }

    virDomainFree(dom);
    return cambiosDetectados;
}

bool FileMonitor::isHashStored(const std::string& file_path) {
    return file_hashes.find(file_path) != file_hashes.end();
}
