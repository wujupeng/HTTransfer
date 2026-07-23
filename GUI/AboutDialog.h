#pragma once

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include "Core/Common/VersionInfo.h"

namespace ht {

class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(tr("About Hunter Transfer"));
        setFixedSize(360, 220);

        auto* layout = new QVBoxLayout(this);

        auto* name_label = new QLabel("<h2>Hunter Transfer</h2>", this);
        name_label->setAlignment(Qt::AlignCenter);
        layout->addWidget(name_label);

        auto* ver_label = new QLabel(
            QString("Version: %1").arg(VersionInfo::version_string), this);
        ver_label->setAlignment(Qt::AlignCenter);
        layout->addWidget(ver_label);

        auto* desc_label = new QLabel(
            tr("High-performance local file copy engine\nwith multi-threading, verification and resume support."), this);
        desc_label->setAlignment(Qt::AlignCenter);
        layout->addWidget(desc_label);

        auto* copyright_label = new QLabel(
            QString::fromUtf8("\xc2\xa9 2026 Hunter Transfer Project"), this);
        copyright_label->setAlignment(Qt::AlignCenter);
        layout->addWidget(copyright_label);

        layout->addSpacing(10);

        auto* close_btn = new QPushButton(tr("OK"), this);
        close_btn->setFixedWidth(80);
        connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);
        layout->addWidget(close_btn, 0, Qt::AlignCenter);
    }
};

}