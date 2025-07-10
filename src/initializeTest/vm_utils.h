#ifndef VM_UTILS_H
#define VM_UTILS_H

#include <string>
#include <libvirt/libvirt.h>

bool ensureBridgeExists();
bool waitForState(virDomainPtr dom, int desired, int timeoutSec);
void configureNewVM(const std::string& vmName, virConnectPtr conn);
void ensureDirectoryExists(const std::string& pathStr);

#endif // VM_UTILS_H
