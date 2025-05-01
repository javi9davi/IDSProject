#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <json-c/json.h>
#include <libvirt/libvirt.h>
#include <libvirt/libvirt-qemu.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <algorithm>

bool downloadVmlinuzFromVM(virDomainPtr dom, const std::string& kernelName, const std::string& outputPath) {
    std::string guestPath = "/boot/vmlinuz-" + kernelName.substr(0, kernelName.find_last_of('\n'));

    // guest-file-open
    std::string openCmd = R"({
        "execute": "guest-file-open",
        "arguments": {
            "path": ")" + guestPath + R"(",
            "mode": "r"
        }
    })";

    char* openResult = virDomainQemuAgentCommand(dom, openCmd.c_str(), -2, 0);
    if (!openResult) {
        std::cerr << "[ERROR] No se pudo abrir el archivo en la VM: " << guestPath << std::endl;
        return false;
    }

    std::string openStr(openResult);
    free(openResult);

    json_object* root = json_tokener_parse(openStr.c_str());
    if (!root) {
        std::cerr << "[ERROR] JSON inválido de guest-file-open" << std::endl;
        return false;
    }

    json_object* handleJson = json_object_object_get(root, "return");
    if (!handleJson || json_object_get_type(handleJson) != json_type_int) {
        std::cerr << "[ERROR] Handle no encontrado" << std::endl;
        json_object_put(root);
        return false;
    }

    int handle = json_object_get_int(handleJson);
    json_object_put(root);

    // guest-file-read
    std::ofstream outputFile(outputPath, std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "[ERROR] No se pudo crear archivo temporal para vmlinuz." << std::endl;
        return false;
    }

    while (true) {
        std::string readCmd = R"({
            "execute": "guest-file-read",
            "arguments": {
                "handle": )" + std::to_string(handle) + R"(
            }
        })";

        char* readResult = virDomainQemuAgentCommand(dom, readCmd.c_str(), -2, 0);
        if (!readResult) {
            std::cerr << "[ERROR] Fallo al leer fragmento del archivo" << std::endl;
            break;
        }

        std::string readStr(readResult);
        free(readResult);

        json_object* readRoot = json_tokener_parse(readStr.c_str());
        if (!readRoot) {
            std::cerr << "[ERROR] JSON inválido de guest-file-read" << std::endl;
            break;
        }

        json_object* returnObj = json_object_object_get(readRoot, "return");
        if (!returnObj || json_object_get_type(returnObj) != json_type_object) {
            json_object_put(readRoot);
            break;
        }

        json_object* eof = json_object_object_get(returnObj, "eof");
        if (eof && json_object_get_boolean(eof)) {
            json_object_put(readRoot);
            break; // terminó de leer
        }

        json_object* buf64 = json_object_object_get(returnObj, "buf-b64");
        if (buf64 && json_object_get_type(buf64) == json_type_string) {
            std::string bufferBase64 = json_object_get_string(buf64);

            // Decode base64
            BIO* b64 = BIO_new(BIO_f_base64());
            BIO* bio = BIO_new_mem_buf(bufferBase64.data(), bufferBase64.size());
            bio = BIO_push(b64, bio);
            BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

            std::string buffer(bufferBase64.size(), '\0');
            int len = BIO_read(bio, buffer.data(), bufferBase64.size());
            if (len > 0) {
                outputFile.write(buffer.data(), len);
            }

            BIO_free_all(bio);
        }

        json_object_put(readRoot);
    }

    outputFile.close();

    // guest-file-close
    std::string closeCmd = R"({
        "execute": "guest-file-close",
        "arguments": {
            "handle": )" + std::to_string(handle) + R"(
        }
    })";

    virDomainQemuAgentCommand(dom, closeCmd.c_str(), -2, 0);

    std::cout << "[INFO] vmlinuz descargado desde VM a: " << outputPath << std::endl;
    return true;
}

