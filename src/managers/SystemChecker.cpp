#include "SystemChecker.h"
#include "ClamAvManager.h"
#include "UFWManager.h"
#include "QuarantineManager.h"
#include <QDateTime>

SystemChecker::SystemChecker(ClamAvManager *clam,
                             UFWManager    *ufw,
                             QuarantineManager *quar,
                             QObject *parent)
    : QObject(parent), m_clam(clam), m_ufw(ufw), m_quar(quar)
{
    startTimer();
    refresh();
}

void SystemChecker::startTimer()
{
    connect(&m_timer, &QTimer::timeout, this, &SystemChecker::refresh);
    m_timer.start(60000);
}

void SystemChecker::refresh()
{
    SystemStatus s;

    s.clamavInstalled = ClamAvManager::isInstalled();
    if (!s.clamavInstalled) {
        s.issues << tr("ClamAV is not installed. Install with: sudo pacman -S clamav");
    } else {
        s.daemonRunning = m_clam->isDaemonRunning();
        if (!s.daemonRunning)
            s.issues << tr("The clamav-daemon service is not active (real-time protection inactive).");

        QDateTime sigDate = m_clam->signatureDate();
        if (sigDate.isValid()) {
            int daysSince = sigDate.daysTo(QDateTime::currentDateTime());
            s.signaturesRecent = (daysSince <= 7);
            if (!s.signaturesRecent)
                s.issues << tr("Virus signatures are more than 7 days old (%1 days).").arg(daysSince);
        } else {
            s.issues << tr("Could not determine virus signature date.");
        }
    }

    if (UFWManager::isInstalled()) {
        s.firewallEnabled = m_ufw->isEnabled();
        if (!s.firewallEnabled)
            s.issues << tr("The firewall (UFW) is not active.");
    }

    s.quarantineClean  = (m_quar->entryCount() == 0);
    s.overallProtected = s.clamavInstalled &&
                         s.daemonRunning   &&
                         s.signaturesRecent &&
                         s.firewallEnabled;

    m_status = s;
    emit statusChanged(m_status);
}
