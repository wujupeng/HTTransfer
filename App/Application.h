#pragma once

#include <QApplication>
#include <memory>
#include "Core/TaskManager.h"
#include "Core/FileEngine.h"
#include "Core/SpeedController.h"
#include "Core/BufferPool.h"
#include "Core/IOCDispatcher.h"
#include "Core/Common/VersionInfo.h"
#include "Verify/VerifyEngine.h"
#include "Resume/ResumeEngine.h"
#include "Transfer/TransferEngine.h"
#include "Logger/Logger.h"
#include "Config/ConfigManager.h"
#include "Watch/WatchSession.h"
#include "GUI/MainWindow.h"

namespace ht {

class Application {
public:
    Application(int& argc, char** argv)
        : qt_app_(argc, argv) {
        for (int i = 1; i < argc; ++i) {
            std::string_view arg(argv[i]);
            if (arg == "--minimized") minimized_ = true;
        }

        auto buffer_pool = std::make_shared<BufferPool>();
        auto iocp = std::make_shared<IOCDispatcher>();
        iocp->initialize(kDefaultParallelism);

        auto verify_engine = std::make_shared<VerifyEngine>();
        auto logger = std::make_shared<Logger>();
        auto resume_engine = std::make_shared<ResumeEngine>(logger);
        auto transfer_engine = std::make_shared<TransferEngine>(logger, buffer_pool, resume_engine);

        auto smb_adapter = std::make_unique<SMBAdapter>();
        transfer_engine->registerAdapter(ProtocolType::SMB, std::move(smb_adapter));

        auto file_engine = std::make_shared<FileEngine>(buffer_pool, iocp, verify_engine);
        auto speed_controller = std::make_shared<SpeedController>();
        transfer_engine->setSpeedController(speed_controller);
        auto config_manager = std::make_shared<ConfigManager>();

        auto task_manager = std::make_shared<TaskManager>(
            file_engine, verify_engine, resume_engine,
            transfer_engine, speed_controller, logger);

        task_manager->recoverFromCrash();

        auto watch_session = std::make_shared<WatchSession>(task_manager, file_engine, logger);

        logger->log(ILogger::Level::Info, "SYSTEM",
            std::string("HunterTransfer started - ") + VersionInfo::version_full);

        main_window_ = std::make_unique<MainWindow>(task_manager, watch_session, minimized_);
    }

    int run() {
        if (!minimized_) main_window_->show();
        return qt_app_.exec();
    }

private:
    QApplication qt_app_;
    std::unique_ptr<MainWindow> main_window_;
    bool minimized_ = false;
};

}
