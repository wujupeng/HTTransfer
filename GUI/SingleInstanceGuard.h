#pragma once

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QString>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ht {

class SingleInstanceGuard : public QObject {
    Q_OBJECT

public:
    explicit SingleInstanceGuard(QObject* parent = nullptr) : QObject(parent) {
        server_name_ = "HTTransferSingleInstance";
    }

    ~SingleInstanceGuard() {
#ifdef _WIN32
        if (mutex_handle_) {
            CloseHandle(mutex_handle_);
        }
#endif
        if (server_) {
            server_->close();
        }
    }

    bool tryAcquireLock() {
#ifdef _WIN32
        mutex_handle_ = CreateMutexW(nullptr, TRUE, L"Local\\HT-Transfer-SingleInstance");
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            return false;
        }
        return mutex_handle_ != nullptr;
#else
        return true;
#endif
    }

    void notifyExistingInstance() {
        QLocalSocket socket;
        socket.connectToServer(server_name_);
        if (socket.waitForConnected(2000)) {
            socket.write("RESTORE_FOREGROUND");
            socket.flush();
            socket.waitForBytesWritten(2000);
            socket.disconnectFromServer();
        }
    }

    void startListening() {
        QLocalServer::removeServer(server_name_);
        server_ = new QLocalServer(this);
        server_->listen(server_name_);
        connect(server_, &QLocalServer::newConnection, this, [this]() {
            auto* client = server_->nextPendingConnection();
            if (client) {
                client->waitForReadyRead(1000);
                delete client;
                emit restoreForegroundRequested();
            }
        });
    }

signals:
    void restoreForegroundRequested();

private:
    QString server_name_;
    QLocalServer* server_ = nullptr;
#ifdef _WIN32
    HANDLE mutex_handle_ = nullptr;
#endif
};

}