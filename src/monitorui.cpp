#include "monitorui.h"
#include <QVBoxLayout>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

MonitorUI::MonitorUI(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("IDS - Monitor de Máquinas Virtuales");
    resize(600, 400);

    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);

    statusLabel = new QLabel("Iniciando IDS...", this);
    layout->addWidget(statusLabel);

    vmList = new QListWidget(this);
    layout->addWidget(vmList);

    refreshButton = new QPushButton("Actualizar VMs", this);
    layout->addWidget(refreshButton);

    connect(refreshButton, &QPushButton::clicked, this, &MonitorUI::updateVMs);

    widget->setLayout(layout);
    setCentralWidget(widget);

    config.loadFromFile("../config/config.json");
    vmManager = std::make_unique<VMManager>();
    conn = vmManager->get_connection();

    fs::path hashPath = fs::current_path().parent_path() / "hashStore";
    config.setHashPath(hashPath.string());

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MonitorUI::updateVMs);
    timer->start(config.getUpdateInterval() * 1000);

    updateVMs();
}

MonitorUI::~MonitorUI() {
    for (auto& [_, monitor] : monitors) monitor->stop();
    for (auto& thread : threads) if (thread.joinable()) thread.join();
}

void MonitorUI::updateVMs() {
    auto vmNames = vmManager->get_vm_names();
    for (const auto& vmName : vmNames) {
        if (monitors.find(vmName) == monitors.end()) {
            auto fileMonitor = std::make_shared<FileMonitor>(config, vmName, conn);
            auto processMonitor = std::make_shared<ProcessMonitor>(vmName, conn);
            auto monitorThread = std::make_shared<MonitorThread>(vmName, conn, fileMonitor, processMonitor, config);
            threads.emplace_back(&MonitorThread::run, monitorThread);
            monitors[vmName] = monitorThread;

            vmList->addItem("Monitoreando VM: " + QString::fromStdString(vmName));
        }
    }
    statusLabel->setText("Vigilando " + QString::number(monitors.size()) + " VM(s)");
}
