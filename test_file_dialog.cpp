#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    qDebug() << "Testing QFileDialog::getOpenFileName with DontUseNativeDialog";
    
    QString filters = "All Files (*.*);;Archive Files (*.zip *.7z *.tar *.gz *.rar)";
    QString path = QFileDialog::getOpenFileName(
        nullptr,
        "Select Source File",
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        filters,
        nullptr,
        QFileDialog::DontUseNativeDialog
    );
    
    qDebug() << "Selected path:" << path;
    
    if (path.isEmpty()) {
        QMessageBox::information(nullptr, "Info", "No file selected");
    } else {
        QMessageBox::information(nullptr, "Info", "Selected: " + path);
    }
    
    return 0;
}