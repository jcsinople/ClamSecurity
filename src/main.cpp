#include <QApplication>
#include <QSettings>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QLocalSocket>
#include <QLocalServer>
#ifdef HAVE_KF6WINDOWSYSTEM
#  include <KWindowSystem>
#endif
#include "mainwindow.h"
#include "LanguageManager.h"

// Socket name is per-user to avoid conflicts in multi-user systems
static QString ipcName()
{
    return QStringLiteral("ClamSecurity-") + QString::fromLocal8Bit(qgetenv("USER"));
}

// Raise and focus the window properly on KDE Wayland.
// Without KWindowSystem, activateWindow() only blinks the taskbar entry
// because Wayland requires an activation token from a user-gesture context.
// KWindowSystem::activateWindow() handles token management on KDE Plasma.
static void raiseAndActivate(QMainWindow &win, const QString &activationToken)
{
#ifdef HAVE_KF6WINDOWSYSTEM
    if (!activationToken.isEmpty())
        KWindowSystem::setCurrentXdgActivationToken(activationToken);
    if (QWindow *w = win.windowHandle())
        KWindowSystem::activateWindow(w);
#else
    Q_UNUSED(activationToken)
    win.show();
    win.raise();
    win.activateWindow();
#endif
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
    // Try to contact an already-running instance. If one responds, forward
    // the command AND the XDG activation token (from the user gesture in
    // Dolphin) so the existing window can raise itself properly on Wayland.
    {
        QLocalSocket probe;
        probe.connectToServer(ipcName());
        if (probe.waitForConnected(300)) {
            const QString token = QString::fromLocal8Bit(qgetenv("XDG_ACTIVATION_TOKEN"));

            QStringList parts;
            if (parser.isSet(scanOpt))
                parts << QStringLiteral("scan:%1").arg(parser.value(scanOpt));
            else
                parts << QStringLiteral("show");
            if (!token.isEmpty())
                parts << QStringLiteral("activation:%1").arg(token);

            probe.write((parts.join('\n') + '\n').toUtf8());
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

    // Handle commands forwarded from subsequent launch attempts
    QObject::connect(ipcServer, &QLocalServer::newConnection, [&]() {
        auto *client = ipcServer->nextPendingConnection();
        QObject::connect(client, &QLocalSocket::readyRead, [&win, client]() {
            const QStringList lines =
                QString::fromUtf8(client->readAll()).trimmed().split('\n');
            client->deleteLater();

            QString scanPath, activationToken;
            for (const QString &line : lines) {
                if (line.startsWith(QStringLiteral("scan:")))
                    scanPath = line.mid(5);
                else if (line.startsWith(QStringLiteral("activation:")))
                    activationToken = line.mid(11);
            }

            if (!scanPath.isEmpty())
                win.startScanWithPath(scanPath);

            // Use the token from Dolphin's user gesture to properly raise the
            // window on Wayland — without it the compositor only blinks the icon
            raiseAndActivate(win, activationToken);
        });
    });

    if (parser.isSet(scanOpt)) {
        win.startScanWithPath(parser.value(scanOpt));
        win.showAndRaise();
    } else if (!parser.isSet(hiddenOpt)) {
        QSettings s;
        if (!s.value("autostart/startHidden", false).toBool())
            win.showAndRaise();
    }

    return app.exec();
}
