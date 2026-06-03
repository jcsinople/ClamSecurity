#include "ClamAvManager.h"
#include <QProcess>
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QStandardPaths>

ClamAvManager::ClamAvManager(QObject *parent) : QObject(parent) {}

bool ClamAvManager::isInstalled()
{
    QProcess p;
    p.start("which", {"clamscan"});
    p.waitForFinished(3000);
    return p.exitCode() == 0;
}

QString ClamAvManager::version()
{
    QProcess p;
    p.start("clamscan", {"--version"});
    p.waitForFinished(3000);
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed().split('\n').first();
}

QDateTime ClamAvManager::signatureDate() const
{
    const QStringList candidates = {
        "/var/lib/clamav/daily.cld",
        "/var/lib/clamav/daily.cvd",
        "/var/lib/clamav/main.cld",
        "/var/lib/clamav/main.cvd"
    };
    QDateTime latest;
    for (const QString &path : candidates) {
        QFileInfo fi(path);
        if (fi.exists()) {
            QDateTime t = fi.lastModified();
            if (!latest.isValid() || t > latest)
                latest = t;
        }
    }
    return latest;
}

bool ClamAvManager::isDaemonRunning() const
{
    QProcess p;
    p.start("systemctl", {"is-active", "--quiet", "clamav-daemon"});
    p.waitForFinished(3000);
    if (p.exitCode() == 0) return true;

    // Fallback: check clamd.service
    QProcess p2;
    p2.start("systemctl", {"is-active", "--quiet", "clamd"});
    p2.waitForFinished(3000);
    return p2.exitCode() == 0;
}

void ClamAvManager::setDaemonEnabled(bool enable)
{
    const QString action = enable ? "start" : "stop";
    QProcess::startDetached("pkexec",
        {"systemctl", action, "clamav-daemon"});
}

bool ClamAvManager::isFreshclamRunning() const
{
    QProcess p;
    p.start("systemctl", {"is-active", "--quiet", "clamav-freshclam"});
    p.waitForFinished(3000);
    return p.exitCode() == 0;
}

QStringList ClamAvManager::exclusions() const
{
    QSettings s;
    return s.value("scan/exclusions", QStringList()).toStringList();
}

void ClamAvManager::setExclusions(const QStringList &paths)
{
    QSettings s;
    s.setValue("scan/exclusions", paths);
}

void ClamAvManager::addExclusion(const QString &path)
{
    QStringList ex = exclusions();
    if (!ex.contains(path)) {
        ex << path;
        setExclusions(ex);
    }
}

void ClamAvManager::removeExclusion(const QString &path)
{
    QStringList ex = exclusions();
    ex.removeAll(path);
    setExclusions(ex);
}

void ClamAvManager::forceUpdate()
{
    if (m_updateProcess) return;

    m_updateProcess = new QProcess(this);
    connect(m_updateProcess, &QProcess::readyReadStandardOutput,
            this, &ClamAvManager::onUpdateReadyRead);
    connect(m_updateProcess, &QProcess::readyReadStandardError,
            this, &ClamAvManager::onUpdateReadyRead);
    connect(m_updateProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &ClamAvManager::onUpdateFinished);

    m_updateProcess->start("pkexec", {"freshclam"});
}

void ClamAvManager::onUpdateReadyRead()
{
    QProcess *p = qobject_cast<QProcess*>(sender());
    if (!p) return;
    QString out = QString::fromUtf8(p->readAllStandardOutput());
    QString err = QString::fromUtf8(p->readAllStandardError());
    if (!out.isEmpty()) emit updateOutput(out.trimmed());
    if (!err.isEmpty()) emit updateOutput(err.trimmed());
}

void ClamAvManager::onUpdateFinished(int exitCode, QProcess::ExitStatus)
{
    QString msg;
    bool success = (exitCode == 0);
    if (success)
        msg = tr("Firmas de virus actualizadas correctamente.");
    else if (exitCode == 127)
        msg = tr("Operación cancelada por el usuario.");
    else
        msg = tr("Error al actualizar firmas (código %1).").arg(exitCode);

    m_updateProcess->deleteLater();
    m_updateProcess = nullptr;
    emit updateFinished(success, msg);
}
