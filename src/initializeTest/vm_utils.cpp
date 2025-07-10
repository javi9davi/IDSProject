// vm_utils.cpp
#include "vm_utils.h"

#include <filesystem>
#include <iostream>
#include <thread>
#include <tinyxml2.h>
#include <libvirt/libvirt.h>
#include <libvirt/libvirt-qemu.h>

bool ensureBridgeExists() {
    const std::string bridgeName = "red-central";
    std::string checkCmd = "ip link show " + bridgeName + " > /dev/null 2>&1";
    if (std::system(checkCmd.c_str()) != 0) {
        std::cout << "[INFO] Bridge '" << bridgeName << "' not found, creating..." << std::endl;
        std::string addCmd = "ip link add name " + bridgeName + " type bridge";
        if (std::system(addCmd.c_str()) != 0) {
            std::cerr << "[ERROR] Failed to add bridge: " << bridgeName << std::endl;
            return false;
        }
        std::string upCmd = "ip link set dev " + bridgeName + " up";
        if (std::system(upCmd.c_str()) != 0) {
            std::cerr << "[ERROR] Failed to bring up bridge: " << bridgeName << std::endl;
            return false;
        }
        std::cout << "[INFO] Bridge '" << bridgeName << "' created and up." << std::endl;
    } else {
        std::cout << "[INFO] Bridge '" << bridgeName << "' already exists." << std::endl;
    }
    return true;
}

bool waitForState(virDomainPtr dom, int desired, int timeoutSec) {
    auto start = std::chrono::steady_clock::now();
    int state = -1;
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < timeoutSec) {
        if (virDomainGetState(dom, &state, nullptr, 0) == 0 && state == desired) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return false;
}


