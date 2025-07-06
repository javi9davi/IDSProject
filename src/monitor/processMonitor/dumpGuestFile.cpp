#include "processMonitor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <filesystem>
#include <utility>
#include <libvirt/libvirt-qemu.h>
#include <libvirt/libvirt.h>
#include <nlohmann/json.hpp>
#include <openssl/sha.h>
#include <iomanip>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <chrono>
#include "config.h"


std::string ProcessMonitor::decodeBase64(const std::string& b64) {
    BIO* bmem = BIO_new_mem_buf(b64.data(), b64.size());
    BIO* b64b = BIO_new(BIO_f_base64());
    bmem    = BIO_push(b64b, bmem);
    BIO_set_flags(bmem, BIO_FLAGS_BASE64_NO_NL);
    std::vector<unsigned char> buf(b64.size());
    const int len = BIO_read(bmem, buf.data(), buf.size());
    BIO_free_all(bmem);
    return std::string(reinterpret_cast<char*>(buf.data()), len);
}

bool ProcessMonitor::dumpGuestFile(const std::string& guestPath, std::string& localPath) const {
    auto dom = getDomain();
    if (!dom) {
        std::cerr << "[ERROR] VM no encontrada: " << vmName << "\n";
        return false;
    }

    // 1) guest-file-open
    json openCmd = {
        {"execute","guest-file-open"},
        {"arguments", {
            {"path", guestPath},
            {"mode", "rb"}
        }}
    };
    char* openRes = virDomainQemuAgentCommand(dom.get(), openCmd.dump().c_str(), 10, 0);
    if (!openRes) {
        std::cerr << "[ERROR] guest-file-open falló para: " << guestPath << "\n";
        return false;
    }
    std::cout << openRes << "\n";

    json parsed;
    try {
        parsed = json::parse(openRes);
    } catch (const std::exception &e) {
        free(openRes);
        std::cerr << "[ERROR] JSON parse guest-exec: " << e.what() << "\n";
        return false;
    }
    free(openRes);

    if (!parsed.contains("return") || !parsed["return"].is_number_integer()) {
        std::cerr << "[ERROR] guest-file-open no devolvió un handle válido\n";
        return false;
    }
    int handle = parsed["return"].get<int>();
    if (handle < 0) {
        std::cerr << "[ERROR] handle inválido para: " << guestPath << "\n";
        return false;
    }

    // 2) leemos en bloques hasta EOF
    std::string allData;
    constexpr size_t chunkSize = 64 * 1024;
    while (true) {
        json readCmd = {
            {"execute","guest-file-read"},
            {"arguments", {
                {"handle", handle},
                {"count", chunkSize}
            }}
        };
        char* readRes = virDomainQemuAgentCommand(dom.get(), readCmd.dump().c_str(), 10, 0);
        if (!readRes) {
            std::cerr << "[ERROR] guest-file-read falló para handle=" << handle << "\n";
            break;
        }
        json jRead = json::parse(readRes)["return"];
        free(readRes);

        std::string b64 = jRead.value("buf-b64", "");
        if (b64.empty()) {
            // EOF
            break;
        }
        allData += decodeBase64(b64);
        if (!jRead.value("eof", false)) {
            // seguimos leyendo
            continue;
        }
        break;
    }

    // 3) guest-file-close
    {
        json closeCmd = {
            {"execute","guest-file-close"},
            {"arguments", {{"handle", handle}}}
        };
        if (char* closeRes = virDomainQemuAgentCommand(dom.get(), closeCmd.dump().c_str(), 10, 0)) free(closeRes);
    }

    // 4) volcamos a fichero local
    // construimos nombre: snapshots/<vmName>_<archivo>_<ts>.bin
    std::filesystem::path dir = "/tmp/IDS-TFG/snapshots";
    std::filesystem::create_directories(dir);
    auto filename = std::filesystem::path(guestPath).filename().string();
    auto ts = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::ostringstream oss;
    oss << dir.string() << "/" << vmName << "_" << filename << "_" << ts << ".bin";
    localPath = oss.str();

    std::ofstream out(localPath, std::ios::binary);
    if (!out) {
        std::cerr << "[ERROR] No se pudo crear fichero local: " << localPath << "\n";
        return false;
    }
    out.write(allData.data(), allData.size());
    out.close();

    return true;
}
