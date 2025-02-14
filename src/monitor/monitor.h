#ifndef MONITOR_H
#define MONITOR_H

#include <string>
#include <libvirt/libvirt.h>
#include <libvmi/libvmi.h>

class Monitor {
public:
    explicit Monitor(const std::string& vm_name, virConnectPtr conn);  // Constructor que inicializa LibVMI
    ~Monitor();                           // Destructor para limpiar recursos
    bool initialize();                    // Inicializa la conexión con la VM
    std::string getKernelName();          // Obtiene el nombre del kernel
    void listProcesses();                 // Lista procesos activos

private:
    vmi_instance_t vmi;                   // Instancia de LibVMI
    std::string vm_name;                  // Nombre de la VM huésped
    bool is_initialized;                  // Estado de inicialización
    virConnectPtr conn;
};

#endif
