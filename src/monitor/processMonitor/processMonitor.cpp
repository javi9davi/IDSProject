// processMonitor.cpp
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
#include "../../detection/signatures.h"
#include "../backend/SqliteHashDB.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

ProcessMonitor::ProcessMonitor(std::string vmName, const virConnectPtr &conn)
    : vmName(std::move(vmName)), conn(conn) {}

std::string decodeBase64(const std::string& encoded);

std::string computeSHA256(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);

    std::ostringstream oss;
    for (unsigned char c : hash) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }

    return oss.str();
}

std::unique_ptr<virDomain, decltype(&virDomainFree)> ProcessMonitor::getDomain() const {
    virDomainPtr dom = virDomainLookupByName(conn, vmName.c_str());
    return std::unique_ptr<virDomain, decltype(&virDomainFree)>(dom, virDomainFree);
}

bool ProcessMonitor::fetchProcessList(std::vector<ProcInfo>& procs) const {
    procs.clear();
    auto dom = getDomain();
    if (!dom) return false;

    // Pedimos PID, %CPU y comando
    const char *cmd = R"({
        "execute":"guest-exec",
        "arguments":{
            "path":"/bin/ps",
            "arg":["-eo","pid=,pcpu=,comm="],
            "capture-output":true
        }
    })";

    char *res = virDomainQemuAgentCommand(dom.get(), cmd, 10, 0);
    if (!res) return false;
    json j = json::parse(res)["return"];
    free(res);

    int guestPid = j.value("pid",-1);
    if (guestPid<0) return false;

    std::string output;
    if (!waitForExecutionCompletion(dom.get(), guestPid, output)) return false;

    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss,line)) {
        std::istringstream ls(line);
    if (ProcInfo pi = {0, "", 0.0}; ls >> pi.pid >> pi.cpu >> pi.name) {
        procs.push_back(pi);
        }
    }
    return true;
}

bool ProcessMonitor::waitForExecutionCompletion(const virDomainPtr dom, const int pid, std::string& output) {
    while (true) {
        std::ostringstream pollCmd;
        pollCmd << R"({"execute":"guest-exec-status","arguments":{"pid":)" << pid << "}}";

        char *result = virDomainQemuAgentCommand(dom, pollCmd.str().c_str(), 10, 0);
        if (!result) {
            std::cerr << "[ERROR] Failed guest-exec-status" << std::endl;
            return false;
        }

        json status = json::parse(result)["return"];
        free(result);

        if (status["exited"].get<bool>()) {
            output.clear();

            if (status.contains("out-data") && status["out-data"].is_string()) {
                const auto& base64out = status["out-data"];
                try {
                    output += decodeBase64(base64out.get<std::string>());
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] Base64 decode failed: " << e.what() << std::endl;
                }
            }

            if (status.contains("err-data") && status["err-data"].is_string()) {
                const auto& base64err = status["err-data"];
                try {
                    output += "\n[STDERR]: " + decodeBase64(base64err.get<std::string>());
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] Base64 decode failed (stderr): " << e.what() << std::endl;
                }
            }

            if (output.empty()) {
                std::cerr << "[ERROR] No output captured (empty after decoding)" << std::endl;
                return false;
            }

            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


void ProcessMonitor::parseProcessList(const std::string& output, std::unordered_map<int, std::string>& snapshot) {
    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        std::istringstream ls(line);
        int pid;
        if (std::string name; ls >> pid >> name) {
            snapshot[pid] = name;
        }
    }
}

