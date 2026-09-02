#pragma once

#include "PAL/IPlatformTray.h"

namespace ht {

class Win32Tray : public IPlatformTray {
public:
    bool isAvailable() const override { return true; }

    DisplayServer detectDisplayServer() const override {
        return DisplayServer::Unknown;
    }

    void showIcon() override {}
    void hideIcon() override {}
    void showMessage(const std::string&, const std::string&) override {}
};

}