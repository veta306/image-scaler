#include <QApplication>
#include <QStyleFactory>
#include "MainWindow.hpp"

int main(int argc, char* argv[]) {
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication app(argc, argv);
    
    MainWindow window;
    window.showMaximized();
    
    return app.exec();
}
