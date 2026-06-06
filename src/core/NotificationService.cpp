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
        if (!line.contains("FOUND") && !line.contains("Threat found"))
            continue;

        // Format: "... some: /path/to/file: ThreatName FOUND"
        QString filePath;
        QString threat;

        // Try to parse "path: threat FOUND"
        static QRegularExpression re(R"(:\s*(/[^:]+):\s+(.+)\s+FOUND)");
        auto match = re.match(line);
        if (match.hasMatch()) {
            filePath = match.captured(1).trimmed();
            threat   = match.captured(2).trimmed();
        } else {
            // Fallback: try last colon before FOUND
            int foundIdx = line.lastIndexOf(" FOUND");
            if (foundIdx > 0) {
                QString part = line.left(foundIdx).trimmed();
                int colon = part.lastIndexOf(':');
                if (colon > 0) {
                    threat   = part.mid(colon + 1).trimmed();
                    filePath = part.left(colon).trimmed();
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
