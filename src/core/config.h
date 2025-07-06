// config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

class Config {
public:
    Config();
    Config(int update_interval,
           int monitoring_interval,
           int process_interval,
           bool monitor_files,
           bool monitor_processes,
           const std::vector<std::string>& files2watch,
           std::string  hash_path,
           std::string  guest_file_path);

    // Setters
    void setUpdateInterval(int interval);
    void setMonitoringInterval(int interval);
    void setProcessInterval(int interval);
    void setMonitorFiles(bool monitor);
    void setMonitorProcesses(bool monitor);
    void setFiles2Watch(const std::vector<std::string>& files);
    void setHashPath(const std::string& path);
    void setGuestFilePath(const std::string& path);


    // Getters
    int getUpdateInterval()     const;
    int getMonitoringInterval() const;
    int getProcessInterval()    const;
    bool isMonitorFiles()       const;
    bool isMonitorProcesses()   const;
    const std::vector<std::string>& getFiles2Watch() const;
    const std::string& getHashPath()      const;
    const std::string& getGuestFilePath() const;
    const std::string& getNetworkInterface() const { return networkInterface; }
    const std::string& getNetworkFilter()    const { return networkFilter; }

    // Carga desde JSON
    bool loadFromFile(const std::string& filename);

private:
    int update_interval{5};
    int monitoring_interval{10};
    int process_interval{10};
    bool monitor_files{false};
    bool monitor_processes{false};
    std::vector<std::string> files2watch;
    std::string hash_path;
    std::string guest_file_path;
    std::string networkFilter;
    std::string networkInterface{"red-central"};
};

#endif // CONFIG_H
