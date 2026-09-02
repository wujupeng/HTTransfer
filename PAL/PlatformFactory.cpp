#include "PAL/PlatformFactory.h"

#ifdef _WIN32
#include "PAL/Win32AsyncIoEngine.h"
#include "PAL/Win32FileIO.h"
#include "PAL/Win32Path.h"
#include "PAL/RegistryAutoStart.h"
#include "PAL/MutexSingleInstance.h"
#include "PAL/Win32Tray.h"
#elif defined(__linux__)
#include "PAL/IoUringEngine.h"
#include "PAL/EpollEngine.h"
#include "PAL/PosixFileIO.h"
#include "PAL/PosixPath.h"
#include "PAL/DesktopAutoStart.h"
#include "PAL/FlockSingleInstance.h"
#include "PAL/SniTray.h"
#endif

namespace ht {

std::unique_ptr<IAsyncIoEngine> PlatformFactory::createAsyncIoEngine() {
#ifdef _WIN32
    return std::make_unique<Win32AsyncIoEngine>();
#elif defined(__linux__)
    if (IoUringEngine::isAvailable()) {
        return std::make_unique<IoUringEngine>();
    }
    return std::make_unique<EpollEngine>();
#else
    #warning "Unsupported platform: AsyncIoEngine not available"
    return nullptr;
#endif
}

std::unique_ptr<IPlatformFileIO> PlatformFactory::createPlatformFileIO() {
#ifdef _WIN32
    return std::make_unique<Win32FileIO>();
#elif defined(__linux__)
    return std::make_unique<PosixFileIO>();
#else
    #warning "Unsupported platform: FileIO not available"
    return nullptr;
#endif
}

std::unique_ptr<IPlatformPath> PlatformFactory::createPlatformPath() {
#ifdef _WIN32
    return std::make_unique<Win32Path>();
#elif defined(__linux__)
    return std::make_unique<PosixPath>();
#else
    return nullptr;
#endif
}

std::unique_ptr<IPlatformAutoStart> PlatformFactory::createPlatformAutoStart() {
#ifdef _WIN32
    return std::make_unique<RegistryAutoStart>();
#elif defined(__linux__)
    return std::make_unique<DesktopAutoStart>();
#else
    return nullptr;
#endif
}

std::unique_ptr<IPlatformSingleInstance> PlatformFactory::createPlatformSingleInstance() {
#ifdef _WIN32
    return std::make_unique<MutexSingleInstance>();
#elif defined(__linux__)
    return std::make_unique<FlockSingleInstance>();
#else
    return nullptr;
#endif
}

std::unique_ptr<IPlatformTray> PlatformFactory::createPlatformTray() {
#ifdef _WIN32
    return std::make_unique<Win32Tray>();
#elif defined(__linux__)
    return std::make_unique<SniTray>();
#else
    return nullptr;
#endif
}

std::string PlatformFactory::currentPlatformName() {
#ifdef _WIN32
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

std::string PlatformFactory::currentPlatformDetail() {
#ifdef _WIN32
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

}