#pragma once

#include "PAL/IPlatformAutoStart.h"
#include "Core/Common/Result.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <string>

#ifdef __linux__
#include <unistd.h>

namespace ht {

class DesktopAutoStart : public IPlatformAutoStart {
public:
    Result<void> enable() override {
        QString autostartDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
        QDir().mkpath(autostartDir);
        QString desktopPath = autostartDir + "/httransfer.desktop";

        QString exePath = QCoreApplication::applicationFilePath();
        QString content = QString(
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=HTTransfer\n"
            "Comment=High-Performance File Copy Engine\n"
            "Exec=%1 --minimized\n"
            "Icon=httransfer\n"
            "Terminal=false\n"
            "Categories=Utility;System;\n"
            "StartupNotify=false\n"
            "X-GNOME-Autostart-enabled=true\n"
        ).arg(exePath);

        QFile file(desktopPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return Result<void>::failure(ErrorCode::ConfigError, "Cannot create .desktop file");
        }
        QTextStream stream(&file);
        stream << content;
        file.close();

        return Result<void>::success();
    }

    Result<void> disable() override {
        QString autostartDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
        QString desktopPath = autostartDir + "/httransfer.desktop";
        QFile::remove(desktopPath);
        return Result<void>::success();
    }

    bool isEnabled() const override {
        QString autostartDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
        QString desktopPath = autostartDir + "/httransfer.desktop";
        return QFile::exists(desktopPath);
    }

    bool isSupported() const override { return true; }
};

}

#endif