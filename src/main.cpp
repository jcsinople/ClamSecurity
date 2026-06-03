#include <QApplication>
#include <QSettings>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include "mainwindow.h"
#include "LanguageManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ClamSecurity");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ClamSecurity");
    app.setOrganizationDomain("clamsecurity.local");
    app.setQuitOnLastWindowClosed(false);

    // ── Language: apply before any UI is created ──────────────────────────
    // LanguageManager is destroyed with app, MainWindow gets its own instance.
    // We use a temporary one here just for startup translation setup.
    LanguageManager startupLang(&app);
    startupLang.applyStartup();

    // ── Command-line arguments ─────────────────────────────────────────────
    QCommandLineParser parser;
    parser.setApplicationDescription("ClamSecurity — Antivirus & Firewall for Linux");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption scanOpt(
        "scan",
        QCoreApplication::translate("main", "Scan the specified file or directory."),
        "path");
    QCommandLineOption hiddenOpt(
        "hidden",
        QCoreApplication::translate("main", "Start minimized in the System Tray."));
    parser.addOption(scanOpt);
    parser.addOption(hiddenOpt);
    parser.process(app);

    // ── Main window ────────────────────────────────────────────────────────
    MainWindow win;

    if (parser.isSet(scanOpt)) {
        win.startScanWithPath(parser.value(scanOpt));
        win.show();
    } else if (!parser.isSet(hiddenOpt)) {
        QSettings s;
        if (!s.value("autostart/startHidden", false).toBool())
            win.show();
    }

    return app.exec();
}
