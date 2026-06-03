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
    connect(&m_timer, &QTimer::timeout, this, &SystemChecker::refresh);
    m_timer.start(60000);
    refresh();
}

void SystemChecker::refresh()
{
    SystemStatus s;

    s.clamavInstalled = ClamAvManager::isInstalled();

    if (!s.clamavInstalled) {
        s.issues << tr("ClamAV is not installed. Install with: sudo pacman -S clamav");
    } else {
        // ── Daemon (on-demand scanning backend) ──────────────────────────
        s.daemonRunning = m_clam->isDaemonRunning();
        if (!s.daemonRunning)
            s.issues << tr("clamav-daemon is not running "
                           "(needed for scanning and real-time protection).");

        // ── ClamOnAcc (on-access / real-time protection) ─────────────────
        s.realtimeAvailable = m_clam->isClamonaacAvailable();
        if (s.realtimeAvailable) {
            s.realtimeRunning = m_clam->isClamonaacRunning();
            if (!s.realtimeRunning)
                s.issues << tr("Real-time protection (clamav-clamonacc) is not active.");
        }
        // If clamonacc is not available as a service, don't add an issue —
        // it means the user hasn't configured on-access scanning yet.

        // ── Signatures ───────────────────────────────────────────────────
        QDateTime sigDate = m_clam->signatureDate();
        if (sigDate.isValid()) {
            int days = sigDate.daysTo(QDateTime::currentDateTime());
            s.signaturesRecent = (days <= 7);
            if (!s.signaturesRecent)
                s.issues << tr("Virus signatures are more than 7 days old (%1 days).").arg(days);
        } else {
            s.issues << tr("Could not determine virus signature date.");
        }
    }

    // ── Firewall ─────────────────────────────────────────────────────────
    if (UFWManager::isInstalled()) {
        s.firewallEnabled = m_ufw->isEnabled();
        if (!s.firewallEnabled)
            s.issues << tr("The firewall (UFW) is not active.");
    }

    s.quarantineClean = (m_quar->entryCount() == 0);

    // Overall: require daemon + signatures + firewall at minimum
    // clamonacc is desirable but not blocking
    s.overallProtected = s.clamavInstalled &&
                         s.daemonRunning   &&
                         s.signaturesRecent &&
                         s.firewallEnabled;

    m_status = s;
    emit statusChanged(m_status);
}
