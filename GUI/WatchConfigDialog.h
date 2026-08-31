#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QMessageBox>

namespace ht {

class WatchConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit WatchConfigDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(tr("Incremental Backup Settings"));
        setMinimumWidth(500);

        auto* layout = new QVBoxLayout(this);

        auto* src_group = new QGroupBox(tr("Source Directory"), this);
        auto* src_layout = new QHBoxLayout(src_group);
        src_edit_ = new QLineEdit(this);
        auto* src_btn = new QPushButton("...", this);
        src_btn->setFixedWidth(40);
        connect(src_btn, &QPushButton::clicked, this, [this]() {
            auto path = QFileDialog::getExistingDirectory(this, tr("Select Source Directory"),
                QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
            if (!path.isEmpty()) src_edit_->setText(path);
        });
        src_layout->addWidget(src_edit_);
        src_layout->addWidget(src_btn);
        layout->addWidget(src_group);

        auto* dst_group = new QGroupBox(tr("Target Directory"), this);
        auto* dst_layout = new QHBoxLayout(dst_group);
        dst_edit_ = new QLineEdit(this);
        auto* dst_btn = new QPushButton("...", this);
        dst_btn->setFixedWidth(40);
        connect(dst_btn, &QPushButton::clicked, this, [this]() {
            auto path = QFileDialog::getExistingDirectory(this, tr("Select Target Directory"));
            if (!path.isEmpty()) dst_edit_->setText(path);
        });
        dst_layout->addWidget(dst_edit_);
        dst_layout->addWidget(dst_btn);
        layout->addWidget(dst_group);

        auto* interval_group = new QGroupBox(tr("Scan Interval"), this);
        auto* interval_layout = new QHBoxLayout(interval_group);
        interval_combo_ = new QComboBox(this);
        interval_combo_->addItem(tr("5 seconds"), 5);
        interval_combo_->addItem(tr("10 seconds"), 10);
        interval_combo_->addItem(tr("Custom"), -1);
        interval_spin_ = new QSpinBox(this);
        interval_spin_->setRange(1, 3600);
        interval_spin_->setValue(30);
        interval_spin_->setSuffix(tr(" s"));
        interval_spin_->setEnabled(false);
        connect(interval_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
            interval_spin_->setEnabled(interval_combo_->itemData(idx).toInt() == -1);
        });
        interval_layout->addWidget(interval_combo_);
        interval_layout->addWidget(interval_spin_);
        layout->addWidget(interval_group);

        auto* btn_layout = new QHBoxLayout();
        start_btn_ = new QPushButton(tr("Start Backup"), this);
        cancel_btn_ = new QPushButton(tr("Cancel"), this);
        connect(start_btn_, &QPushButton::clicked, this, &WatchConfigDialog::onStartClicked);
        connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
        btn_layout->addWidget(start_btn_);
        btn_layout->addWidget(cancel_btn_);
        layout->addLayout(btn_layout);
    }

    QString sourcePath() const { return src_edit_->text(); }
    QString targetPath() const { return dst_edit_->text(); }
    int scanInterval() const {
        int val = interval_combo_->currentData().toInt();
        return val == -1 ? interval_spin_->value() : val;
    }

    void setRunningMode(bool running) {
        src_edit_->setEnabled(!running);
        dst_edit_->setEnabled(!running);
        interval_combo_->setEnabled(!running);
        interval_spin_->setEnabled(!running && interval_combo_->currentData().toInt() == -1);
        start_btn_->setEnabled(!running);
    }

signals:
    void startWatchRequested(const QString& source, const QString& target, int interval_seconds);

private slots:
    void onStartClicked() {
        if (src_edit_->text().isEmpty() || dst_edit_->text().isEmpty()) {
            QMessageBox::warning(this, tr("Error"), tr("Please specify source and target paths"));
            return;
        }
        if (src_edit_->text() == dst_edit_->text()) {
            QMessageBox::warning(this, tr("Error"), tr("Source and target must be different"));
            return;
        }
        emit startWatchRequested(src_edit_->text(), dst_edit_->text(), scanInterval());
        accept();
    }

private:
    QLineEdit* src_edit_ = nullptr;
    QLineEdit* dst_edit_ = nullptr;
    QComboBox* interval_combo_ = nullptr;
    QSpinBox* interval_spin_ = nullptr;
    QPushButton* start_btn_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
};

}