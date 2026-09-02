#pragma once

#include "PAL/IPlatformSingleInstance.h"
#include "Core/Common/Result.h"
#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QString>
#include <functional>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace ht {

class MutexSingleInstance : public IPlatformSingleInstance {
public:
    MutexSingleInstance() : server_name_("HTTransferSingleInstance") {}

    ~MutexSingleInstance() {
        releaseLock();
        if (server_) server_->close();
    }

    bool tryAcquireLock() override {
        mutex_handle_ = CreateMutexW(nullptr, TRUE, L"Local\\HT-Transfer-SingleInstance");
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            return false;
        }
        acquired_ = (mutex_handle_ != nullptr);
        return acquired_;
    }

    void releaseLock() override {
        if (mutex_handle_) {
            CloseHandle(mutex_handle_);
            mutex_handle_ = nullptr;
        }
        acquired_ = false;
    }

    bool isAcquired() const override { return acquired_; }

    void sendRestoreNotice() override {
        QLocalSocket socket;
        socket.connectToServer(server_name_);
        if (socket.waitForConnected(2000)) {
            socket.write("RESTORE_FOREGROUND");
            socket.flush();
            socket.waitForBytesWritten(2000);
            socket.disconnectFromServer();
        }
    }

    void startListening(std::function<void()> on_restore) override {
        QLocalServer::removeServer(server_name_);
        server_ = new QLocalServer();
        server_->listen(server_name_);
        QObject::connect(server_, &QLocalServer::newConnection, [this, on_restore]() {
            auto* client = server_->nextPendingConnection();
            if (client) {
                client->waitForReadyRead(1000);
                delete client;
                if (on_restore) on_restore();
            }
        });
    }

    bool isSupported() const override { return true; }

private:
    HANDLE mutex_handle_ = nullptr;
    bool acquired_ = false;
    QString server_name_;
    QLocalServer* server_ = nullptr;
};

}

#endif