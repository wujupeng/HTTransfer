#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressBar>
#include <QTimer>
#include <QTranslator>
#include <QComboBox>
#include <QListWidget>
#include <QSettings>
#include <QApplication>
#include <QEvent>
#include <QStandardPaths>
#include <QDebug>
#include <thread>
#include <format>
#include <QMenuBar>
#include <QAction>
#include <QCloseEvent>
#include "Core/TaskManager.h"
#include "Core/Common/VersionInfo.h"
#include "AboutDialog.h"
#include "WatchConfigDialog.h"
#include "Watch/IWatchSession.h"
#include "SystemTrayManager.h"
#include "Config/AppConfigManager.h"
#include "Config/AutoStartManager.h"

namespace ht {

class HtTranslator : public QTranslator {
    Q_OBJECT
public:
    explicit HtTranslator(QObject* parent = nullptr) : QTranslator(parent) {}

    void setLanguage(const QString& lang) {
        translations_.clear();
        if (lang == "zh") {
            translations_ = {
                {"HTTransfer", "HTTransfer"},
                {"Source", "源路径"},
                {"Target", "目标路径"},
                {"Options", "选项"},
                {"Multi-thread", "多线程"},
                {"Overwrite", "覆盖"},
                {"Resume", "断点续传"},
                {"Verify", "校验"},
                {"Speed Limit", "限速"},
                {"Threads:", "线程数:"},
                {"Start", "开始"},
                {"Pause", "暂停"},
                {"Continue", "继续"},
                {"Stop", "停止"},
                {"Progress", "进度"},
                {"Status: Ready", "状态: 就绪"},
                {"Status: Starting...", "状态: 启动中..."},
                {"Status: Checking file stability...", "状态: 检查文件稳定性..."},
                {"Status: Transferring...", "状态: 传输中..."},
                {"Status: Verifying integrity...", "状态: 校验完整性..."},
                {"Status: Completed", "状态: 完成"},
                {"Status: Failed", "状态: 失败"},
                {"Status: Paused", "状态: 已暂停"},
                {"Status: Resuming...", "状态: 恢复中..."},
                {"Status: Stopped", "状态: 已停止"},
                {"Status: Cancelled", "状态: 已取消"},
                {"Language", "语言"},
                {"Error", "错误"},
                {"Please specify source and target paths", "请指定源路径和目标路径"},
                {"Select Source File", "选择源文件"},
                {"Select Source Directory", "选择源目录"},
                {"Select Target Directory", "选择目标目录"},
                {"All Files (*.*)", "所有文件 (*.*)"},
                {"Backup Files (*.bak *.bkf)", "备份文件 (*.bak *.bkf)"},
                {"Archive Files (*.zip *.7z *.tar *.gz *.rar)", "压缩文件 (*.zip *.7z *.tar *.gz *.rar)"},
                {"Disk Images (*.iso *.vhd *.vhdx)", "磁盘镜像 (*.iso *.vhd *.vhdx)"},
                {"Select source type", "选择源类型"},
                {"File", "文件"},
                {"Directory", "目录"},
                {"Crash Recovery", "崩溃恢复"},
                {"The following unfinished tasks were found from a previous session:\n\n", "发现以下上次未完成的任务:\n\n"},
                {"Would you like to recover these tasks?", "是否恢复这些任务？"},
            };
        }
        current_lang_ = lang;
    }

    QString translate(const char* context, const char* sourceText,
                      const char* disambiguation = nullptr, int n = -1) const override {
        Q_UNUSED(context); Q_UNUSED(disambiguation); Q_UNUSED(n);
        auto it = translations_.find(QString::fromUtf8(sourceText));
        if (it != translations_.end()) return it.value();
        return QString::fromUtf8(sourceText);
    }

