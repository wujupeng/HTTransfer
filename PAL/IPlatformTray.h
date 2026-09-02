#pragma once

#include <string>

namespace ht {

enum class DisplayServer {
    Unknown,
    Wayland,
    X11,
};

class IPlatformTray {
public:
    virtual ~IPlatformTray() = default;

    virtual bool isAvailable() const = 0;
    virtual DisplayServer detectDisplayServer() const = 0;
    virtual void showIcon() = 0;
    virtual void hideIcon() = 0;
    virtual void showMessage(const std::string& title, const std::string& message) = 0;
};

}