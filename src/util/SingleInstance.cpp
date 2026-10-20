#include "SingleInstance.h"

#include <QLocalServer>
#include <QLocalSocket>

SingleInstance::SingleInstance(QString key, QObject *parent)
    : QObject(parent), m_key(std::move(key)) {}

bool SingleInstance::tryLock() {
    auto pingExisting = [this] {
        QLocalSocket socket;
        socket.connectToServer(m_key);
        if (!socket.waitForConnected(200))
            return false;
        socket.write("raise");
        socket.flush();
        socket.waitForBytesWritten(200);
        return true;
    };

    if (pingExisting())
        return false;

    // 只清崩溃留下的管道名。活着的实例在上面已经连上，不会进到这里。
    QLocalServer::removeServer(m_key);

    m_server = new QLocalServer(this);
    if (!m_server->listen(m_key)) {
        // 对方刚 listen 成功。再通知一次并退出，不能再开一个窗口。
        pingExisting();
        return false;
    }

    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (auto *conn = m_server->nextPendingConnection()) {
            connect(conn, &QLocalSocket::disconnected,
                    conn, &QObject::deleteLater);
            emit anotherInstanceStarted();
        }
    });

    return true;
}
