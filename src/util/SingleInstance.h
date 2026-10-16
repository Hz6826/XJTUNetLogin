#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

class SingleInstance : public QObject {
    Q_OBJECT
public:
    explicit SingleInstance(QString key, QObject *parent = nullptr);

    bool tryLock();

    signals:
        void anotherInstanceStarted();

private:
    QString m_key;
    QLocalServer *m_server = nullptr;
};
