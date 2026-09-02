#pragma once

#include "PAL/IPlatformTray.h"
#include <QSystemTrayIcon>
#include <QMenu>
#include <QApplication>
#include <QProcessEnvironment>
#include <string>

#ifdef __linux__
#include <cstdlib>

namespace ht {

class SniTray : public IPlatformTray {
public:
    SniTray() {
        tray_ = new QSystemTrayIcon();
    }

    ~SniTray() {
        delete tray_;
    }

    bool isAvailable() const override {
        return QSystemTrayIcon::isSystemTrayAvailable();
    }

    DisplayServer detectDisplayServer() const override {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString waylandDisplay = env.value("WAYLAND_DISPLAY");
        QString x11Display = env.value("DISPLAY");

        if (!waylandDisplay.isEmpty()) {
            return DisplayServer::Wayland;
        }
        if (!x11Display.isEmpty()) {
            return DisplayServer::X11;
        }
        return DisplayServer::Unknown;
    }

    void showIcon() override {
        if (tray_) tray_->show();
    }

    void hideIcon() override {
        if (tray_) tray_->hide();
    }

    void showMessage(const std::string& title, const std::string& message) override {
        if (tray_) {
            tray_->showMessage(
                QString::fromStdString(title),
                QString::fromStdString(message),
                QSystemTrayIcon::Information,
                5000
            );
        }
    }

private:
    QSystemTrayIcon* tray_ = nullptr;
};

}

#endif