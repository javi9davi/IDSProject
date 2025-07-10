#include "networkMonitor.h"
#include <iostream>
#include <regex>
#include <thread>
#include <cstdio>
#include <libvirt/libvirt.h>
#include <libvirt/libvirt-qemu.h>
#include <openssl/evp.h>

#include "utils.h"
#include "processMonitor/processMonitor.h"


NetworkMonitor::NetworkMonitor(const Config& config, const std::atomic<bool>& shouldRun)
    : config(config), shouldContinue(shouldRun) {}

void NetworkMonitor::run(const std::string& vmName) const
{
    const std::string iface = config.getNetworkInterface();
    const std::string cmd   = "sudo tcpdump -i " + iface + " -nn -l -q";

    const auto& filters = config.getNetworkFilters();

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "[ERROR][NETWORK] No se pudo lanzar tcpdump en " << iface << "\n";
        return;
    }

    const std::regex re(R"(IP\s+(\S+)\.(\d+)\s+>\s+(\S+)\.(\d+))");

    char* line = nullptr;
    size_t len = 0;

    while (shouldContinue && getline(&line, &len, pipe) != -1) {
        std::string s(line);
        std::smatch m;
        if (!std::regex_search(s, m, re)) continue;

        std::string srcIP = m[1].str();
        const int srcPort = std::stoi(m[2].str());
        std::string dstIP = m[3].str();
        const int dstPort = std::stoi(m[4].str());

        for (const auto& [protocol, ports, port_range, ip] : filters) {
            if (!protocol.empty()) {
                if (protocol == "tcp" && s.find("Flags [") == std::string::npos) continue;
                if (protocol == "udp" && s.find(": UDP,") == std::string::npos) continue;
            }

            if (!ports.empty()) {
                bool match_port = false;
                for (int p : ports) {
                    if (dstPort == p) { match_port = true; break; }
                }
                if (!match_port) continue;
            }

            if (port_range.first >= 0 && port_range.first < port_range.second) {
                if (dstPort < port_range.first || dstPort > port_range.second) continue;
            }

            if (!ip.empty()) {
                auto slash = ip.find('/');
                std::string net = ip.substr(0, slash);
                if (srcIP.rfind(net, 0) != 0 && dstIP.rfind(net, 0) != 0)
                    continue;
            }

            std::cerr << "[ALERT][NETWORK] Tráfico filtrado por regla: "
                      << "(proto=" << protocol << ", range=[" << port_range.first << "-" << port_range.second << "]"
                      << ", ip=" << ip << ")\n"
                      << "    " << srcIP << ":" << srcPort << " -> " << dstIP << ":" << dstPort << "\n";
            break;
        }
    }

    if (line) free(line);
    pclose(pipe);
}

void NetworkMonitor::monitorSyscalls(const std::string& vmName, virConnectPtr conn) const {
    auto dom = virDomainLookupByName(conn, vmName.c_str());
    if (!dom) {
        std::cerr << "[ERROR][SYSCALL] No se encontró la VM: " << vmName << "\n";
        return;
    }

    const char* straceCmd = R"({
        "execute": "guest-exec",
        "arguments": {
            "path": "/usr/bin/strace",
            "arg": ["-f", "-tt", "-e", "trace=openat,execve,connect", "timeout", "5", "ls", "/"],
            "capture-output": true
        }
    })";

    char* result = virDomainQemuAgentCommand(dom, straceCmd, 30, 0);
    if (!result) {
        std::cerr << "[ERROR][SYSCALL] No se pudo lanzar strace en la VM\n";
        virDomainFree(dom);
        return;
    }

    json j = json::parse(result);
    free(result);

    if (!j.contains("return") || !j["return"].contains("pid")) {
        std::cerr << "[ERROR][SYSCALL] Respuesta inválida al lanzar strace\n";
        virDomainFree(dom);
        return;
    }

    int guestPid = j["return"]["pid"];
    std::cout << "[INFO][SYSCALL] strace lanzado con PID invitado: " << guestPid << "\n";

    while (shouldContinue) {
        std::string pollCmd = R"({"execute":"guest-exec-status","arguments":{"pid":)" + std::to_string(guestPid) + "}}";
        char* pollResult = virDomainQemuAgentCommand(dom, pollCmd.c_str(), 10, 0);
        if (!pollResult) break;

        json status = json::parse(pollResult)["return"];
        free(pollResult);

        if (status["exited"].get<bool>()) {
            if (status.contains("out-data")) {
                const std::string decoded = decodeBase64(status["out-data"]);
                std::istringstream iss(decoded);
                std::string line;
                while (std::getline(iss, line)) {
                    if (line.find("openat") != std::string::npos &&
                        line.find("/etc/shadow") != std::string::npos) {
                        std::cerr << "[ALERT][SYSCALL] Acceso sospechoso a /etc/shadow: " << line << "\n";
                        //TODO añadir files2watch
                    } else {
                        std::cout << "[SYSCALL] " << line << "\n";
                    }
                }
            }
            break;
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    virDomainFree(dom);
}


