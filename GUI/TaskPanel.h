#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QMenu>
#include <QVBoxLayout>
#include "Core/TaskManager.h"

namespace ht {

class TaskPanel : public QWidget {
    Q_OBJECT
public:
    explicit TaskPanel(std::shared_ptr<ITaskManager> task_manager, QWidget* parent = nullptr)
        : QWidget(parent), task_manager_(std::move(task_manager)) {
        auto* layout = new QVBoxLayout(this);
        table_ = new QTableWidget(this);
        table_->setColumnCount(5);
        table_->setHorizontalHeaderLabels({"Task ID", "Source", "Target", "Status", "Progress"});
        table_->horizontalHeader()->setStretchLastSection(true);
        table_->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(table_, &QTableWidget::customContextMenuRequested, this, &TaskPanel::showContextMenu);
        layout->addWidget(table_);
    }

    void refresh() {
        auto result = task_manager_->listTasks();
        if (result.isErr()) return;

        table_->setRowCount(0);
        for (const auto& task : result.value()) {
            int row = table_->rowCount();
            table_->insertRow(row);
            table_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(task.task_id)));
            table_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(task.source_path)));
            table_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(task.target_path)));
            table_->setItem(row, 3, new QTableWidgetItem(QString::number(static_cast<int>(task.status))));
            table_->setItem(row, 4, new QTableWidgetItem(QString::number(task.progress_percent, 'f', 1) + "%"));
        }
    }

private slots:
    void showContextMenu(const QPoint& pos) {
        QMenu menu;
        menu.addAction("Pause", [this]() {});
        menu.addAction("Resume", [this]() {});
        menu.addAction("Cancel", [this]() {});
        menu.exec(table_->mapToGlobal(pos));
    }

private:
    std::shared_ptr<ITaskManager> task_manager_;
    QTableWidget* table_ = nullptr;
};

}