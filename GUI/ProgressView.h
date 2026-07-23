#pragma once

#include <QWidget>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include "Core/TaskManager.h"

namespace ht {

class ProgressView : public QWidget {
    Q_OBJECT
public:
    explicit ProgressView(QWidget* parent = nullptr) : QWidget(parent) {
        auto* layout = new QVBoxLayout(this);
        progress_bar_ = new QProgressBar(this);
        progress_bar_->setRange(0, 100);
        speed_label_ = new QLabel("Speed: -- MB/s", this);
        avg_speed_label_ = new QLabel("Average: -- MB/s", this);
        remaining_label_ = new QLabel("Remaining: --", this);
        transferred_label_ = new QLabel("Transferred: -- / --", this);
        layout->addWidget(progress_bar_);
        layout->addWidget(speed_label_);
        layout->addWidget(avg_speed_label_);
        layout->addWidget(remaining_label_);
        layout->addWidget(transferred_label_);
    }

    void updateProgress(const TaskProgress& progress) {
        int percent = progress.total_bytes > 0
            ? static_cast<int>(progress.transferred_bytes * 100 / progress.total_bytes)
            : 0;
        progress_bar_->setValue(percent);
        speed_label_->setText(QString("Speed: %1 MB/s").arg(progress.speed_mbps, 0, 'f', 1));
        avg_speed_label_->setText(QString("Average: %1 MB/s").arg(progress.average_speed_mbps, 0, 'f', 1));
        remaining_label_->setText(QString("Remaining: %1s").arg(progress.estimated_remaining.count()));
        transferred_label_->setText(QString("Transferred: %1 / %2 MB")
            .arg(progress.transferred_bytes / 1048576.0, 0, 'f', 1)
            .arg(progress.total_bytes / 1048576.0, 0, 'f', 1));
    }

private:
    QProgressBar* progress_bar_ = nullptr;
    QLabel* speed_label_ = nullptr;
    QLabel* avg_speed_label_ = nullptr;
    QLabel* remaining_label_ = nullptr;
    QLabel* transferred_label_ = nullptr;
};

}