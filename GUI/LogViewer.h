#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QDateTimeEdit>
#include <QLineEdit>
#include <QComboBox>
#include "Logger/ILogger.h"

namespace ht {

class LogViewer : public QWidget {
    Q_OBJECT
public:
    explicit LogViewer(std::shared_ptr<ILogger> logger, QWidget* parent = nullptr)
        : QWidget(parent), logger_(std::move(logger)) {
        auto* layout = new QVBoxLayout(this);
        table_ = new QTableWidget(this);
        table_->setColumnCount(8);
        table_->setHorizontalHeaderLabels(
            {"Task ID", "Source", "Target", "User", "Result", "Speed", "Hash", "Reason"});
        table_->horizontalHeader()->setStretchLastSection(true);
        layout->addWidget(table_);
    }

    void refresh() {
        if (!logger_) return;
        AuditLogQuery query;
        auto result = logger_->queryAuditLogs(query);
        if (result.isErr()) return;

        table_->setRowCount(0);
        for (const auto& log : result.value()) {
            int row = table_->rowCount();
            table_->insertRow(row);
            table_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(log.task_id)));
            table_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(log.source_path)));
            table_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(log.target_path)));
            table_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(log.username)));
            table_->setItem(row, 4, new QTableWidgetItem(
                log.result == AuditResult::Success ? "SUCCESS" : "FAILED"));
            table_->setItem(row, 5, new QTableWidgetItem(
                QString::number(log.speed_average, 'f', 1) + " MB/s"));
            table_->setItem(row, 6, new QTableWidgetItem(QString::fromStdString(log.end_hash)));
            table_->setItem(row, 7, new QTableWidgetItem(QString::fromStdString(log.failure_reason)));
        }
    }

private:
    std::shared_ptr<ILogger> logger_;
    QTableWidget* table_ = nullptr;
};

}