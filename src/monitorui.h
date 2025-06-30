#ifndef MONITORUI_H
#define MONITORUI_H

#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <unordered_map>
#include <memory>
#include <thread>
#include "core/config.h"
#include "initializeTest/initialize.h"
#include "monitor/fileMonitor/fileMonitor.h"
#include "monitor/processMonitor/processMonitor.h"
#include "monitor/MonitorThread.h"

class MonitorUI : public QMainWindow {
    Q_OBJECT

public:
    explicit MonitorUI(QWidget* parent = nullptr);
    ~MonitorUI();

public slots:
    void updateVMs();

private:
    QLabel* statusLabel;
    QListWidget* vmList;
    QPushButton* refreshButton;
    QTimer* timer;

    Config config;
    std::unique_ptr<VMManager> vmManager;
    virConnectPtr conn;
    std::unordered_map<std::string, std::shared_ptr<MonitorThread>> monitors;
    std::vector<std::thread> threads;
};

#endif // MONITORUI_H
