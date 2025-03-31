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

    Config(const Config& config);

    // Constructor con parámetros
    Config(int update_interval, int monitoring_interval, bool monitor_files, bool monitor_processes);

    Config(std::string filename);

    bool load();

    bool save() const;

    // Métodos para establecer los valores
    void setUpdateInterval(int interval);
    void setMonitoringInterval(int interval);
    void setMonitorFiles(bool monitor);
    void setMonitorProcesses(bool monitor);

    // Métodos para obtener los valores
    [[nodiscard]] int getUpdateInterval() const;
    [[nodiscard]] int getMonitoringInterval() const;
    [[nodiscard]] bool isMonitorFiles() const;
    [[nodiscard]] bool isMonitorProcesses() const;

    // Método para cargar la configuración desde un archivo JSON
    bool loadFromFile(const std::string& filename);

private:
    std::string configFile;
    nlohmann::json configData;
    int update_interval{};       // Intervalo de actualización
    int monitoring_interval{};   // Intervalo de monitoreo
    bool monitor_files{};        // Indica si se deben monitorear archivos
    bool monitor_processes{};    // Indica si se deben monitorear procesos
};

#endif // CONFIG_H