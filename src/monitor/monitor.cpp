#include "monitor.h"
#include <iostream>
#include <utility>
#include <libvirt/libvirt-domain.h>
#include <json-c/json.h>
#include <libvirt/libvirt.h>
#include <libvirt/libvirt-qemu.h>
#include <thread>
#include <chrono>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <glib.h>

Monitor::Monitor(std::string  vm_name, const virConnectPtr &conn)
    : vm_name(std::move(vm_name)), conn(conn) {}


bool Monitor::initialize()
{
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
    return true;
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

    decoded.erase(decoded.find_last_not_of(" \n\r\t") + 1);

    return decoded;
}