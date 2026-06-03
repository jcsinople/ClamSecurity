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
        s.issues << tr("ClamAV no está instalado. Instálalo con: sudo pacman -S clamav");
    } else {
        s.daemonRunning = m_clam->isDaemonRunning();
        if (!s.daemonRunning)
            s.issues << tr("El servicio clamav-daemon no está activo (protección en tiempo real inactiva).");

        QDateTime sigDate = m_clam->signatureDate();
        if (sigDate.isValid()) {
            int daysSince = sigDate.daysTo(QDateTime::currentDateTime());
            s.signaturesRecent = (daysSince <= 7);
            if (!s.signaturesRecent)
                s.issues << tr("Las firmas de virus tienen más de 7 días (%1 días).").arg(daysSince);
        } else {
            s.issues << tr("No se pudo determinar la fecha de las firmas de virus.");
        }
    }

    if (UFWManager::isInstalled()) {
        s.firewallEnabled = m_ufw->isEnabled();
        if (!s.firewallEnabled)
            s.issues << tr("El firewall (UFW) no está activo.");
    }

    s.quarantineClean = (m_quar->entryCount() == 0);

    s.overallProtected = s.clamavInstalled &&
                         s.daemonRunning   &&
                         s.signaturesRecent &&
                         s.firewallEnabled;

    m_status = s;
    emit statusChanged(m_status);
}