    bool isEmpty() const override { return false; }
    QString currentLanguage() const { return current_lang_; }

signals:
    void languageChanged();

private:
    QMap<QString, QString> translations_;
    QString current_lang_ = "en";
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(std::shared_ptr<ITaskManager> task_manager,
                        std::shared_ptr<IWatchSession> watch_session = nullptr,
                        bool minimized = false,
                        QWidget* parent = nullptr)
        : QMainWindow(parent), task_manager_(std::move(task_manager)),
          watch_session_(std::move(watch_session)), minimized_start_(minimized) {
        config_manager_ = std::make_unique<AppConfigManager>();
        auto_start_manager_ = std::make_unique<AutoStartManager>();
        auto config = config_manager_->load();
        QString saved_lang = QString::fromUtf8(config.language.c_str());

        translator_ = new HtTranslator(this);
        translator_->setLanguage(saved_lang);
        qApp->installTranslator(translator_);

        setWindowTitle(tr("HTTransfer") + QString(" %1").arg(VersionInfo::version_string));
        setWindowIcon(QIcon(":/icons/app.png"));
        setMinimumSize(640, 480);

        auto* central = new QWidget(this);
        setCentralWidget(central);
        auto* main_layout = new QVBoxLayout(central);

        source_group_ = new QGroupBox(tr("Source Directories"), this);
        auto* source_layout = new QVBoxLayout(source_group_);
        source_list_ = new QListWidget(this);
        source_list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
        source_list_->setMaximumHeight(100);
        auto* source_btn_layout = new QHBoxLayout();
        auto* add_dir_btn = new QPushButton(tr("Add Dir"), this);
        auto* add_file_btn = new QPushButton(tr("Add File"), this);
        auto* remove_btn = new QPushButton(tr("Remove"), this);
        auto* clear_btn = new QPushButton(tr("Clear"), this);
        add_dir_btn->setFixedWidth(70);
        add_file_btn->setFixedWidth(70);
        remove_btn->setFixedWidth(70);
        clear_btn->setFixedWidth(60);
        connect(add_dir_btn, &QPushButton::clicked, this, &MainWindow::browseSourceDir);
        connect(add_file_btn, &QPushButton::clicked, this, &MainWindow::browseSourceFile);
        connect(remove_btn, &QPushButton::clicked, this, [this]() {
            auto selected = source_list_->selectedItems();
            for (auto* item : selected) delete item;
        });
        connect(clear_btn, &QPushButton::clicked, this, [this]() { source_list_->clear(); });
        source_btn_layout->addWidget(add_dir_btn);
        source_btn_layout->addWidget(add_file_btn);
        source_btn_layout->addWidget(remove_btn);
        source_btn_layout->addWidget(clear_btn);
        source_btn_layout->addStretch();
        source_layout->addWidget(source_list_);
        source_layout->addLayout(source_btn_layout);
        main_layout->addWidget(source_group_);

        target_group_ = new QGroupBox(tr("Target"), this);
        auto* target_layout = new QHBoxLayout(target_group_);
        target_edit_ = new QLineEdit(this);
        target_edit_->setPlaceholderText("\\\\HQSERVER\\Share\\ERP\\ or D:\\Backup\\");
        auto* target_browse = new QPushButton("...", this);
        target_browse->setFixedWidth(40);
        connect(target_browse, &QPushButton::clicked, this, &MainWindow::browseTarget);
        target_layout->addWidget(target_edit_);
        target_layout->addWidget(target_browse);
        main_layout->addWidget(target_group_);

        options_group_ = new QGroupBox(tr("Options"), this);
        auto* options_layout = new QHBoxLayout(options_group_);
        multi_thread_cb_ = new QCheckBox(tr("Multi-thread"), this);
        multi_thread_cb_->setChecked(true);
        overwrite_cb_ = new QCheckBox(tr("Overwrite"), this);
        resume_cb_ = new QCheckBox(tr("Resume"), this);
        resume_cb_->setChecked(true);
        verify_cb_ = new QCheckBox(tr("Verify"), this);
        verify_cb_->setChecked(true);
        speed_limit_cb_ = new QCheckBox(tr("Speed Limit"), this);
        speed_spin_ = new QSpinBox(this);
        speed_spin_->setRange(1, 10000);
        speed_spin_->setValue(100);
        speed_spin_->setSuffix(" MB/s");
        speed_spin_->setEnabled(false);
        connect(speed_limit_cb_, &QCheckBox::toggled, speed_spin_, &QSpinBox::setEnabled);
        thread_spin_ = new QSpinBox(this);
        thread_spin_->setRange(1, 8);
        thread_spin_->setValue(4);
        options_layout->addWidget(multi_thread_cb_);
        options_layout->addWidget(overwrite_cb_);
        options_layout->addWidget(resume_cb_);
        options_layout->addWidget(verify_cb_);
        options_layout->addWidget(speed_limit_cb_);
        options_layout->addWidget(speed_spin_);
        options_layout->addWidget(new QLabel(tr("Threads:"), this));
        options_layout->addWidget(thread_spin_);
        autostart_cb_ = new QCheckBox(tr("Auto Start"), this);
        autostart_cb_->setChecked(auto_start_manager_->isEnabled());
        options_layout->addWidget(autostart_cb_);
        connect(autostart_cb_, &QCheckBox::toggled, this, [this](bool checked) {
            if (checked) {
                auto r = auto_start_manager_->enable();
                if (r.isErr()) {
                    autostart_cb_->setChecked(false);
                    QMessageBox::warning(this, tr("Error"), QString::fromStdString(r.errorMessage()));
                }
            } else {
                auto_start_manager_->disable();
            }
        });
        main_layout->addWidget(options_group_);

        auto* button_layout = new QHBoxLayout();
        start_btn_ = new QPushButton(tr("Start"), this);
        pause_btn_ = new QPushButton(tr("Pause"), this);
        resume_btn_ = new QPushButton(tr("Continue"), this);
        stop_btn_ = new QPushButton(tr("Stop"), this);
        connect(start_btn_, &QPushButton::clicked, this, &MainWindow::onStart);
        connect(pause_btn_, &QPushButton::clicked, this, &MainWindow::onPause);
        connect(resume_btn_, &QPushButton::clicked, this, &MainWindow::onResume);
        connect(stop_btn_, &QPushButton::clicked, this, &MainWindow::onStop);
        button_layout->addWidget(start_btn_);
        button_layout->addWidget(pause_btn_);
        button_layout->addWidget(resume_btn_);
        button_layout->addWidget(stop_btn_);

        watch_btn_ = new QPushButton(tr("Incremental Backup"), this);
        watch_btn_->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
        connect(watch_btn_, &QPushButton::clicked, this, &MainWindow::onWatchButtonClicked);
        button_layout->addWidget(watch_btn_);

        main_layout->addLayout(button_layout);

        progress_group_ = new QGroupBox(tr("Progress"), this);
        auto* progress_layout = new QVBoxLayout(progress_group_);
        progress_bar_ = new QProgressBar(this);
        progress_bar_->setRange(0, 100);
        status_label_ = new QLabel(tr("Status: Ready"), this);
        speed_label_ = new QLabel(tr("Speed: --"), this);
        remaining_label_ = new QLabel(tr("Remaining: --"), this);
        progress_layout->addWidget(progress_bar_);
        progress_layout->addWidget(status_label_);
        progress_layout->addWidget(speed_label_);
        progress_layout->addWidget(remaining_label_);
        main_layout->addWidget(progress_group_);

        watch_status_label_ = new QLabel("", this);
        watch_status_label_->setStyleSheet("QLabel { color: #666; font-size: 11px; }");
        main_layout->addWidget(watch_status_label_);

        auto* help_menu = menuBar()->addMenu(tr("Help"));

        auto* lang_menu = help_menu->addMenu(tr("Language"));
        auto* en_action = lang_menu->addAction("English");
        auto* zh_action = lang_menu->addAction("中文");
        connect(en_action, &QAction::triggered, this, [this]() { switchLanguage("en"); });
        connect(zh_action, &QAction::triggered, this, [this]() { switchLanguage("zh"); });

        auto* about_action = help_menu->addAction(tr("About"));
        connect(about_action, &QAction::triggered, this, [this]() {
            AboutDialog dlg(this);
            dlg.exec();
        });

        updateButtonStates(TaskStatus::Created);

        progress_timer_ = new QTimer(this);
        connect(progress_timer_, &QTimer::timeout, this, &MainWindow::updateProgress);

        if (watch_session_) {
            watch_session_->registerStatusCallback(
                [this](WatchStatus status, const WatchStatistics& stats) {
                    QMetaObject::invokeMethod(this, [this, status]() {
                        updateWatchButtonStates(status);
                    }, Qt::QueuedConnection);
                });
        }

        watch_status_timer_ = new QTimer(this);
        connect(watch_status_timer_, &QTimer::timeout, this, &MainWindow::updateWatchStatistics);
        watch_status_timer_->start(500);

        tray_manager_ = new SystemTrayManager(this, this);
        if (tray_manager_->isAvailable()) {
            tray_manager_->show();
            connect(tray_manager_, &SystemTrayManager::quitRequested, this, [this]() {
                saveCurrentConfig();
                QApplication::quit();
            });
        }

        restoreConfig(config);

        if (minimized_start_ && tray_manager_ && tray_manager_->isAvailable()) {
            this->hide();
        } else {
            this->show();
        }

        QTimer::singleShot(100, this, &MainWindow::checkRecoverableTasks);
    }

protected:
    void closeEvent(QCloseEvent* event) override {
        if (tray_manager_ && tray_manager_->isAvailable()) {
            this->hide();
            tray_manager_->showMessage(tr("HTTransfer"), tr("Running in background. Right-click tray icon to exit."));
            event->ignore();
        } else {
            saveCurrentConfig();
            event->accept();
        }
    }

private slots:

