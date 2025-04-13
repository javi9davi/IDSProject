// initialize.h
#ifndef INITIALIZE_H
#define INITIALIZE_H

#include <string>
#include <vector>
#include <libvirt/libvirt.h>

class VMManager {
public:
    VMManager();
    ~VMManager();

    std::vector<std::string> get_vm_names();
    void monitor_vms() const;
    virConnectPtr get_connection(); // Obtener conexión

private:
    virConnectPtr conn;
};

#endif // INITIALIZE_H
