#ifndef MONITOR_H
#define MONITOR_H

#include <string>
#include <libvirt/libvirt.h>

class Monitor {
public:
    explicit Monitor(std::string  vm_name, const virConnectPtr &conn);  // Constructor que inicializa LibVMI
    bool initialize();                    // Inicializa la conexión con la VM
    [[nodiscard]] std::string getKernelName(virDomainPtr domain) const;          // Obtiene el nombre del kernel


private:
    std::string vm_name;                  // Nombre de la VM huésped
    virConnectPtr conn;
};

#endif
