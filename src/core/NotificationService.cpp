#include "NotificationService.h"
#include <QDBusInterface>
#include <QVariantMap>
#include <QTimer>
#include <QRegularExpression>

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
        if (!line.contains("FOUND"))
            continue;

        // clamd/clamonacc journal format:
        //   "clamd[PID]: Mon DD HH:MM:SS YYYY -> /path/to/file: ThreatName FOUND"
        // Use " -> " as anchor to extract path and threat reliably.
        QString filePath;
        QString threat;

        int arrowPos = line.lastIndexOf(" -> ");
        if (arrowPos >= 0) {
            QString rest = line.mid(arrowPos + 4).trimmed(); // "/path: Threat FOUND"
            int foundPos = rest.lastIndexOf(" FOUND");
            if (foundPos > 0) {
                QString part   = rest.left(foundPos);         // "/path: Threat"
                int colonPos   = part.lastIndexOf(": ");
                if (colonPos > 0) {
                    filePath = part.left(colonPos).trimmed();
                    threat   = part.mid(colonPos + 2).trimmed();
                }
            }
        }

        // Fallback for other formats (no " -> ")
        if (filePath.isEmpty()) {
            int foundIdx = line.lastIndexOf(" FOUND");
            if (foundIdx > 0) {
                QString part = line.left(foundIdx).trimmed();
                int colon    = part.lastIndexOf(": ");
                if (colon > 0) {
                    threat            = part.mid(colon + 2).trimmed();
                    QString pathPart  = part.left(colon).trimmed();
                    // Extract last path-like token (starts with '/')
                    int sp = pathPart.lastIndexOf(' ');
                    if (sp >= 0 && sp + 1 < pathPart.size() && pathPart.at(sp + 1) == '/')
                        filePath = pathPart.mid(sp + 1);
                    else if (pathPart.startsWith('/'))
                        filePath = pathPart;
                }
            }
        }

        if (filePath.isEmpty())
            filePath = "(real-time)";

        // Deduplication: skip if same file was seen within the last 15 seconds
        if (m_recentThreats.contains(filePath))
            continue;

        m_recentThreats.insert(filePath);
        QTimer::singleShot(15000, this, [this, filePath]() {
            m_recentThreats.remove(filePath);
        });

        // Emit only — mainwindow handles the notification to avoid duplicates
        emit threatDetectedInLog(filePath, threat);
    }
}
