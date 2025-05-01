#ifndef MONITOR_H
#define MONITOR_H

#include <string>
#include <libvirt/libvirt.h>
#include <libvmi/libvmi.h>

class Monitor {
public:
    explicit Monitor(std::string  vm_name, const virConnectPtr &conn);  // Constructor que inicializa LibVMI
    ~Monitor();                           // Destructor para limpiar recursos
    bool initialize();                    // Inicializa la conexión con la VM
    [[nodiscard]] std::string getKernelName(virDomainPtr domain) const;          // Obtiene el nombre del kernel
    [[nodiscard]] vmi_instance_t get_vmi() const;             // Get VMI


private:
    vmi_instance_t vmi;            // Instancia de LibVMI
    std::string vm_name;                  // Nombre de la VM huésped
    virConnectPtr conn;
};

#endif
