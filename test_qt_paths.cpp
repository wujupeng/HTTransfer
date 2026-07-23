#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QPluginLoader>
#include <iostream>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    qDebug() << "Application directory:" << QCoreApplication::applicationDirPath();
    qDebug() << "Library paths:" << QCoreApplication::libraryPaths();
    
    // Check qt.conf
    QFile qtConf(QCoreApplication::applicationDirPath() + "/qt.conf");
    if (qtConf.exists()) {
        qDebug() << "qt.conf exists at:" << qtConf.fileName();
        if (qtConf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "qt.conf content:" << qtConf.readAll();
            qtConf.close();
        }
    } else {
        qDebug() << "qt.conf NOT found";
    }
    
    // Check platforms plugin
    QString pluginPath = QCoreApplication::applicationDirPath() + "/../plugins/platforms/qwindows.dll";
    QFile pluginFile(pluginPath);
    qDebug() << "Plugin path:" << pluginPath;
    qDebug() << "Plugin exists:" << pluginFile.exists();
    
    // Try to load plugin
    QPluginLoader loader(pluginPath);
    qDebug() << "Plugin loadable:" << loader.isLoaded();
    if (!loader.isLoaded()) {
        qDebug() << "Error:" << loader.errorString();
    }
    
    return 0;
}