    void onWatchButtonClicked() {
        if (!watch_session_) return;

        auto status = watch_session_->getStatus();
        if (status == WatchStatus::Running) {
            watch_session_->stopWatch();
            return;
        }

        auto* dlg = new WatchConfigDialog(this);
        connect(dlg, &WatchConfigDialog::startWatchRequested, this,
            [this](const QString& source, const QString& target, int interval) {
                auto result = watch_session_->startWatch(
                    source.toUtf8().toStdString(),
                    target.toUtf8().toStdString(),
                    interval);
                if (result.isErr()) {
                    QMessageBox::critical(this, tr("Error"),
                        QString::fromStdString(result.errorMessage()));
                }
            });
        dlg->exec();
        delete dlg;
    }

    void updateWatchButtonStates(WatchStatus status) {
        if (status == WatchStatus::Running) {
            watch_btn_->setText(tr("Stop Backup"));
            watch_btn_->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; }");
        } else {
            watch_btn_->setText(tr("Incremental Backup"));
            watch_btn_->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
        }
    }

    void updateWatchStatistics() {
        if (!watch_session_) return;
        auto stats = watch_session_->getStatistics();
        if (stats.status == WatchStatus::Running) {
            watch_status_label_->setText(
                tr("Monitoring: interval %1s | detected: %2 | backed up: %3")
                    .arg(stats.scan_interval)
                    .arg(stats.total_detected)
                    .arg(stats.total_backed_up));
        } else if (stats.status == WatchStatus::Error) {
            watch_status_label_->setText(tr("Monitor error - click to restart"));
        } else {
            watch_status_label_->setText("");
        }
    }

