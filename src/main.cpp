#include <QApplication>
#include <QSettings>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QLocalSocket>
#include <QLocalServer>
#include "mainwindow.h"
#include "LanguageManager.h"

// Socket name is per-user to avoid conflicts in multi-user systems
static QString ipcName()
{
    return QStringLiteral("ClamSecurity-") + QString::fromLocal8Bit(qgetenv("USER"));
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ClamSecurity");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ClamSecurity");
    app.setOrganizationDomain("clamsecurity.local");
    app.setQuitOnLastWindowClosed(false);

    // ── Language: apply before any UI is created ──────────────────────────
    // Single instance shared with MainWindow so apply() removes the right translators.
    auto *langMgr = new LanguageManager(&app, &app);
    langMgr->applyStartup();

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

    // ── Single-instance guard ──────────────────────────────────────────────
    // Try to contact an already-running instance. If one responds, tell it to
    // show its window and exit immediately — don't start a second copy.
    {
        QLocalSocket probe;
        probe.connectToServer(ipcName());
        if (probe.waitForConnected(300)) {
            probe.write("show\n");
            probe.flush();
            probe.waitForBytesWritten(500);
            return 0;
        }
    }
    // No running instance — become the server.
    QLocalServer::removeServer(ipcName()); // clean up any stale socket file
    auto *ipcServer = new QLocalServer(&app);
    ipcServer->listen(ipcName());

    // ── Main window ────────────────────────────────────────────────────────
    MainWindow win(langMgr);

    // Raise existing window when a second launch attempt arrives
    QObject::connect(ipcServer, &QLocalServer::newConnection, [&]() {
        auto *client = ipcServer->nextPendingConnection();
        client->deleteLater();
        win.showAndRaise();
    });

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