std::string cleanName(const std::string& kernelName) {
    std::string cleanedName = kernelName;
    std::string suffixToRemove = "-generic";

    // Verificar si cleanedName termina con suffixToRemove
    if (cleanedName.size() >= suffixToRemove.size() &&
        cleanedName.compare(cleanedName.size() - suffixToRemove.size(), suffixToRemove.size(), suffixToRemove) == 0) {
        // Eliminar el sufijo
        cleanedName.erase(cleanedName.size() - suffixToRemove.size());
        }

    return cleanedName;
}


bool isVMInVMIConf(const std::string& vmName, const std::string& confPath) {
    std::ifstream file(confPath);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.find(vmName + " {") != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool generateProfileAndConfigureLibVMI(const std::string& kernelName, const std::string& vmName) {
    namespace fs = std::filesystem;

    std::string baseDir = "/etc/libvmi/profiles/";
    std::string tempDir = baseDir + vmName + "_temp";
    std::string profilePath = baseDir + vmName + "_profile.json";
    std::string vmiConfPath = "/etc/libvmi.conf";

    std::string cleanKernelName = kernelName;
    cleanKernelName.erase(std::remove(cleanKernelName.begin(), cleanKernelName.end(), '\n'), cleanKernelName.end());
    cleanKernelName.erase(std::remove(cleanKernelName.begin(), cleanKernelName.end(), '\r'), cleanKernelName.end());

    if (isVMInVMIConf(vmName, vmiConfPath)) {
        std::cout << "[INFO] La VM " << vmName << " ya está registrada en vmi.conf. No se realizará generación de perfil." << std::endl;
        return true;
    }

    std::string packageName = "linux-image-" + cleanKernelName + "-dbgsym_" + cleanName(cleanKernelName) + "_ppc64el.ddeb";
    std::string downloadURL = "http://ddebs.ubuntu.com/pool/main/l/linux/" + packageName;
    std::string downloadedPackage = baseDir + packageName;

    fs::create_directories(baseDir);

    if (!fs::exists(downloadedPackage)) {
        std::cout << "[INFO] Descargando: " << downloadURL << std::endl;
        std::string wgetCommand = "wget -O " + downloadedPackage + " " + downloadURL;
        if (std::system(wgetCommand.c_str()) != 0) {
            std::cerr << "[ERROR] Error al descargar el paquete ddeb." << std::endl;
            return false;
        }
    } else {
        std::cout << "[INFO] El paquete ya estaba descargado: " << downloadedPackage << std::endl;
    }

    fs::create_directories(tempDir);
    std::string dpkgCommand = "dpkg-deb -x " + downloadedPackage + " " + tempDir;
    if (std::system(dpkgCommand.c_str()) != 0) {
        std::cerr << "[ERROR] Error al extraer el paquete ddeb." << std::endl;
        return false;
    }

    std::string extractedVmlinux = tempDir + "/usr/lib/debug/boot/vmlinux-" + cleanKernelName;
    if (!fs::exists(extractedVmlinux)) {
        std::cerr << "[ERROR] No se encontró vmlinux extraído." << std::endl;
        return false;
    }

    std::cout << "[INFO] Generando perfil con dwarf2json..." << std::endl;
    //TODO CAMBIAR LA RUTA POR AUTOMATICA O CONFIG
    std::string dwarfCommand = "/home/javierdr/dwarf2json/dwarf2json linux --elf " + extractedVmlinux + " > " + profilePath;
    if (std::system(dwarfCommand.c_str()) != 0) {
        std::cerr << "[ERROR] Error ejecutando dwarf2json." << std::endl;
        return false;
    }

    std::ofstream vmiConf(vmiConfPath, std::ios_base::app);
    if (!vmiConf.is_open()) {
        std::cerr << "[ERROR] No se pudo abrir " << vmiConfPath << std::endl;
        return false;
    }

    vmiConf << vmName << " {\n";
    vmiConf << "    ostype = \"Linux\";\n";
    vmiConf << "    rekall_profile = \"" << profilePath << "\";\n";
    vmiConf << "}\n";

    vmiConf.close();

    std::cout << "[INFO] Configuración de LibVMI actualizada para VM: " << vmName << std::endl;
    return true;
}
