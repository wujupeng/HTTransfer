#pragma once

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QMainWindow>
#include <QStyle>
#include <QMessageBox>

namespace ht {

class SystemTrayManager : public QObject {
    Q_OBJECT

public:
    explicit SystemTrayManager(QMainWindow* main_window, QObject* parent = nullptr)
        : QObject(parent), main_window_(main_window) {
        tray_icon_ = new QSystemTrayIcon(this);
        tray_icon_->setIcon(QIcon(":/icons/app.png"));
        tray_icon_->setToolTip("HTTransfer");

        tray_menu_ = new QMenu();
        show_action_ = tray_menu_->addAction(tr("Show Main Window"));
        tray_menu_->addSeparator();
        quit_action_ = tray_menu_->addAction(tr("Exit"));

        tray_icon_->setContextMenu(tray_menu_);

        connect(show_action_, &QAction::triggered, this, &SystemTrayManager::onShowWindow);
        connect(quit_action_, &QAction::triggered, this, &SystemTrayManager::onQuit);
        connect(tray_icon_, &QSystemTrayIcon::activated, this, &SystemTrayManager::onActivated);
    }

    bool isAvailable() const {
        return QSystemTrayIcon::isSystemTrayAvailable();
    }

    void show() {
        tray_icon_->show();
    }

    void hide() {
        tray_icon_->hide();
    }

    void showMessage(const QString& title, const QString& message) {
        if (tray_icon_->isVisible()) {
            tray_icon_->showMessage(title, message, QSystemTrayIcon::Information, 5000);
        }
    }

signals:
    void quitRequested();
    void showWindowRequested();

private slots:
    void onShowWindow() {
        if (main_window_) {
            main_window_->show();
            main_window_->raise();
            main_window_->activateWindow();
        }
        emit showWindowRequested();
    }

    void onQuit() {
        emit quitRequested();
    }

    void onActivated(QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            onShowWindow();
        }
    }

private:
    QMainWindow* main_window_ = nullptr;
    QSystemTrayIcon* tray_icon_ = nullptr;
    QMenu* tray_menu_ = nullptr;
    QAction* show_action_ = nullptr;
    QAction* quit_action_ = nullptr;
};

}