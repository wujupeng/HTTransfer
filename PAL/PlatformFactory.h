#pragma once

#include <memory>
#include <string>
#include "PAL/IAsyncIoEngine.h"
#include "PAL/IPlatformFileIO.h"
#include "PAL/IPlatformPath.h"
#include "PAL/IPlatformAutoStart.h"
#include "PAL/IPlatformSingleInstance.h"
#include "PAL/IPlatformTray.h"

namespace ht {

class PlatformFactory {
public:
    static std::unique_ptr<IAsyncIoEngine> createAsyncIoEngine();
    static std::unique_ptr<IPlatformFileIO> createPlatformFileIO();
    static std::unique_ptr<IPlatformPath> createPlatformPath();
    static std::unique_ptr<IPlatformAutoStart> createPlatformAutoStart();
    static std::unique_ptr<IPlatformSingleInstance> createPlatformSingleInstance();
    static std::unique_ptr<IPlatformTray> createPlatformTray();

    static std::string currentPlatformName();
    static std::string currentPlatformDetail();
};

}