#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <iostream>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

class Config {
public:
    // Constructor por defecto
    Config();

    // Constructor con parámetros
    Config(int update_interval, int monitoring_interval, bool monitor_files, bool monitor_processes, std::vector<std::string>& files2watch);

    // Métodos para establecer los valores
    void setUpdateInterval(int interval);
    void setMonitoringInterval(int interval);
    void setMonitorFiles(bool monitor);
    void setMonitorProcesses(bool monitor);
    void setFiles2Watch(const std::vector<std::string>& files);
    void setHashPath(const std::string& path);

    // Métodos para obtener los valores
    [[nodiscard]] int getUpdateInterval() const;
    [[nodiscard]] int getMonitoringInterval() const;
    [[nodiscard]] bool isMonitorFiles() const;
    [[nodiscard]] bool isMonitorProcesses() const;
    [[nodiscard]] std::vector<std::string> getFiles2Watch() const;
    [[nodiscard]] const std::string& getHashPath() const;

    // Método para cargar la configuración desde un archivo JSON
    bool loadFromFile(const std::string& filename);

private:
    std::string configFile;
    nlohmann::json configData;
    std::string hash_path;
    std::vector<std::string> files2watch;
    int update_interval{};           // Intervalo de actualización
    int monitoring_interval{};       // Intervalo de monitoreo
    bool monitor_files{};            // Indica si se deben monitorear archivos
    bool monitor_processes{};        // Indica si se deben monitorear procesos
};

#endif // CONFIG_H