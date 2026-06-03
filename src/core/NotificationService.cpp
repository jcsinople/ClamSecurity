#include "NotificationService.h"
#include <QDBusInterface>
#include <QDBusArgument>
#include <QVariantMap>

NotificationService::NotificationService(QObject *parent) : QObject(parent) {}

NotificationService::~NotificationService()
{
    stopLogWatcher();
}

void NotificationService::send(const QString &summary,
                                const QString &body,
                                const QString &icon,
                                int urgency)
{
    QDBusInterface iface(
        "org.freedesktop.Notifications",
        "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications"
    );

    if (!iface.isValid()) return;

    QVariantMap hints;
    hints["urgency"] = static_cast<quint8>(urgency);

    iface.call(
        "Notify",
        QString("ClamSecurity"),        // app name
        static_cast<quint32>(0),        // replace id
        icon,                           // icon
        summary,                        // summary
        body,                           // body
        QStringList(),                  // actions
        hints,                          // hints
        static_cast<qint32>(5000)       // timeout ms
    );
}

void NotificationService::startLogWatcher()
{
    if (m_journalProcess) return;

    m_journalProcess = new QProcess(this);
    connect(m_journalProcess, &QProcess::readyReadStandardOutput,
            this, &NotificationService::onJournalOutput);

    // Follow clamav-daemon logs in real-time, only new entries (-n 0)
    m_journalProcess->start("journalctl",
        {"-fu", "clamav-daemon", "-n", "0", "--no-pager"});
}

void NotificationService::stopLogWatcher()
{
    if (m_journalProcess) {
        m_journalProcess->terminate();
        m_journalProcess->waitForFinished(2000);
        m_journalProcess->deleteLater();
        m_journalProcess = nullptr;
    }
}

void NotificationService::onJournalOutput()
{
    while (m_journalProcess->canReadLine()) {
        QString line = QString::fromUtf8(m_journalProcess->readLine()).trimmed();
        // ClamAV on-access logs: "Access denied (Verdict: Threat found)"
        // or "fd[N]: {Threat name} FOUND"
        if (line.contains("FOUND") || line.contains("Threat found")) {
            QString threat;
            int idx = line.lastIndexOf("FOUND");
            if (idx > 0) {
                // Try to extract "path: ThreatName FOUND"
                QString part = line.left(idx).trimmed();
                int colon = part.lastIndexOf(':');
                if (colon > 0) threat = part.mid(colon + 1).trimmed();
            }
            emit threatDetectedInLog("(en tiempo real)", threat);
            send(tr("¡Amenaza detectada!"),
                 tr("ClamAV bloqueó: %1").arg(threat.isEmpty() ? line : threat),
                 "security-low", 2);
        }
    }
}
