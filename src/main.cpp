#include <AppInfo.h>
#include <QApplication>
// ReSharper disable once CppUnusedIncludeDirective
#include <QIcon>

#include "net/SrunClient.h"
#include "ui/MainWindow.h"
#include "ui/TrayController.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(AppInfo::Author);
    QApplication::setApplicationName(AppInfo::Name);
    QApplication::setApplicationVersion(AppInfo::Version);

    QIcon appIcon;
    appIcon.addFile(":/icons/app/app-16.png");
    appIcon.addFile(":/icons/app/app-32.png");
    appIcon.addFile(":/icons/app/app-48.png");
    appIcon.addFile(":/icons/app/app-64.png");
    appIcon.addFile(":/icons/app/app-128.png");
    appIcon.addFile(":/icons/app/app-256.png");
    QApplication::setWindowIcon(appIcon);

    SrunClient client;
    MainWindow window(&client);
    TrayController tray(&client, &window);

    client.startPolling();
    window.show();

    return app.exec();
}
