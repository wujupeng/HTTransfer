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
#include <QSettings>
#include <QApplication>
#include <QEvent>
#include <QStandardPaths>
#include <QDebug>
#include <thread>
#include <format>
#include <QMenuBar>
#include <QAction>
#include "Core/TaskManager.h"
#include "Core/Common/VersionInfo.h"
#include "AboutDialog.h"

namespace ht {

class HtTranslator : public QTranslator {
    Q_OBJECT
public:
    explicit HtTranslator(QObject* parent = nullptr) : QTranslator(parent) {}

    void setLanguage(const QString& lang) {
        translations_.clear();
        if (lang == "zh") {
            translations_ = {
                {"Hunter Transfer", "Hunter Transfer"},
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
    explicit MainWindow(std::shared_ptr<ITaskManager> task_manager, QWidget* parent = nullptr)
        : QMainWindow(parent), task_manager_(std::move(task_manager)) {
        QSettings settings("HunterTransfer", "HunterTransfer");
        QString saved_lang = settings.value("language", "en").toString();

        translator_ = new HtTranslator(this);
        translator_->setLanguage(saved_lang);
        qApp->installTranslator(translator_);

        setWindowTitle(tr("Hunter Transfer") + QString(" %1").arg(VersionInfo::version_string));
        setMinimumSize(640, 480);

        auto* central = new QWidget(this);
        setCentralWidget(central);
        auto* main_layout = new QVBoxLayout(central);

        auto* lang_widget = new QWidget(this);
        auto* lang_layout = new QHBoxLayout(lang_widget);
        lang_layout->setContentsMargins(10, 5, 10, 5);

        lang_label_ = new QLabel(tr("Language"), this);
        lang_label_->setStyleSheet("font-weight: bold; color: #333; font-size: 12px;");

        lang_combo_ = new QComboBox(this);
        lang_combo_->setMinimumWidth(120);
        lang_combo_->addItem("English", "en");
        lang_combo_->addItem("中文", "zh");
        int idx = lang_combo_->findData(saved_lang);
        if (idx >= 0) lang_combo_->setCurrentIndex(idx);
        connect(lang_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &MainWindow::onLanguageChanged);

        lang_layout->addWidget(lang_label_);
        lang_layout->addWidget(lang_combo_);
        lang_layout->addStretch();

        lang_widget->setStyleSheet("QWidget { background-color: #f0f0f0; border: 1px solid #ccc; border-radius: 5px; padding: 5px; }");
        main_layout->addWidget(lang_widget);
        main_layout->addSpacing(10);

        source_group_ = new QGroupBox(tr("Source"), this);
        auto* source_layout = new QHBoxLayout(source_group_);
        source_edit_ = new QLineEdit(this);
        source_edit_->setPlaceholderText("D:\\ERPBackup\\data.bak or D:\\ERPBackup\\");
        auto* source_file_btn = new QPushButton(tr("File"), this);
        auto* source_dir_btn = new QPushButton(tr("Directory"), this);
        source_file_btn->setFixedWidth(60);
        source_dir_btn->setFixedWidth(70);
        connect(source_file_btn, &QPushButton::clicked, this, &MainWindow::browseSourceFile);
        connect(source_dir_btn, &QPushButton::clicked, this, &MainWindow::browseSourceDir);
        source_layout->addWidget(source_edit_);
        source_layout->addWidget(source_file_btn);
        source_layout->addWidget(source_dir_btn);
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

        auto* help_menu = menuBar()->addMenu(tr("Help"));
        auto* about_action = help_menu->addAction(tr("About"));
        connect(about_action, &QAction::triggered, this, [this]() {
            AboutDialog dlg(this);
            dlg.exec();
        });

        updateButtonStates(TaskStatus::Created);

        progress_timer_ = new QTimer(this);
        connect(progress_timer_, &QTimer::timeout, this, &MainWindow::updateProgress);

        QTimer::singleShot(100, this, &MainWindow::checkRecoverableTasks);
    }

private slots:

    void onLanguageChanged(int index) {
        QString lang = lang_combo_->itemData(index).toString();
        qApp->removeTranslator(translator_);
        translator_->setLanguage(lang);
        qApp->installTranslator(translator_);

        QSettings settings("HunterTransfer", "HunterTransfer");
        settings.setValue("language", lang);

        retranslateUi();
    }

    void retranslateUi() {
        setWindowTitle(tr("Hunter Transfer") + QString(" %1").arg(VersionInfo::version_string));
        lang_label_->setText(tr("Language"));
        source_group_->setTitle(tr("Source"));
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

        QString path = QFileDialog::getOpenFileName(
            this,
            tr("Select Source File"),
            initialDir,
            filters
        );

        if (!path.isEmpty()) {
            source_edit_->setText(path);
        }
    }

    void browseSourceDir() {
        QString initialDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        QString path = QFileDialog::getExistingDirectory(
            this,
            tr("Select Source Directory"),
            initialDir
        );

        if (!path.isEmpty()) {
            source_edit_->setText(path);
        }
    }

    void browseTarget() {
        auto path = QFileDialog::getExistingDirectory(this, tr("Select Target Directory"));
        if (!path.isEmpty()) target_edit_->setText(path);
    }

    void onStart() {
        auto source = source_edit_->text().toUtf8().toStdString();
        auto target = target_edit_->text().toUtf8().toStdString();
        if (source.empty() || target.empty()) {
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

        qDebug() << "onStart: preset=" << static_cast<uint32_t>(preset)
                 << "parallelism=" << parallelism
                 << "speed_limit=" << speed_limit;

        auto result = task_manager_->createTask(source, target, preset, parallelism, speed_limit);
        if (result.isErr()) {
            QMessageBox::critical(this, tr("Error"), QString::fromStdString(result.errorMessage()));
            return;
        }

        current_task_id_ = result.value();
        updateButtonStates(TaskStatus::Queued);
        status_label_->setText(tr("Status: Starting..."));

        auto start_result = task_manager_->startTask(current_task_id_);
        if (start_result.isErr()) {
            status_label_->setText(tr("Status: Failed") + " - " + QString::fromStdString(start_result.errorMessage()));
            updateButtonStates(TaskStatus::Failed);
            return;
        }

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
    QComboBox* lang_combo_ = nullptr;
    QLabel* lang_label_ = nullptr;
    QLineEdit* source_edit_ = nullptr;
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
};

}
