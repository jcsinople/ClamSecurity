#pragma once
#include <QObject>
#include <QTimer>
#include <QStringList>

struct SystemStatus {
    bool clamavInstalled   = false;
    bool daemonRunning     = false;   // clamav-daemon (scanning backend)
    bool realtimeRunning   = false;   // clamav-clamonacc (on-access protection)
    bool realtimeAvailable = false;   // clamonacc service unit exists on this system
    bool signaturesRecent  = false;
    bool firewallEnabled   = false;
    bool quarantineClean   = true;
    bool overallProtected  = false;
    QStringList issues;
};

class ClamAvManager;
class UFWManager;
class QuarantineManager;

class SystemChecker : public QObject
{
    Q_OBJECT
public:
    explicit SystemChecker(ClamAvManager     *clam,
                           UFWManager        *ufw,
                           QuarantineManager *quar,
                           QObject *parent = nullptr);

    SystemStatus currentStatus() const { return m_status; }
    void refresh();

signals:
    void statusChanged(const SystemStatus &status);

private:
    ClamAvManager     *m_clam;
    UFWManager        *m_ufw;
    QuarantineManager *m_quar;
    SystemStatus       m_status;
    QTimer             m_timer;
};
