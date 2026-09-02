#pragma once

#include "PAL/IPlatformSingleInstance.h"
#include "Core/Common/Result.h"
#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QString>
#include <QStandardPaths>
#include <QDir>
#include <functional>
#include <string>

#ifdef __linux__
#include <sys/file.h>
#include <unistd.h>

namespace ht {

class FlockSingleInstance : public IPlatformSingleInstance {
public:
    FlockSingleInstance() : server_name_("HTTransferSingleInstance") {}

    ~FlockSingleInstance() {
        releaseLock();
        if (server_) server_->close();
    }

    bool tryAcquireLock() override {
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        QString lockDir = dataDir + "/HunterTransfer";
        QDir().mkpath(lockDir);
        lockPath_ = (lockDir + "/single.lock").toStdString();

        lock_fd_ = ::open(lockPath_.c_str(), O_CREAT | O_RDWR, 0644);
        if (lock_fd_ < 0) {
            return false;
        }
        if (flock(lock_fd_, LOCK_EX | LOCK_NB) != 0) {
            ::close(lock_fd_);
            lock_fd_ = -1;
            return false;
        }
        acquired_ = true;
        return true;
    }

    void releaseLock() override {
        if (lock_fd_ >= 0) {
            flock(lock_fd_, LOCK_UN);
            ::close(lock_fd_);
            lock_fd_ = -1;
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
    int lock_fd_ = -1;
    bool acquired_ = false;
    std::string lockPath_;
    QString server_name_;
    QLocalServer* server_ = nullptr;
};

}

#endif