// config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

class Config {
public:
    static constexpr uint16_t PORT_UNSET = 0;

    // Estructura de filtros de red
    struct NetworkFilter {
        std::string protocol;             // "tcp" o "udp", o "" para cualquiera
        std::vector<int> ports;           // puertos exactos
        std::pair<uint16_t,uint16_t> port_range{PORT_UNSET, PORT_UNSET};    //rango de puertos
        std::string ip;                   // CIDR o prefijo, "" si no aplica
    };

    Config();
    Config(int update_interval,
           int monitoring_interval,
           int process_interval,
           bool monitor_files,
           bool monitor_processes,
           const std::vector<std::string>& files2watch,
           std::string hash_path,
           std::string guest_file_path,
           std::string network_interface,
           std::vector<NetworkFilter> network_filters);

    // Setters
    void setUpdateInterval(int interval);
    void setMonitoringInterval(int interval);
    void setProcessInterval(int interval);
    void setMonitorFiles(bool monitor);
    void setMonitorProcesses(bool monitor);
    void setFiles2Watch(const std::vector<std::string>& files);
    void setHashPath(const std::string& path);
    void setGuestFilePath(const std::string& path);
    void setNetworkInterface(const std::string& iface);
    void setNetworkFilters(const std::vector<NetworkFilter>& filters);

    // Getters
    [[nodiscard]] int getUpdateInterval()     const;
    [[nodiscard]] int getMonitoringInterval() const;
    [[nodiscard]] int getProcessInterval()    const;
    [[nodiscard]] bool isMonitorFiles()       const;
    [[nodiscard]] bool isMonitorProcesses()   const;
    [[nodiscard]] const std::vector<std::string>& getFiles2Watch() const;
    [[nodiscard]] const std::string& getHashPath()      const;
    [[nodiscard]] const std::string& getGuestFilePath() const;
    [[nodiscard]] const std::string& getNetworkInterface() const;
    [[nodiscard]] const std::vector<NetworkFilter>& getNetworkFilters() const;

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
    std::string networkInterface{"red-central"};
    std::vector<NetworkFilter> networkFilters;
};

#endif // CONFIG_H
