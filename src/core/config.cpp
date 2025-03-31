#include <iostream>
#include <fstream>
#include <string>
#include "nlohmann/json.hpp"
#include "config.h"

using json = nlohmann::json;

// Constructor por defecto
Config::Config()
    : update_interval(5), monitoring_interval(10) {}

// Constructor con parámetros
Config::Config(const int update_interval, const int monitoring_interval, const bool monitor_files, const bool monitor_processes)
    : update_interval(update_interval), monitoring_interval(monitoring_interval),
      monitor_files(monitor_files), monitor_processes(monitor_processes) {}

// Métodos `set`
void Config::setUpdateInterval(const int interval) { update_interval = interval; }
void Config::setMonitoringInterval(const int interval) { monitoring_interval = interval; }
void Config::setMonitorFiles(const bool monitor) { monitor_files = monitor; }
void Config::setMonitorProcesses(const bool monitor) { monitor_processes = monitor; }

// Métodos `get`
int Config::getUpdateInterval() const { return update_interval; }
int Config::getMonitoringInterval() const { return monitoring_interval; }
bool Config::isMonitorFiles() const { return monitor_files; }
bool Config::isMonitorProcesses() const { return monitor_processes; }

// Método para cargar la configuración desde un archivo JSON
bool Config::loadFromFile(const std::string& filename) {
    std::ifstream configFile(filename);

    if (!configFile) {
        std::cerr << "Error al abrir el archivo de configuración: " << filename << std::endl;
        return false;
    }

    json configJson;
    configFile >> configJson;

    if (configJson.contains("General")) {
        if (configJson["General"].contains("Update_Interval")) {
            setUpdateInterval(configJson["General"]["Update_Interval"].get<int>());
        }
    }

    if (configJson.contains("Monitoring")) {
        if (configJson["Monitoring"].contains("Monitor_Processes")) {
            setMonitorProcesses(configJson["Monitoring"]["Monitor_Processes"].get<bool>());
            setMonitoringInterval(isMonitorProcesses() ? 5 : 10);
        }
        if (configJson["Monitoring"].contains("Monitor_Files")) {
            setMonitorFiles(configJson["Monitoring"]["Monitor_Files"].get<bool>());
        }
    }

    return true;
}