// Configure a new VM: remove default NAT, add bridge and guest-agent channel, then verify
void configureNewVM(const std::string& vmName, virConnectPtr conn)
{
    virDomainPtr dom = virDomainLookupByName(conn, vmName.c_str());
    if (!dom) {
        std::cerr << "[ERROR] Could not find domain for VM: " << vmName << std::endl;
        return;
    }

    char* xmlDesc = virDomainGetXMLDesc(dom, 0);
    if (!xmlDesc) {
        std::cerr << "[ERROR] Failed to get XML for VM: " << vmName << std::endl;
        virDomainFree(dom);
        return;
    }

    tinyxml2::XMLDocument doc;
    doc.Parse(xmlDesc);
    auto* domainElem = doc.FirstChildElement("domain");
    auto* devices = domainElem ? domainElem->FirstChildElement("devices") : nullptr;
    if (!devices) {
        std::cerr << "[ERROR] <devices> missing in XML for VM: " << vmName << std::endl;
        free(xmlDesc);
        virDomainFree(dom);
        return;
    }

    // Detect existing 'red-central' bridge and guest-agent channel
    bool hasBridge = false;
    bool hasChannel = false;
    for (auto* e = devices->FirstChildElement(); e; e = e->NextSiblingElement()) {
        std::string tag = e->Name();
        if (tag == "interface" && !hasBridge) {
            const char* type = e->Attribute("type");
            if (type && std::string(type) == "bridge") {
                auto* src = e->FirstChildElement("source");
                if (src) {
                    const char* br = src->Attribute("bridge");
                    if (br && std::string(br) == "red-central") {
                        hasBridge = true;
                    }
                }
            }
        }
        if (tag == "channel" && !hasChannel) {
            const char* type = e->Attribute("type");
            if (type && std::string(type) == "unix") {
                auto* tgt = e->FirstChildElement("target");
                if (tgt) {
                    const char* name = tgt->Attribute("name");
                    if (name && std::string(name) == "org.qemu.guest_agent.0") {
                        hasChannel = true;
                    }
                }
            }
        }
        if (hasBridge && hasChannel) break;
    }

    // Remove default NAT interface if present
    for (auto* e = devices->FirstChildElement(); e; ) {
        auto* next = e->NextSiblingElement();
        if (std::string(e->Name()) == "interface") {
            const char* type = e->Attribute("type");
            if (type && std::string(type) == "network") {
                auto* src = e->FirstChildElement("source");
                if (src) {
                    const char* nw = src->Attribute("network");
                    if (nw && std::string(nw) == "default") {
                        devices->DeleteChild(e);
                        std::cout << "[INFO] Removed default NAT interface from VM: " << vmName << std::endl;
                    }
                }
            }
        }
        e = next;
    }

    if (!hasBridge) {
        auto* iface = doc.NewElement("interface");
        iface->SetAttribute("type", "bridge");
        auto* source = doc.NewElement("source");
        source->SetAttribute("bridge", "red-central");
        iface->InsertEndChild(source);
        devices->InsertEndChild(iface);
        std::cout << "[INFO] Added 'red-central' bridge to VM: " << vmName << std::endl;
    }

    if (!hasChannel) {
        auto* channel = doc.NewElement("channel");
        channel->SetAttribute("type", "unix");
        auto* target = doc.NewElement("target");
        target->SetAttribute("type", "virtio");
        target->SetAttribute("name", "org.qemu.guest_agent.0");
        channel->InsertEndChild(target);
        devices->InsertEndChild(channel);
        std::cout << "[INFO] Added QEMU guest-agent channel to VM: " << vmName << std::endl;
    }

    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    const char* newXml = printer.CStr();

    if (!hasBridge || !hasChannel)
    {
        std::cout << "[INFO] VM " << vmName << " will be powered off to apply XML modifications." << std::endl;
        if (virDomainShutdown(dom) < 0) {
            std::cerr << "[ERROR] Failed to shutdown VM: " << vmName << std::endl;
            free(xmlDesc);
            virDomainFree(dom);
            return;
        }

        // Wait until powered off
        int state = -1;
        while (virDomainGetState(dom, &state, nullptr, 0) == 0 && state != VIR_DOMAIN_SHUTOFF) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }


        // Redefine domain with updated XML
        if (virDomainDefineXML(conn, newXml) == nullptr) {
            std::cerr << "[ERROR] Failed to redefine VM XML for: " << vmName << std::endl;
        } else {
            std::cout << "[INFO] VM XML updated successfully for: " << vmName << std::endl;
            // Start VM
            if (virDomainCreate(dom) < 0) {
                std::cerr << "[ERROR] Failed to start VM: " << vmName << std::endl;
            } else {
                std::cout << "[INFO] VM " << vmName << " has been restarted to apply changes." << std::endl;
            }
        }

        // Verify guest-agent via ping
        auto sendAgentCmd = [&](const char* cmd) {
            char* result = virDomainQemuAgentCommand(dom, cmd, 15, 0);
            bool success = (result != nullptr);
            if (result) free(result);
            return success;
        };

        const char* cmdPing = R"({"execute":"guest-ping"})";
        if (!sendAgentCmd(cmdPing)) {
            std::cerr << "[WARN] QEMU guest-agent not responding on VM: " << vmName << std::endl;
        } else {
            std::cout << "[INFO] QEMU guest-agent is operational on VM: " << vmName << std::endl;
        }

        // Cleanup
        free(xmlDesc);
        virDomainFree(dom);

    }
}

void ensureDirectoryExists(const std::string& pathStr) {
    namespace fs = std::filesystem;
    if (const fs::path dirPath(pathStr); fs::exists(dirPath)) {
        if (fs::is_directory(dirPath)) {
            std::cout << "El directorio ya existe: " << dirPath << std::endl;
        } else {
            std::cerr << "La ruta existe pero no es un directorio: " << dirPath << std::endl;
        }
    } else {
        try {
            if (fs::create_directories(dirPath)) {
                std::cout << "Directorio creado: " << dirPath << std::endl;
            } else {
                std::cerr << "No se pudo crear el directorio: " << dirPath << std::endl;
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error al crear el directorio: " << e.what() << std::endl;
        }
    }
}
