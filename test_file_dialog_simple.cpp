#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <iostream>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    std::cout << "Testing QFileDialog::getOpenFileName()" << std::endl;
    std::cout << "======================================" << std::endl;
    
    // 测试1：标准文件选择对话框
    QString file = QFileDialog::getOpenFileName(
        nullptr,
        "Select Source File - TEST",
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        "All Files (*.*);;Archive Files (*.zip *.rar *.7z *.tar *.gz)"
    );
    
    std::cout << "File selected: " << (file.isEmpty() ? "(empty)" : file.toStdString()) << std::endl;
    
    // 测试2：标准文件夹选择对话框
    QString dir = QFileDialog::getExistingDirectory(
        nullptr,
        "Select Source Directory - TEST",
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
    );
    
    std::cout << "Directory selected: " << (dir.isEmpty() ? "(empty)" : dir.toStdString()) << std::endl;
    
    QMessageBox::information(nullptr, "Test Result", 
        QString("File: %1\nDirectory: %2").arg(file.isEmpty() ? "None" : file).arg(dir.isEmpty() ? "None" : dir));
    
    return 0;
}