#include <AppInfo.h>
#include <QApplication>
// ReSharper disable once CppUnusedIncludeDirective
#include <QIcon>
#include <QTimer>

#include "net/SrunClient.h"
#include "ui/MainWindow.h"
#include "ui/TrayController.h"
#include "util/SingleInstance.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(AppInfo::Author);
    QApplication::setApplicationName(AppInfo::Name);
    QApplication::setApplicationVersion(AppInfo::Version);

    // ---------- 单实例保护 ----------
    SingleInstance instance(
        QStringLiteral("%1-%2").arg(AppInfo::Author, AppInfo::Name));

    if (!instance.tryLock()) {
        // 已有实例在跑，已通知它唤起窗口，当前进程静默退出
        return 0;
    }
    // --------------------------------

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

    // 第二个实例启动 → 唤起已有窗口
    QObject::connect(&instance, &SingleInstance::anotherInstanceStarted,
                     &window, [&window] {
                         if (window.isMinimized()) {
                             window.showNormal();
                         } else {
                             window.show();
                         }
                         QTimer::singleShot(0, &window, [&window] {
                             window.raise();
                             window.activateWindow();
                         });
                     });

    client.startPolling(window.config().pollTimerIntervalMs);
    window.show();

    return QApplication::exec();
}
