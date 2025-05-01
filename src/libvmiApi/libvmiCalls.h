//
// Created by javierdr on 28/04/25.
//

#ifndef LIBVMICALLS_H
#define LIBVMICALLS_H

/**
 * Generates a LibVMI profile for a VM and configures it in the system
 * @param kernelName The kernel version string (e.g., "5.4.0-generic")
 * @param vmName The name of the virtual machine
 * @return true if profile generation and configuration succeeded, false otherwise
 */
bool generateProfileAndConfigureLibVMI(const std::string& kernelName, const std::string& vmName);
bool downloadVmlinuzFromVM(virDomainPtr dom, const std::string& kernelName, const std::string& outputPath);


#endif //LIBVMICALLS_H
