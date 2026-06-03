#include "AutostartManager.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QSettings>

AutostartManager::AutostartManager(QObject *parent) : QObject(parent) {}

bool AutostartManager::isAutostartEnabled() const
{
    return QFile::exists(autostartFilePath());
}

void AutostartManager::setAutostartEnabled(bool enable, bool startHidden)
{
    QString path = autostartFilePath();
    QDir dir = QFileInfo(path).dir();
    dir.mkpath(".");

    if (enable) {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream s(&f);
            s << desktopFileContent(startHidden);
        }
        QSettings settings;
        settings.setValue("autostart/startHidden", startHidden);
    } else {
        QFile::remove(path);
    }
}

bool AutostartManager::isStartHidden() const
{
    QSettings s;
    return s.value("autostart/startHidden", false).toBool();
}

QString AutostartManager::autostartFilePath() const
{
    return QDir::homePath() + "/.config/autostart/ClamSecurity.desktop";
}

QString AutostartManager::desktopFileContent(bool startHidden) const
{
    QString exec = startHidden ? "ClamSecurity --hidden" : "ClamSecurity";
    return QString(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=ClamSecurity\n"
        "Comment=Antivirus y Firewall para Linux\n"
        "Exec=%1\n"
        "Icon=security-high\n"
        "Terminal=false\n"
        "Categories=System;Security;\n"
        "X-GNOME-Autostart-enabled=true\n"
    ).arg(exec);
}
