#include "monitorui.h"
#include <QVBoxLayout>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Función para cargar un archivo .env como mapa clave/valor
std::unordered_map<std::string, std::string> loadEnvFile(const std::string& filename) {
    std::unordered_map<std::string, std::string> env;
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            env[key] = value;
        }
    }
    return env;
}

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

    // Cargar configuración
    config.loadFromFile("../config/config.json");

    // Cargar API_KEY desde variable de entorno o archivo .env
    const char* envApiKey = std::getenv("API_KEY");
    std::string apiKey;
    if (envApiKey) {
        apiKey = envApiKey;
    } else {
        auto envMap = loadEnvFile("../config/.env");
        auto it = envMap.find("API_KEY");
        if (it != envMap.end()) {
            apiKey = it->second;
        }
    }

    if (apiKey.empty()) {
        statusLabel->setText("API Key no encontrada.");
    } else {
        setenv("API_KEY", apiKey.c_str(), 1);
    }

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

