#include <QApplication>
#include <QSettings>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ClamSecurity");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ClamSecurity");
    app.setOrganizationDomain("clamsecurity.local");
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "ClamSecurity — Antivirus y Firewall para Linux");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption scanOpt(
        "scan",
        "Escanear el archivo o directorio especificado.",
        "path");
    QCommandLineOption hiddenOpt(
        "hidden",
        "Iniciar minimizado en el System Tray.");
    parser.addOption(scanOpt);
    parser.addOption(hiddenOpt);
    parser.process(app);

    MainWindow win;

    if (parser.isSet(scanOpt)) {
        win.startScanWithPath(parser.value(scanOpt));
        win.show();
    } else if (!parser.isSet(hiddenOpt)) {
        QSettings s;
        bool startHidden = s.value("autostart/startHidden", false).toBool();
        if (!startHidden)
            win.show();
    }

    return app.exec();
}
