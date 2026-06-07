#include "SystemChecker.h"
#include "ClamAvManager.h"
#include "UFWManager.h"
#include "QuarantineManager.h"
#include "ClamdConfigManager.h"
#include "../ui/ActiveThreatsPage.h"
#include <QDateTime>

SystemChecker::SystemChecker(ClamAvManager      *clam,
                             UFWManager         *ufw,
                             QuarantineManager  *quar,
                             ClamdConfigManager *cfgMgr,
                             QObject *parent)
    : QObject(parent), m_clam(clam), m_ufw(ufw), m_quar(quar), m_cfgMgr(cfgMgr)
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
        // ── Daemon ───────────────────────────────────────────────────────────
        s.daemonRunning = m_clam->isDaemonRunning();
        if (!s.daemonRunning)
            s.issues << tr("clamav-daemon is not running "
                           "(needed for scanning and real-time protection).");

        // ── ClamOnAcc ────────────────────────────────────────────────────────
        s.realtimeAvailable = m_clam->isClamonaacAvailable();
        if (s.realtimeAvailable) {
            s.realtimeRunning = m_clam->isClamonaacRunning();
            if (!s.realtimeRunning)
                s.issues << tr("Real-time protection (clamav-clamonacc) is disabled.");
        }

        // ── OnAccessPrevention ───────────────────────────────────────────────
        if (s.realtimeAvailable && s.realtimeRunning) {
            s.onAccessPreventionEnabled = m_cfgMgr->readConfig().preventionEnabled;
        }

        // ── Signatures ───────────────────────────────────────────────────────
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

    // ── Firewall ─────────────────────────────────────────────────────────────
    if (UFWManager::isInstalled()) {
        s.firewallEnabled = m_ufw->isEnabled();
        if (!s.firewallEnabled)
            s.issues << tr("The firewall (UFW) is not active.");
    }

    // ── Quarantine (informational only, does not affect level) ───────────────
    s.quarantineClean = (m_quar->entryCount() == 0);

    // ── Active threats ───────────────────────────────────────────────────────
    s.hasActiveThreats = ActiveThreatsPage::pendingCount() > 0;

    // ── Compute protection level ─────────────────────────────────────────────
    s.level = ProtectionLevel::Protected;

    // Red conditions
    if (!s.clamavInstalled || !s.daemonRunning || !s.signaturesRecent || !s.firewallEnabled)
        s.level = ProtectionLevel::Alert;

    if (s.realtimeAvailable && !s.realtimeRunning)
        s.level = ProtectionLevel::Alert;

    if (s.hasActiveThreats) {
        bool oapProtecting = s.realtimeAvailable && s.realtimeRunning && s.onAccessPreventionEnabled;
        if (!oapProtecting) {
            s.level = ProtectionLevel::Alert;
            s.issues << tr("Active threats detected without OnAccessPrevention protection.");
        } else if (s.level == ProtectionLevel::Protected) {
            s.level = ProtectionLevel::Warning;
            s.issues << tr("Active threats detected (OnAccessPrevention is active).");
        }
    }

    // Yellow conditions (only if not already Alert)
    if (s.level != ProtectionLevel::Alert) {
        if (!s.realtimeAvailable) {
            s.level = ProtectionLevel::Warning;
            s.issues << tr("Real-time protection (clamav-clamonacc) is not configured.");
        } else if (s.realtimeRunning && !s.onAccessPreventionEnabled) {
            s.level = ProtectionLevel::Warning;
            s.issues << tr("OnAccessPrevention is disabled — "
                           "active files are not blocked when a threat is detected.");
        }
    }

    s.overallProtected = (s.level == ProtectionLevel::Protected);

    m_status = s;
    emit statusChanged(m_status);
}
