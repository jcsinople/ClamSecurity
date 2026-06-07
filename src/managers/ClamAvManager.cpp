#include "ClamAvManager.h"
#include <QProcess>
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>

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

// ── Helpers ───────────────────────────────────────────────────────────────────

bool ClamAvManager::serviceIsActive(const QString &unitName)
{
    QProcess p;
    p.start("systemctl", {"is-active", "--quiet", unitName});
    p.waitForFinished(3000);
    return p.exitCode() == 0;
}

bool ClamAvManager::serviceUnitExists(const QString &unitName)
{
    QProcess p;
    // "systemctl cat" exits 0 if unit file is found anywhere on the system
    p.start("systemctl", {"cat", "--quiet", unitName});
    p.waitForFinished(3000);
    return p.exitCode() == 0;
}

void ClamAvManager::toggleService(bool enable, const QString &unitName)
{
    QProcess::startDetached("pkexec",
        {"systemctl", enable ? "start" : "stop", unitName});
}

// ── clamav-daemon ─────────────────────────────────────────────────────────────

bool ClamAvManager::isDaemonRunning() const
{
    return serviceIsActive("clamav-daemon") || serviceIsActive("clamd");
}

void ClamAvManager::setDaemonEnabled(bool enable)
{
    // Try the Debian/Ubuntu name first, then the generic one
    QString svc = serviceUnitExists("clamav-daemon") ? "clamav-daemon" : "clamd";
    toggleService(enable, svc);
}

// ── clamav-clamonacc ──────────────────────────────────────────────────────────

QString ClamAvManager::clamonaacServiceName()
{
    // Check known unit names in order of preference
    static const QStringList candidates = {
        "clamav-clamonacc",
        "clamonacc"
    };
    for (const QString &name : candidates) {
        if (serviceUnitExists(name))
            return name;
    }
    return {};   // not found / not installed as a service
}

bool ClamAvManager::isClamonaacAvailable() const
{
    return !clamonaacServiceName().isEmpty();
}

bool ClamAvManager::isClamonaacRunning() const
{
    return serviceIsActive("clamav-clamonacc") || serviceIsActive("clamonacc");
}

void ClamAvManager::setClamonaacEnabled(bool enable)
{
    QString svc = clamonaacServiceName();
    if (svc.isEmpty()) return;
    toggleService(enable, svc);
}

// ── freshclam ─────────────────────────────────────────────────────────────────

bool ClamAvManager::isFreshclamRunning() const
{
    return serviceIsActive("clamav-freshclam");
}

bool ClamAvManager::isFreshclamEnabled() const
{
    QProcess p;
    p.start("systemctl", {"is-enabled", "--quiet", "clamav-freshclam"});
    p.waitForFinished(3000);
    return p.exitCode() == 0;
}

void ClamAvManager::setFreshclamEnabled(bool enable)
{
    QStringList args = {"systemctl", enable ? "enable" : "disable", "--now", "clamav-freshclam"};
    QProcess::startDetached("pkexec", args);
}

// ── signature update ──────────────────────────────────────────────────────────

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
    QString out = QString::fromUtf8(p->readAllStandardOutput()).trimmed();
    QString err = QString::fromUtf8(p->readAllStandardError()).trimmed();
    if (!out.isEmpty()) emit updateOutput(out);
    if (!err.isEmpty()) emit updateOutput(err);
}

void ClamAvManager::onUpdateFinished(int exitCode, QProcess::ExitStatus)
{
    bool success = (exitCode == 0);
    QString msg;
    if (success)
        msg = tr("Virus signatures updated successfully.");
    else if (exitCode == 127)
        msg = tr("Operation cancelled by user.");
    else
        msg = tr("Error updating signatures (code %1).").arg(exitCode);

    m_updateProcess->deleteLater();
    m_updateProcess = nullptr;
    emit updateFinished(success, msg);
}

// ── scan exclusions ───────────────────────────────────────────────────────────

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
    if (!ex.contains(path)) { ex << path; setExclusions(ex); }
}

void ClamAvManager::removeExclusion(const QString &path)
{
    QStringList ex = exclusions();
    ex.removeAll(path);
    setExclusions(ex);
}

// ── extension exclusions ──────────────────────────────────────────────────────

QStringList ClamAvManager::excludedExtensions() const
{
    QSettings s;
    return s.value("scan/extension_exclusions",
                   QStringList{".crdownload", ".part"}).toStringList();
}

void ClamAvManager::addExcludedExtension(const QString &ext)
{
    QStringList exts = excludedExtensions();
    if (!exts.contains(ext)) {
        exts << ext;
        QSettings s;
        s.setValue("scan/extension_exclusions", exts);
    }
}

void ClamAvManager::removeExcludedExtension(const QString &ext)
{
    QStringList exts = excludedExtensions();
    exts.removeAll(ext);
    QSettings s;
    s.setValue("scan/extension_exclusions", exts);
}
