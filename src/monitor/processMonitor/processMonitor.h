#ifndef PROCESS_MONITOR_H
#define PROCESS_MONITOR_H

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <libvirt/libvirt.h>
#include "SqliteHashDB.h"

class ProcessMonitor {
public:
    ProcessMonitor(std::string vmName, const virConnectPtr &conn);

    struct ProcInfo {
        int pid;
        std::string name;
        double cpu;
    };

    // Declaramos GuestProcessInfo **antes** de usarlo
    struct GuestProcessInfo {
        int pid;
        std::string name;
        double cpu;
        std::string binaryPath;  // Ruta completa dentro de la VM
    };

    // Ahora ya podemos usarlo en la firma
    std::vector<GuestProcessInfo> listGuestProcesses() const;

    std::unique_ptr<virDomain, int(*)(virDomainPtr)> getDomain() const;
    bool fetchProcessList(std::vector<ProcInfo>& procs) const;
    static bool waitForExecutionCompletion(virDomainPtr dom, int pid, std::string& output);
    static void parseProcessList(const std::string& output, std::unordered_map<int, std::string>& snapshot);
    bool dumpProcessBinary(int pid, std::string& outLocalPath) const;
    bool dumpGuestFile(const std::string& guestPath, std::string& localPath) const;
    bool checkGuestFileWithBazaarAPI(const std::string& guestFilePath) const;
    static std::string decodeBase64(const std::string& b64);

private:
    std::string vmName;
    virConnectPtr conn;
    std::unordered_map<int, std::string> lastSnapshot;
};

#endif // PROCESS_MONITOR_H
