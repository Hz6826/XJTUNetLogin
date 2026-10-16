#include "SingleInstance.h"

#include <QLocalServer>
#include <QLocalSocket>

SingleInstance::SingleInstance(QString key, QObject *parent)
    : QObject(parent), m_key(std::move(key)) {}

bool SingleInstance::tryLock() {
    {
        QLocalSocket socket;
        socket.connectToServer(m_key);
        if (socket.waitForConnected(200)) {
            socket.write("raise");
            socket.flush();
            socket.waitForBytesWritten(200);
            return false;
        }
    }

    QLocalServer::removeServer(m_key);

    m_server = new QLocalServer(this);
    if (!m_server->listen(m_key))
        return true;

    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (auto *conn = m_server->nextPendingConnection()) {
            connect(conn, &QLocalSocket::disconnected,
                    conn, &QObject::deleteLater);
            emit anotherInstanceStarted();
        }
    });

    return true;
}
