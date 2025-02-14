#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <set>
#include <chrono>
#include "initializeTest/initialize.h"
#include "monitor/monitor.h"

// Clase para manejar cada monitor en un hilo separado
class MonitorThread {
public:
    MonitorThread(const std::string& vmName, virConnectPtr conn)
        : vmName(vmName), conn(conn) {}

    void operator()() {
        Monitor monitor(vmName, conn);
        if (monitor.initialize()) {
            std::cout << "Monitoreando la VM: " << vmName << std::endl;
            while (shouldContinue) {
                std::this_thread::sleep_for(std::chrono::seconds(5)); // Simula tiempo de trabajo
                std::cout << "Continuando la monitorización de la VM: " << vmName << std::endl;
            }
        } else {
            std::cerr << "Error al inicializar el monitor para la VM: " << vmName << std::endl;
        }
    }

    void stop() {
        shouldContinue = false; // Método para detener el monitoreo
    }

private:
    std::string vmName;
    virConnectPtr conn;
    bool shouldContinue = true;
};

int main() {
    VMManager vmManager;
    std::vector<std::thread> threads;
    std::set<std::string> monitoredVMs; // Conjunto para rastrear VMs monitoreadas
    std::mutex mtx; // Mutex para proteger acceso a hilos y VMs

    // Bucle de monitoreo para nuevas VMs
    while (true) {
        std::vector<std::string> vm_names = vmManager.get_vm_names(); // Obtener lista actual de VMs

        if (vm_names.size() == 0) {
            std::cout << "No hay MVs disponibles" << std::endl;
        }

        // Proteger el acceso a la lista de hilos
        mtx.lock();
        for (const auto& vm_name : vm_names) {
            if (monitoredVMs.find(vm_name) == monitoredVMs.end()) { // Si la VM no está monitoreada
                std::cout << "Inicializando el thread para la VM: " << vm_name << std::endl;
                threads.emplace_back(MonitorThread(vm_name, vmManager.get_connection())); // Lanzar un hilo
                monitoredVMs.insert(vm_name); // Marcar la VM como monitoreada
            }
        }
        mtx.unlock();

        // Esperar un tiempo antes de volver a verificar
        std::this_thread::sleep_for(std::chrono::seconds(10)); // Ajusta el tiempo según sea necesario
    }

    // (No se llegará aquí debido al bucle infinito)
    for (auto& thread : threads) {
        thread.join(); // Unirse a cada hilo
    }

    return 0;
}