    void switchLanguage(const QString& lang) {
        qApp->removeTranslator(translator_);
        translator_->setLanguage(lang);
        qApp->installTranslator(translator_);

        QSettings settings("HTTransfer", "HTTransfer");
        settings.setValue("language", lang);

        retranslateUi();
    }

    void retranslateUi() {
        setWindowTitle(tr("HTTransfer") + QString(" %1").arg(VersionInfo::version_string));
        source_group_->setTitle(tr("Source Directories"));
        target_group_->setTitle(tr("Target"));
        options_group_->setTitle(tr("Options"));
        progress_group_->setTitle(tr("Progress"));
        multi_thread_cb_->setText(tr("Multi-thread"));
        overwrite_cb_->setText(tr("Overwrite"));
        resume_cb_->setText(tr("Resume"));
        verify_cb_->setText(tr("Verify"));
        speed_limit_cb_->setText(tr("Speed Limit"));
        start_btn_->setText(tr("Start"));
        pause_btn_->setText(tr("Pause"));
        resume_btn_->setText(tr("Continue"));
        stop_btn_->setText(tr("Stop"));
    }

    void browseSourceFile() {
        QString filters = "All Files (*.*);;Backup Files (*.bak *.bkf);;Archive Files (*.zip *.rar *.7z *.tar *.gz);;Disk Images (*.iso *.vhd *.vhdx)";
        QString initialDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

        QStringList paths = QFileDialog::getOpenFileNames(
            this,
            tr("Select Source File"),
            initialDir,
            filters
        );

        for (const auto& path : paths) {
            if (!path.isEmpty()) source_list_->addItem(path);
        }
    }

