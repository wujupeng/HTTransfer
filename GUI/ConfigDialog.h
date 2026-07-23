#pragma once

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include "Config/ConfigManager.h"

namespace ht {

class ConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConfigDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("Configuration");
        setMinimumSize(400, 300);
    }
};

}