// processMonitor.cpp
#include "processMonitor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <experimental/filesystem>
#include <utility>
#include <libvirt/libvirt-qemu.h>
#include <libvirt/libvirt.h>
#include <nlohmann/json.hpp>
#include <openssl/sha.h>
#include <iomanip>
#include "../../detection/signatures.h"

namespace fs = std::experimental::filesystem;
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

bool ProcessMonitor::fetchProcessList(std::unordered_map<int, std::string>& snapshot) const {
    snapshot.clear();
    auto dom = getDomain();
    if (!dom) {
        std::cerr << "[ERROR] VM not found: " << vmName << std::endl;
        return false;
    }

    const char *cmd = R"({
        "execute": "guest-exec",
        "arguments": {
            "path": "/bin/ps",
            "arg": ["-e", "-o", "pid=,comm="],
            "capture-output": true
        }
    })";

    char *result = virDomainQemuAgentCommand(dom.get(), cmd, 10, 0);
    if (!result) {
        std::cerr << "[ERROR] No response from guest-exec for VM: " << vmName << std::endl;
        return false;
    }

    json root;
    try {
        root = json::parse(result);
        free(result);
    } catch (const json::exception &e) {
        std::cerr << "[ERROR] JSON parse error: " << e.what() << std::endl;
        free(result);
        return false;
    }

    int guestPid = root["return"].value("pid", -1);
    if (guestPid == -1) {
        std::cerr << "[ERROR] Invalid PID returned" << std::endl;
        return false;
    }

    std::string output;
    if (!waitForExecutionCompletion(dom.get(), guestPid, output)) return false;

    parseProcessList(output, snapshot);

    return true;
}

bool ProcessMonitor::waitForExecutionCompletion(virDomainPtr dom, int pid, std::string& output) const {
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
            if (status.contains("out-data")) {
                output = decodeBase64(status["out-data"]);
                return true;
            }
            std::cerr << "[ERROR] No output captured" << std::endl;
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ProcessMonitor::parseProcessList(const std::string& output, std::unordered_map<int, std::string>& snapshot) const {
    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        std::istringstream ls(line);
        int pid;
        std::string name;
        if (ls >> pid >> name) {
            snapshot[pid] = name;
        }
    }
}

bool ProcessMonitor::checkProcessesWithBazaarAPI() {
    std::unordered_map<int, std::string> snapshot;
    if (!fetchProcessList(snapshot)) return false;

    bool foundMalicious = false;

    for (const auto& [pid, name] : snapshot) {
        std::string hash = computeSHA256(name);
        json api_response;

        if (queryMalwareBazaar(hash, api_response)) {
            if (api_response.contains("query_status") && api_response["query_status"] == "ok") {
                std::cerr << "[ALERTA] Malware detected: PID=" << pid << " NAME=" << name << " HASH=" << hash << std::endl;
                foundMalicious = true;
            } else {
                std::cout << "[INFO] Clean process: " << name << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    }
    return foundMalicious;
}

std::string ProcessMonitor::generateSnapshotPath() const {
    fs::path dir = "snapshots";
    fs::create_directories(dir);
    return (dir / (vmName + "_processes.snapshot")).string();
}
