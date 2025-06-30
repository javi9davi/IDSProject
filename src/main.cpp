#include <QApplication>
#include "monitorui.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MonitorUI window;
    window.show();
    return app.exec();
}
