#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QTimeEdit>
#include <QSpinBox>
#include "Config/ConfigManager.h"

namespace ht {

class SpeedScheduleWidget : public QWidget {
    Q_OBJECT
public:
    explicit SpeedScheduleWidget(QWidget* parent = nullptr) : QWidget(parent) {}
};

}