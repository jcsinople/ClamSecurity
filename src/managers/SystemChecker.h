#pragma once
#include <QObject>
#include <QTimer>
#include <QStringList>

enum class ProtectionLevel { Protected, Warning, Alert };

struct SystemStatus {
    bool clamavInstalled          = false;
    bool daemonRunning            = false;
    bool realtimeRunning          = false;
    bool realtimeAvailable        = false;
    bool signaturesRecent         = false;
    bool firewallEnabled          = false;
    bool quarantineClean          = true;
    bool hasActiveThreats         = false;
    bool onAccessPreventionEnabled = false;
    ProtectionLevel level         = ProtectionLevel::Alert;
    bool overallProtected         = false;  // true only when level == Protected
    QStringList issues;
};

class ClamAvManager;
class UFWManager;
class QuarantineManager;
class ClamdConfigManager;

class SystemChecker : public QObject
{
    Q_OBJECT
public:
    explicit SystemChecker(ClamAvManager      *clam,
                           UFWManager         *ufw,
                           QuarantineManager  *quar,
                           ClamdConfigManager *cfgMgr,
                           QObject *parent = nullptr);

    SystemStatus currentStatus() const { return m_status; }
    void refresh();

signals:
    void statusChanged(const SystemStatus &status);

private:
    ClamAvManager      *m_clam;
    UFWManager         *m_ufw;
    QuarantineManager  *m_quar;
    ClamdConfigManager *m_cfgMgr;
    SystemStatus        m_status;
    QTimer              m_timer;
};
