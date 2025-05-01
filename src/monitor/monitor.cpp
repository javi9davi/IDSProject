#include "monitor.h"
#include <iostream>
#include <utility>
#include <vector>
#include <libvirt/libvirt-domain.h>
#include <libvmi/libvmi.h>
#include <json-c/json.h>
#include <libvirt/libvirt.h>
#include <libvirt/libvirt-qemu.h>
#include <thread>
#include <chrono>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include "../libvmiApi/libvmiCalls.h"

Monitor::Monitor(std::string  vm_name, const virConnectPtr &conn)
    : vm_name(std::move(vm_name)), conn(conn), vmi(nullptr) {}


Monitor::~Monitor() {
    if (vmi) {
        vmi_destroy(vmi);  // Liberar recursos de LibVMI
        std::cout << "LibVMI cerrado correctamente." << std::endl;
    }
}

vmi_instance_t Monitor::get_vmi() const {
    return vmi;
}


bool Monitor::initialize() {
    if (!conn) {
        std::cerr << "[ERROR] Conexión con libvirt no inicializada." << std::endl;
        return false;
    }

    virDomainPtr domain = virDomainLookupByName(conn, vm_name.c_str());
    if (!domain) {
        std::cerr << "[ERROR] No se encontró la VM: " << vm_name << std::endl;
        return false;
    }

    std::string kernelName = getKernelName(domain);
    if (kernelName.empty()) {
        std::cerr << "[ERROR] No se pudo obtener el kernel de la VM: " << vm_name << std::endl;
        return false;
    }

    if (!generateProfileAndConfigureLibVMI(kernelName, vm_name)) {
        std::cerr << "[ERROR] No se pudo generar el perfil LibVMI para la VM: " << vm_name << std::endl;
        return false;
    }

    vmi_init_error_t error_info;
    status_t status = vmi_init_complete(
        &vmi,
        vm_name.c_str(),
        VMI_INIT_DOMAINNAME,
        nullptr,
        VMI_CONFIG_GLOBAL_FILE_ENTRY,
        nullptr,
        &error_info
    );

    if (status == VMI_SUCCESS) {
        std::cout << "[INFO] Conexión establecida con la VM: " << vm_name << std::endl;
        return true;
    }

    std::cerr << "[ERROR] Falló la inicialización de LibVMI para: " << vm_name << std::endl;

    switch (error_info) {
        case VMI_INIT_ERROR_NONE: std::cerr << "No error.\n"; break;
        case VMI_INIT_ERROR_DRIVER_NOT_DETECTED: std::cerr << "Hypervisor no detectado.\n"; break;
        case VMI_INIT_ERROR_DRIVER: std::cerr << "Error con driver de LibVMI.\n"; break;
        case VMI_INIT_ERROR_VM_NOT_FOUND: std::cerr << "VM no encontrada.\n"; break;
        case VMI_INIT_ERROR_PAGING: std::cerr << "Error en paginación.\n"; break;
        case VMI_INIT_ERROR_OS: std::cerr << "Error en funciones de SO.\n"; break;
        case VMI_INIT_ERROR_EVENTS: std::cerr << "Error inicializando eventos.\n"; break;
        case VMI_INIT_ERROR_NO_CONFIG: std::cerr << "No hay configuración de SO.\n"; break;
        case VMI_INIT_ERROR_NO_CONFIG_ENTRY: std::cerr << "No hay entrada en vmi.conf.\n"; break;
        default: std::cerr << "Error desconocido.\n"; break;
    }

    return false;
}


std::string Monitor::getKernelName(virDomainPtr domain) const {

    if (!domain) {
        return "Dominio nulo.";
    }

    // guest-exec: /bin/uname -r
    std::string execCmd = R"({
        "execute": "guest-exec",
        "arguments": {
            "path": "/bin/uname",
            "arg": ["-r"],
            "capture-output": true
        }
    })";

    char* execResult = virDomainQemuAgentCommand(domain, execCmd.c_str(), -2, 0);
    if (!execResult) {
        std::cerr << "[ERROR] No se pudo ejecutar uname -r en la VM." << std::endl;
        return "";
    }

    std::string execStr(execResult);
    free(execResult);

    // Parsear respuesta
    json_object* root = json_tokener_parse(execStr.c_str());
    if (!root) {
        std::cerr << "[ERROR] JSON inválido de guest-exec." << std::endl;
        return "";
    }

    json_object* pidObj = json_object_object_get(root, "return");
    if (!pidObj || json_object_get_type(pidObj) != json_type_object) {
        json_object_put(root);
        return "";
    }

    json_object* pidField = json_object_object_get(pidObj, "pid");
    int pid = pidField ? json_object_get_int(pidField) : -1;
    json_object_put(root);

    if (pid == -1) {
        return "";
    }

    // Esperar a que termine
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::string statusCmd = R"({
        "execute": "guest-exec-status",
        "arguments": {
            "pid": )" + std::to_string(pid) + R"(
        }
    })";

    char* statusResult = virDomainQemuAgentCommand(domain, statusCmd.c_str(), -2, 0);
    if (!statusResult) {
        std::cerr << "[ERROR] No se pudo obtener estado del uname -r." << std::endl;
        return "";
    }

    std::string statusStr(statusResult);
    free(statusResult);

    root = json_tokener_parse(statusStr.c_str());
    if (!root) {
        std::cerr << "[ERROR] JSON inválido de guest-exec-status." << std::endl;
        return "";
    }

    json_object* outRoot = json_object_object_get(root, "return");
    if (!outRoot || json_object_get_type(outRoot) != json_type_object) {
        json_object_put(root);
        return "";
    }

    json_object* outContent = json_object_object_get(outRoot, "out-data");
    if (!outContent || json_object_get_type(outContent) != json_type_string) {
        json_object_put(root);
        return "";
    }

    std::string outputBase64 = json_object_get_string(outContent);
    json_object_put(root);

    // Decodificar base64
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bio = BIO_new_mem_buf(outputBase64.data(), outputBase64.length());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    std::string decoded(outputBase64.size(), '\0');
    int decodedLen = BIO_read(bio, decoded.data(), outputBase64.length());
    BIO_free_all(bio);

    if (decodedLen <= 0) {
        return "";
    }

    decoded.resize(decodedLen);
    return decoded;
}