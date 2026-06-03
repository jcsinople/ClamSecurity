#include "NotificationService.h"
#include <QDBusInterface>
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

    iface.call("Notify",
        QString("ClamSecurity"),
        static_cast<quint32>(0),
        icon,
        summary,
        body,
        QStringList(),
        hints,
        static_cast<qint32>(5000));
}

void NotificationService::startLogWatcher()
{
    if (m_journalProcess) return;

    m_journalProcess = new QProcess(this);
    connect(m_journalProcess, &QProcess::readyReadStandardOutput,
            this, &NotificationService::onJournalOutput);
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
        if (line.contains("FOUND") || line.contains("Threat found")) {
            QString threat;
            int idx = line.lastIndexOf("FOUND");
            if (idx > 0) {
                QString part = line.left(idx).trimmed();
                int colon = part.lastIndexOf(':');
                if (colon > 0) threat = part.mid(colon + 1).trimmed();
            }
            emit threatDetectedInLog("(real-time)", threat);
            send(tr("Threat detected!"),
                 tr("ClamAV blocked: %1").arg(threat.isEmpty() ? line : threat),
                 "security-low", 2);
        }
    }
}