bool ProcessMonitor::checkGuestFileWithBazaarAPI(const std::string& guestFilePath) const
{
    bool shouldRemove = false;
    std::string localPath;
    if (std::filesystem::exists(guestFilePath)) {
        // ¡es un archivo en el host! Lo usamos directamente.
        localPath = guestFilePath;
    } else {
        // no existe en el host → lo dump-eamos desde la VM
        if (!dumpGuestFile(guestFilePath, localPath)) {
            std::cerr << "[ERROR] No se pudo extraer el fichero: " << guestFilePath << "\n";
            return false;
        }
        shouldRemove = true;  // lo limpiaremos al final
    }
    // 2) Preparamos la base de datos de caché
    const fs::path projectRoot = fs::current_path();       // asume que corres el binario desde la raíz
    const fs::path cacheDir    = projectRoot / "cache";    // ./cache
    fs::create_directories(cacheDir);

    const fs::path dbFile      = cacheDir / (vmName + "_cache.db");
    const std::string dbPath   = dbFile.string();
    SqliteHashDB db{ dbPath };
    if (!db.initialize()) {
        std::cerr << "[ERROR] No se pudo inicializar la base de datos en "
                  << dbPath << "\n";
        if (shouldRemove) fs::remove(localPath);
        return false;
    }

    // 3) Leemos el fichero volcado y calculamos su SHA256
    std::ifstream in(localPath, std::ios::binary);
    if (!in) {
        std::cerr << "[ERROR] No se pudo abrir el fichero local: " << localPath << "\n";
        fs::remove(localPath);
        return false;
    }
    std::ostringstream oss;
    oss << in.rdbuf();
    const std::string binData = oss.str();
    const std::string hash = computeSHA256(binData);

    // 4) Comprobamos caché
    bool foundMalicious = false;
    const std::time_t now = std::time(nullptr);
    if (ProcessRecord rec; db.getRecord(hash, rec)) {
        if (rec.status == "malicious") {
            std::cerr << "[ALERTA][CACHÉ] Malware detectado en " << guestFilePath
                      << " HASH=" << hash << "\n";
            foundMalicious = true;
        } else {
            std::cout << "[INFO][CACHÉ] Archivo limpio: " << guestFilePath << "\n";
        }

    } else {
        // 5) No estaba en caché → consultamos la API
        bool isMalicious = false;
        json api_response;
        if (queryMalwareBazaar(hash, api_response) &&
            api_response.value("query_status","") == "ok") {
            isMalicious = true;
            std::cerr << "[ALERTA][API] Malware detectado en " << guestFilePath
                      << " HASH=" << hash << "\n";
            foundMalicious = true;
        } else {
            std::cout << "[INFO][API] Archivo limpio: " << guestFilePath << "\n";
        }

        // 6) Guardamos el resultado en caché
        if (!db.setRecord(hash, guestFilePath,
                          isMalicious ? "malicious" : "clean",
                          now)) {
            std::cerr << "[ERROR] No se pudo guardar en caché el hash: " << hash << "\n";
        }
    }

    if (shouldRemove) {
        fs::remove(localPath);
    }

    fs::remove(localPath);
    return foundMalicious;
}

std::vector<ProcessMonitor::GuestProcessInfo> ProcessMonitor::listGuestProcesses() const {
    // 1) Vector de salida
    std::vector<ProcessMonitor::GuestProcessInfo> result;

    // 2) Obtenemos primero la lista básica: pid, nombre y cpu
    std::vector<ProcInfo> procs;
    if (!fetchProcessList(procs)) {
        std::cerr << "[ERROR] No se pudo obtener la lista de procesos de la VM\n";
        return result;
    }

    // 3) Conexión al dominio QEMU/KVM
    auto dom = getDomain();
    if (!dom) return result;

    // 4) Para cada proceso, consultamos su ruta de ejecutable con readlink
    for (const auto& proc : procs) {
        // 4.1) Construimos el comando JSON para guest-exec → readlink /proc/<pid>/exe
        std::ostringstream cmd;
        cmd << R"({
            "execute": "guest-exec",
            "arguments": {
                "path": "/bin/readlink",
                "arg": ["/proc/)" << proc.pid << R"(/exe"],
                "capture-output": true
            }
        })";

        // 4.2) Enviamos el comando al agente QEMU y obtenemos un JSON con un pid temporal
        char* res = virDomainQemuAgentCommand(dom.get(), cmd.str().c_str(), 10, 0);
        if (!res) {
            std::cerr << "[WARN] No se pudo lanzar readlink para PID " << proc.pid << "\n";
            continue;
        }
        json j = json::parse(res)["return"];
        free(res);

        int guestPid = j.value("pid", -1);
        if (guestPid < 0) continue;

        // 4.3) Esperamos a que ese comando termine y recogemos su salida
        std::string output;
        if (!waitForExecutionCompletion(dom.get(), guestPid, output)) {
            std::cerr << "[WARN] No se pudo leer ruta del binario para PID " << proc.pid << "\n";
            continue;
        }

        // 4.4) Quitamos posibles saltos de línea
        output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());

        // 4.5) Añadimos al vector final
        result.push_back(GuestProcessInfo{
            proc.pid,
            proc.name,
            proc.cpu,
            output
        });
    }

    // 5) ¡Ordenamos de mayor a menor uso de CPU!
    std::sort(result.begin(), result.end(),
              [](auto &a, auto &b){ return a.cpu > b.cpu; });

    return result;
}