    void browseSourceDir() {
        QString initialDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        QFileDialog dlg(this, tr("Select Source Directories"), initialDir);
        dlg.setFileMode(QFileDialog::Directory);
        dlg.setOption(QFileDialog::ShowDirsOnly, true);
        dlg.setOption(QFileDialog::DontUseNativeDialog, true);
        if (dlg.exec() == QDialog::Accepted) {
            for (const auto& path : dlg.selectedFiles()) {
                if (!path.isEmpty()) source_list_->addItem(path);
            }
        }
    }

    void browseTarget() {
        auto path = QFileDialog::getExistingDirectory(this, tr("Select Target Directory"));
        if (!path.isEmpty()) target_edit_->setText(path);
    }

    void onStart() {
        auto target = target_edit_->text().toUtf8().toStdString();
        if (source_list_->count() == 0 || target.empty()) {
            QMessageBox::warning(this, tr("Error"), tr("Please specify source and target paths"));
            return;
        }

        TransferPreset preset = TransferPreset::Balanced;
        if (!multi_thread_cb_->isChecked() && !verify_cb_->isChecked()) {
            preset = TransferPreset::Fast;
        } else if (verify_cb_->isChecked() && speed_limit_cb_->isChecked()) {
            preset = TransferPreset::Secure;
        }

        uint32_t parallelism = multi_thread_cb_->isChecked()
            ? static_cast<uint32_t>(thread_spin_->value()) : 1;
        uint64_t speed_limit = speed_limit_cb_->isChecked()
            ? static_cast<uint64_t>(speed_spin_->value()) * 1024 * 1024 : 0;

        for (int i = 0; i < source_list_->count(); ++i) {
            auto source = source_list_->item(i)->text().toUtf8().toStdString();
            auto src_path = std::filesystem::path(source);
            auto basename = src_path.filename().string();
            auto final_target = target;
            if (source_list_->count() > 1 && !basename.empty()) {
                final_target = target + "/" + basename;
            }

            auto result = task_manager_->createTask(source, final_target, preset, parallelism, speed_limit);
            if (result.isErr()) {
                QMessageBox::critical(this, tr("Error"), QString::fromStdString(result.errorMessage()));
                continue;
            }

            current_task_id_ = result.value();
            task_manager_->startTask(current_task_id_);
        }

        updateButtonStates(TaskStatus::Queued);
        status_label_->setText(tr("Status: Starting..."));

        progress_timer_->start(500);
    }

    void onPause() {
        if (!current_task_id_.empty()) {
            task_manager_->pauseTask(current_task_id_);
            updateButtonStates(TaskStatus::Paused);
            status_label_->setText(tr("Status: Paused"));
        }
    }

    void onResume() {
        if (!current_task_id_.empty()) {
            task_manager_->resumeTask(current_task_id_);
            updateButtonStates(TaskStatus::Transferring);
            status_label_->setText(tr("Status: Resuming..."));
        }
    }

