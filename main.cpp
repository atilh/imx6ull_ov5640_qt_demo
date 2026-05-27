#include "offlinevisiontest.h"
#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const QStringList arguments = app.arguments();
    if (arguments.size() > 1 && arguments.at(1) == QStringLiteral("--offline-test")) {
        return runOfflineVisionTest(arguments);
    }

    MainWindow window;
    window.show();
    return app.exec();
}
