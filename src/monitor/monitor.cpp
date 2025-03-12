#include "monitor.h"
#include <iostream>
#include <vector>
#include <libvirt/libvirt-domain.h>
#include <libvmi/libvmi.h>

Monitor::Monitor(const std::string& vm_name, virConnectPtr conn)
    : vm_name(vm_name), vmi(nullptr), is_initialized(false), conn(conn) {}

Monitor::~Monitor() {
    if (is_initialized && vmi) {
        vmi_destroy(vmi);  // Liberar recursos de LibVMI
        std::cout << "LibVMI cerrado correctamente." << std::endl;
    }
}


bool Monitor::initialize() {
    vmi_init_error_t error_info; // Variable para capturar el error
    vmi_instance_t vmi = nullptr;
    vmi_mode_t mode; // Variable para el modo de acceso
    constexpr uint64_t init_flags = VMI_INIT_DOMAINNAME; // Inicializar con el flag apropiado
    vmi_init_data_t init_data; // Estructura para datos de inicialización (ajusta según sea necesario)
    const std::string xmlFilePathStr = "/etc/libvirt/qemu/" + vm_name + ".xml";
    const char *xmlFilePath = xmlFilePathStr.c_str();

    // Determina el modo de acceso
    const status_t access_status = vmi_get_access_mode(
        nullptr,                  // Pasar NULL para determinar el modo automáticamente
        xmlFilePath,              // Nombre de la VM
        init_flags,               // Flags de inicialización
        &init_data,               // Datos de inicialización adicionales
        &mode                     // Puntero donde se almacenará el modo
    );

    // Manejo de errores al determinar el modo de acceso
    if (access_status != VMI_SUCCESS) {
        std::cerr << "Error determining access mode." << std::endl;
        return false;
    }
    // Inicializa VMI utilizando la función vmi_init
    const status_t status = vmi_init(
        &vmi,                               // Estructura que contendrá la instancia
        mode,                               // Modo de acceso determinado
        xmlFilePath,                        // Nombre de la VM
        init_flags,                         // Flags de inicialización
        &init_data,                         // Datos de inicialización adicionales
        &error_info                         // Puntero para capturar información de error
    );

    if (!conn) {
        std::cerr << "Conexión con libvirt no inicializada." << std::endl;
        return false;
    }
    virDomainPtr domain = virDomainLookupByName(conn, vm_name.c_str());
    if (!domain) {
        std::cerr << "No se encontró la VM: " << vm_name << std::endl;
        return false;
    }

    if (status == VMI_SUCCESS) {
        is_initialized = true;
        std::cout << "Conexión establecida con la VM: " << vm_name << std::endl;
        return true;
    }
        // Si la inicialización falla, imprime el error
        std::cerr << "Error al inicializar LibVMI para la VM: " << vm_name << std::endl;

    // Imprimir el mensaje de error basado en el valor de error_info
    switch (error_info) {
        case VMI_INIT_ERROR_NONE:
            std::cerr << "Error: No error." << std::endl;
            break;
        case VMI_INIT_ERROR_DRIVER_NOT_DETECTED:
            std::cerr << "Error: Failed to auto-detect hypervisor." << std::endl;
            break;
        case VMI_INIT_ERROR_DRIVER:
            std::cerr << "Error: Failed to initialize hypervisor driver." << std::endl;
            break;
        case VMI_INIT_ERROR_VM_NOT_FOUND:
            std::cerr << "Error: Failed to find the specified VM." << std::endl;
            break;
        case VMI_INIT_ERROR_PAGING:
            std::cerr << "Error: Failed to determine or initialize paging functions." << std::endl;
            break;
        case VMI_INIT_ERROR_OS:
            std::cerr << "Error: Failed to determine or initialize OS functions." << std::endl;
            break;
        case VMI_INIT_ERROR_EVENTS:
            std::cerr << "Error: Failed to initialize events." << std::endl;
            break;
        case VMI_INIT_ERROR_NO_CONFIG:
            std::cerr << "Error: No configuration was found for OS initialization." << std::endl;
            break;
        case VMI_INIT_ERROR_NO_CONFIG_ENTRY:
            std::cerr << "Error: No configuration entry found." << std::endl;
            break;
        // Agrega otros casos según sea necesario
        default:
            std::cerr << "Error: Unknown error." << std::endl;
            break;
    }

    return false;
}

std::string Monitor::getKernelName() const {
    if (!is_initialized) {
        return "LibVMI no inicializado.";
    }

    char* kernel_name = nullptr;

    // vmi_read_str_va devuelve un puntero
    kernel_name = vmi_read_str_va(vmi, 0xFFFF800000000000, 0);

    if (kernel_name != nullptr) {  // Comprueba si el puntero no es nulo
        std::string result(kernel_name);  // Convierte la cadena a std::string
        free(kernel_name);                // Libera la memoria
        return result;                    // Devuelve el nombre del kernel
    }
    return "No se pudo leer el nombre del kernel.";
}

void Monitor::listProcesses() const {
    addr_t kernel_addr = 0xFFFF800000000000;

    if (!is_initialized) {
        std::cerr << "LibVMI no inicializado. No se pueden listar procesos." << std::endl;
        return;
    }

    addr_t list_head = 0;
    addr_t list_entry = 0;

    // Suponiendo que los procesos están en una lista doblemente enlazada.
    if (vmi_get_offset(vmi, "linux_tasks", &list_head) != VMI_SUCCESS) {
        std::cerr << "No se pudo obtener la dirección de los procesos." << std::endl;
        return;
    }

    list_entry = list_head;

    do {
        constexpr int offset_to_name_field = 0x04;
        addr_t task_struct = 0;

        // Leer la dirección de la estructura de tarea (task_struct) desde la lista
        if (vmi_read_64_va(vmi, list_entry, 0, &task_struct) != VMI_SUCCESS) {
            break; // Salir del bucle si hay un error en la lectura
        }

        // Leer el nombre del proceso desde la memoria usando el desplazamiento adecuado
        if (char* process_name = vmi_read_str_va(vmi, task_struct + offset_to_name_field, 0); process_name != nullptr) {
            std::cout << "Proceso: " << process_name << std::endl;
            free(process_name); // Liberar la memoria asignada por vmi_read_str_va
        } else {
            std::cerr << "Error al leer el nombre del proceso." << std::endl;
        }

        // Leer la siguiente entrada de la lista (circular)
        if (vmi_read_64_va(vmi, task_struct, 0, &list_entry) != VMI_SUCCESS) {
            break; // Salir del bucle si hay un error en la lectura
        }

    } while (list_entry != list_head);
}