    void onStop() {
        if (!current_task_id_.empty()) {
            task_manager_->cancelTask(current_task_id_);
            updateButtonStates(TaskStatus::Cancelled);
            status_label_->setText(tr("Status: Stopped"));
            progress_timer_->stop();
            current_task_id_.clear();
        }
    }

    void checkRecoverableTasks() {
        try {
        auto result = task_manager_->listRecoverableTasks();
        if (result.isErr()) return;

        auto& tasks = result.value();
        if (tasks.empty()) return;

        QString msg = tr("The following unfinished tasks were found from a previous session:\n\n");
        for (const auto& t : tasks) {
            msg += QString("%1 -> %2 (%3%)\n")
                .arg(QString::fromStdString(t.source_path))
                .arg(QString::fromStdString(t.target_path))
                .arg(static_cast<int>(t.progress_percent));
        }
        msg += "\n" + tr("Would you like to recover these tasks?");

        auto ret = QMessageBox::question(this, tr("Crash Recovery"), msg,
            QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::Yes) {
            task_manager_->recoverFromCrash();
            if (!tasks.empty()) {
                current_task_id_ = tasks[0].task_id;
                auto start_result = task_manager_->startTask(current_task_id_);
                if (start_result.isOk()) {
                    progress_timer_->start(500);
                }
            }
        } else {
            for (const auto& t : tasks) {
                task_manager_->cancelTask(t.task_id);
            }
        }
        } catch (...) {}
    }

    void updateProgress() {
        if (current_task_id_.empty()) return;
        auto status_result = task_manager_->getTaskStatus(current_task_id_);
        if (status_result.isErr()) return;

        auto status = status_result.value();
        switch (status) {
            case TaskStatus::Stabilizing:
                status_label_->setText(tr("Status: Checking file stability..."));
                break;
            case TaskStatus::Transferring:
                status_label_->setText(tr("Status: Transferring..."));
                break;
            case TaskStatus::Verifying:
                status_label_->setText(tr("Status: Verifying integrity..."));
                break;
            case TaskStatus::Completed:
                status_label_->setText(tr("Status: Completed"));
                progress_bar_->setValue(100);
                updateButtonStates(TaskStatus::Completed);
                progress_timer_->stop();
                return;
            case TaskStatus::Failed:
                status_label_->setText(tr("Status: Failed"));
                updateButtonStates(TaskStatus::Failed);
                progress_timer_->stop();
                return;
            case TaskStatus::Cancelled:
                status_label_->setText(tr("Status: Cancelled"));
                updateButtonStates(TaskStatus::Cancelled);
                progress_timer_->stop();
                return;
            default:
                break;
        }

        auto progress_result = task_manager_->getTaskProgress(current_task_id_);
        if (progress_result.isOk()) {
            auto& prog = progress_result.value();
            int pct = prog.total_bytes > 0
                ? static_cast<int>((static_cast<double>(prog.transferred_bytes) / prog.total_bytes) * 100.0)
                : 0;
            progress_bar_->setValue(pct);

            if (prog.speed_mbps > 0) {
                speed_label_->setText(QString("%1 MB/s").arg(prog.speed_mbps, 0, 'f', 1));
            }
            if (prog.estimated_remaining.count() > 0) {
                auto secs = prog.estimated_remaining.count();
                int h = static_cast<int>(secs / 3600);
                int m = static_cast<int>((secs % 3600) / 60);
                int s = static_cast<int>(secs % 60);
                if (h > 0) {
                    remaining_label_->setText(QString("%1h %2m %3s").arg(h).arg(m).arg(s));
                } else if (m > 0) {
                    remaining_label_->setText(QString("%1m %2s").arg(m).arg(s));
                } else {
                    remaining_label_->setText(QString("%1s").arg(s));
                }
            }
        }
    }

private:

