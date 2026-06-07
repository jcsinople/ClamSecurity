#pragma once
#include <QObject>
#include <QDateTime>
#include <QStringList>
#include <QProcess>

class ClamAvManager : public QObject
{
    Q_OBJECT
public:
    explicit ClamAvManager(QObject *parent = nullptr);

    static bool    isInstalled();
    static QString version();
    QDateTime signatureDate() const;

    // ── clamav-daemon (scanning backend) ─────────────────────────────────
    bool isDaemonRunning() const;
    void setDaemonEnabled(bool enable);

    // ── clamav-clamonacc (on-access / real-time protection) ───────────────
    // Returns the systemd unit name found on this system, or empty if unavailable
    static QString clamonaacServiceName();
    bool isClamonaacAvailable() const;   // true if the service unit exists
    bool isClamonaacRunning() const;
    void setClamonaacEnabled(bool enable);

    // ── freshclam ─────────────────────────────────────────────────────────
    bool isFreshclamRunning() const;
    bool isFreshclamEnabled() const;   // systemctl is-enabled
    void setFreshclamEnabled(bool enable);  // pkexec enable/disable --now
    void forceUpdate();

    // ── scan exclusions ───────────────────────────────────────────────────
    QStringList exclusions() const;
    void setExclusions(const QStringList &paths);
    void addExclusion(const QString &path);
    void removeExclusion(const QString &path);

    // Extension exclusions for both manual scan and on-access
    QStringList excludedExtensions() const;
    void addExcludedExtension(const QString &ext);
    void removeExcludedExtension(const QString &ext);

signals:
    void updateOutput(const QString &line);
    void updateFinished(bool success, const QString &message);

private slots:
    void onUpdateReadyRead();
    void onUpdateFinished(int exitCode, QProcess::ExitStatus status);

private:
    static bool serviceIsActive(const QString &unitName);
    static bool serviceUnitExists(const QString &unitName);
    static void toggleService(bool enable, const QString &unitName);

    QProcess *m_updateProcess = nullptr;
};
