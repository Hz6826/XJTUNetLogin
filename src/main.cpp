#include <AppInfo.h>
#include <QApplication>
#include "net/SrunClient.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    QApplication::setOrganizationName(AppInfo::Author);
    QApplication::setApplicationName(AppInfo::Name);
    QApplication::setApplicationVersion(AppInfo::Version);

    SrunClient client;
    MainWindow w(&client);
    client.startPolling();
    w.show();

    return a.exec();
}