    void updateButtonStates(TaskStatus status) {
        switch (status) {
            case TaskStatus::Created:
            case TaskStatus::Completed:
            case TaskStatus::Failed:
            case TaskStatus::Cancelled:
                start_btn_->setEnabled(true);
                pause_btn_->setEnabled(false);
                resume_btn_->setEnabled(false);
                stop_btn_->setEnabled(false);
                break;
            case TaskStatus::Queued:
            case TaskStatus::Stabilizing:
            case TaskStatus::Transferring:
            case TaskStatus::Verifying:
                start_btn_->setEnabled(false);
                pause_btn_->setEnabled(true);
                resume_btn_->setEnabled(false);
                stop_btn_->setEnabled(true);
                break;
            case TaskStatus::Paused:
                start_btn_->setEnabled(false);
                pause_btn_->setEnabled(false);
                resume_btn_->setEnabled(true);
                stop_btn_->setEnabled(true);
                break;
        }
    }

    std::shared_ptr<ITaskManager> task_manager_;
    HtTranslator* translator_ = nullptr;
    std::string current_task_id_;

    QListWidget* source_list_ = nullptr;
    QLineEdit* target_edit_ = nullptr;
    QGroupBox* source_group_ = nullptr;
    QGroupBox* target_group_ = nullptr;
    QGroupBox* options_group_ = nullptr;
    QGroupBox* progress_group_ = nullptr;
    QCheckBox* multi_thread_cb_ = nullptr;
    QCheckBox* overwrite_cb_ = nullptr;
    QCheckBox* resume_cb_ = nullptr;
    QCheckBox* verify_cb_ = nullptr;
    QCheckBox* speed_limit_cb_ = nullptr;
    QSpinBox* speed_spin_ = nullptr;
    QSpinBox* thread_spin_ = nullptr;
    QPushButton* start_btn_ = nullptr;
    QPushButton* pause_btn_ = nullptr;
    QPushButton* resume_btn_ = nullptr;
    QPushButton* stop_btn_ = nullptr;
    QProgressBar* progress_bar_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* speed_label_ = nullptr;
    QLabel* remaining_label_ = nullptr;
    QTimer* progress_timer_ = nullptr;

    std::shared_ptr<IWatchSession> watch_session_;
    QPushButton* watch_btn_ = nullptr;
    QLabel* watch_status_label_ = nullptr;
    QTimer* watch_status_timer_ = nullptr;

    std::unique_ptr<AppConfigManager> config_manager_;
    std::unique_ptr<AutoStartManager> auto_start_manager_;
    SystemTrayManager* tray_manager_ = nullptr;
    QCheckBox* autostart_cb_ = nullptr;
    bool minimized_start_ = false;

    void saveCurrentConfig() {
        if (!config_manager_) return;
        AppConfig config;
        config.source_path = source_list_->count() > 0 ? source_list_->item(0)->text().toUtf8().toStdString() : "";
        config.target_path = target_edit_->text().toUtf8().toStdString();
        config.multi_thread = multi_thread_cb_->isChecked();
        config.overwrite = overwrite_cb_->isChecked();
        config.resume = resume_cb_->isChecked();
        config.verify = verify_cb_->isChecked();
        config.speed_limit = speed_limit_cb_->isChecked();
        config.speed_limit_value = speed_spin_->value();
        config.thread_count = thread_spin_->value();
        config.auto_start = autostart_cb_->isChecked();
        if (translator_) config.language = translator_->currentLanguage().toUtf8().toStdString();
        config.minimized_start = minimized_start_;
        config_manager_->save(config);
    }

    void restoreConfig(const AppConfig& config) {
        if (!config.source_path.empty()) source_list_->addItem(QString::fromUtf8(config.source_path.c_str()));
        if (!config.target_path.empty()) target_edit_->setText(QString::fromUtf8(config.target_path.c_str()));
        multi_thread_cb_->setChecked(config.multi_thread);
        overwrite_cb_->setChecked(config.overwrite);
        resume_cb_->setChecked(config.resume);
        verify_cb_->setChecked(config.verify);
        speed_limit_cb_->setChecked(config.speed_limit);
        speed_spin_->setValue(config.speed_limit_value);
        thread_spin_->setValue(config.thread_count);
    }
};

}
