#pragma once

#include <QSettings>
#include <QString>
#include "Config/AppConfig.h"

namespace ht {

class AppConfigManager {
public:
    AppConfigManager() : settings_("HTTransfer", "HTTransfer") {}

    AppConfig load() {
        AppConfig config;
        config.source_path = settings_.value("source_path", "").toString().toUtf8().toStdString();
        config.target_path = settings_.value("target_path", "").toString().toUtf8().toStdString();
        config.multi_thread = settings_.value("multi_thread", true).toBool();
        config.overwrite = settings_.value("overwrite", false).toBool();
        config.resume = settings_.value("resume", true).toBool();
        config.verify = settings_.value("verify", true).toBool();
        config.speed_limit = settings_.value("speed_limit", false).toBool();
        config.speed_limit_value = settings_.value("speed_limit_value", 100).toInt();
        config.thread_count = settings_.value("thread_count", 4).toInt();
        config.auto_start = settings_.value("auto_start", false).toBool();
        config.language = settings_.value("language", "en").toString().toUtf8().toStdString();
        config.minimized_start = settings_.value("minimized_start", false).toBool();
        return config;
    }

    void save(const AppConfig& config) {
        settings_.setValue("source_path", QString::fromUtf8(config.source_path.c_str()));
        settings_.setValue("target_path", QString::fromUtf8(config.target_path.c_str()));
        settings_.setValue("multi_thread", config.multi_thread);
        settings_.setValue("overwrite", config.overwrite);
        settings_.setValue("resume", config.resume);
        settings_.setValue("verify", config.verify);
        settings_.setValue("speed_limit", config.speed_limit);
        settings_.setValue("speed_limit_value", config.speed_limit_value);
        settings_.setValue("thread_count", config.thread_count);
        settings_.setValue("auto_start", config.auto_start);
        settings_.setValue("language", QString::fromUtf8(config.language.c_str()));
        settings_.setValue("minimized_start", config.minimized_start);
    }

    void saveLanguage(const std::string& lang) {
        settings_.setValue("language", QString::fromUtf8(lang.c_str()));
    }

    std::string loadLanguage() {
        return settings_.value("language", "en").toString().toUtf8().toStdString();
    }

private:
    QSettings settings_;
};

}