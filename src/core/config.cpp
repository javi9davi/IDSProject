// config.cpp
#include "config.h"
#include <iostream>
#include <fstream>
#include <utility>

Config::Config()= default;

Config::Config(const int update_interval,
               const int monitoring_interval,
               const int process_interval,
               const bool monitor_files,
               const bool monitor_processes,
               const std::vector<std::string>& files2watch,
               std::string  hash_path,
               std::string  guest_file_path)
    : update_interval(update_interval),
      monitoring_interval(monitoring_interval),
      process_interval(process_interval),
      monitor_files(monitor_files),
      monitor_processes(monitor_processes),
      files2watch(files2watch),
      hash_path(std::move(hash_path)),
      guest_file_path(std::move(guest_file_path))
{}

// Setters
void Config::setUpdateInterval(int i)      { update_interval     = i; }
void Config::setMonitoringInterval(int i)  { monitoring_interval = i; }
void Config::setProcessInterval(int i)     { process_interval    = i; }
void Config::setMonitorFiles(bool b)       { monitor_files       = b; }
void Config::setMonitorProcesses(bool b)   { monitor_processes   = b; }
void Config::setFiles2Watch(const std::vector<std::string>& v) { files2watch = v; }
void Config::setHashPath(const std::string& p)    { hash_path       = p; }
void Config::setGuestFilePath(const std::string& p){ guest_file_path = p; }

// Getters
int    Config::getUpdateInterval()     const { return update_interval; }
int    Config::getMonitoringInterval() const { return monitoring_interval; }
int    Config::getProcessInterval()    const { return process_interval; }
bool   Config::isMonitorFiles()        const { return monitor_files; }
bool   Config::isMonitorProcesses()    const { return monitor_processes; }
const std::vector<std::string>& Config::getFiles2Watch() const { return files2watch; }
const std::string& Config::getHashPath()      const { return hash_path; }
const std::string& Config::getGuestFilePath() const { return guest_file_path; }

bool Config::loadFromFile(const std::string& filename) {
    std::ifstream in(filename);
    if (!in.is_open()) {
        std::cerr << "[ERROR] No se pudo abrir config: " << filename << "\n";
        return false;
    }

    json j;
    in >> j;

    if (j.contains("General")) {
        if (auto it = j["General"].find("Update_Interval"); it != j["General"].end())
            update_interval = it->get<int>();
    }

    if (j.contains("Monitoring")) {
        auto& m = j["Monitoring"];
        if (m.contains("Monitor_Processes"))
            monitor_processes = m["Monitor_Processes"].get<bool>();
        if (m.contains("Process_Interval"))
            process_interval = m["Process_Interval"].get<int>();
        if (m.contains("Monitor_Files"))
            monitor_files = m["Monitor_Files"].get<bool>();
        if (m.contains("Files2Watch"))
            files2watch = m["Files2Watch"].get<std::vector<std::string>>();
        if (m.contains("Hash_Path"))
            hash_path = m["Hash_Path"].get<std::string>();
        if (m.contains("Guest_File_Path"))
            guest_file_path = m["Guest_File_Path"].get<std::string>();
        if (j.contains("Network") && j["Network"].is_object()) {
            auto& n = j["Network"];
            if (n.contains("Interface"))
                networkInterface = n["Interface"].get<std::string>();
            if (n.contains("Filter"))
                networkFilter    = n["Filter"].get<std::string>();
            }
    }

    return true;
}
