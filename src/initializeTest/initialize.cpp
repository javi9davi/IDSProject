// initialize.cpp
#include "initialize.h"
#include <cstdio>
#include <cstdlib>

// Constructor para inicializar la conexión
VMManager::VMManager() {
    // Conectar al hipervisor local
    conn = virConnectOpen("qemu:///system");
    if (conn == NULL) {
        fprintf(stderr, "Error: No se pudo conectar al hipervisor.\n");
    }
}

// Destructor para cerrar la conexión
VMManager::~VMManager() {
    if (conn) {
        virConnectClose(conn);
    }
}

// Función para obtener los nombres de las máquinas virtuales
std::vector<std::string> VMManager::get_vm_names() {
    std::vector<std::string> vm_names;

    if (!conn) return vm_names; // Retorna un vector vacío si no hay conexión

    // Obtener la lista de todas las máquinas virtuales (activas e inactivas)
    int numDomains = virConnectListAllDomains(conn, NULL, VIR_CONNECT_LIST_DOMAINS_ACTIVE | VIR_CONNECT_LIST_DOMAINS_INACTIVE);
    if (numDomains < 0) {
        fprintf(stderr, "Error: No se pudieron listar las máquinas virtuales.\n");
        return vm_names; // Retorna un vector vacío
    }

    // Crear un array para guardar los IDs de los dominios
    int *activeDomains = (int*)malloc(sizeof(int) * numDomains);
    int numActiveDomains = virConnectListDomains(conn, activeDomains, numDomains);
    if (numActiveDomains < 0) {
        fprintf(stderr, "Error: No se pudieron listar los dominios activos.\n");
        free(activeDomains);
        return vm_names; // Retorna un vector vacío
    }

    // Iterar por cada dominio activo y obtener su nombre
    for (int i = 0; i < numActiveDomains; i++) {
        virDomainPtr dom = virDomainLookupByID(conn, activeDomains[i]);
        if (dom) {
            const char *domName = virDomainGetName(dom);
            if (domName) {
                vm_names.push_back(std::string(domName)); // Agregar el nombre al vector
            }
            virDomainFree(dom);
        }
    }

    free(activeDomains);
    return vm_names; // Retorna el vector con los nombres de las VMs
}

// Función optimizada para monitorear las máquinas virtuales
void VMManager::monitor_vms() const {
    if (!conn) {
        fprintf(stderr, "Error: No se pudo conectar al hipervisor.\n");
        return;
    }

    // Obtener la lista de todas las máquinas virtuales
    int numDomains = virConnectListAllDomains(conn, NULL, VIR_CONNECT_LIST_DOMAINS_ACTIVE | VIR_CONNECT_LIST_DOMAINS_INACTIVE);
    if (numDomains < 0) {
        fprintf(stderr, "Error: No se pudieron listar las máquinas virtuales.\n");
        return;
    }

    printf("Número de máquinas virtuales detectadas: %d\n", numDomains);

    // Crear un array para guardar los IDs de los dominios
    int *activeDomains = (int*)malloc(sizeof(int) * numDomains);
    int numActiveDomains = virConnectListDomains(conn, activeDomains, numDomains);
    if (numActiveDomains < 0) {
        fprintf(stderr, "Error: No se pudieron listar los dominios activos.\n");
        free(activeDomains);
        return;
    }

    // Iterar por cada dominio activo y obtener su información
    for (int i = 0; i < numActiveDomains; i++) {
        virDomainPtr dom = virDomainLookupByID(conn, activeDomains[i]);
        if (dom) {
            virDomainInfo domInfo;

            if (virDomainGetInfo(dom, &domInfo) == 0) {
                char *domName = (char*)virDomainGetName(dom);
                printf("\nDominio: %s\n", domName);
                printf("Estado: %d\n", domInfo.state);
                printf("Memoria asignada: %lu KB\n", domInfo.memory);
                printf("Número de vCPUs: %u\n", domInfo.nrVirtCpu);
                printf("Tiempo de CPU: %llu ns\n", domInfo.cpuTime);
            } else {
                fprintf(stderr, "Error: No se pudo obtener información del dominio.\n");
            }

            virDomainFree(dom);
        }
    }

    free(activeDomains);
}

virConnectPtr VMManager::get_connection() {
    return conn; // Retorna el puntero a la conexión
}
