#ifndef IDS_BACKEND_SERVICE_H
#define IDS_BACKEND_SERVICE_H

#include <QObject>
#include <QString>
#include <thread>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <memory>
#include <atomic>

#include "monitor/monitor.h"
#include "monitor/fileMonitor/fileMonitor.h"
#include "initializeTest/initialize.h"
#include "core/config.h"

class IDSBackendService : public QObject {
    Q_OBJECT

public:
    explicit IDSBackendService(QObject* parent = nullptr);
    ~IDSBackendService();

    void start();

signals:
    void alertGenerated(const QString& vmName, const QString& message);

private:
    void monitoringLoop();
    void monitorVM(const std::string& vmName);

    std::vector<std::thread> threads;
    std::unordered_set<size_t> monitoredVMs;
    std::mutex mtx;
    std::atomic<bool> running;

    Config config;
    VMManager vmManager;
};

#endif // IDS_BACKEND_SERVICE_H
