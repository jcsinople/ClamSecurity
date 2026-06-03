#include "ScanWorker.h"
#include <QFileInfo>
#include <QRegularExpression>

ScanWorker::ScanWorker(const QString &path,
                       const QStringList &exclusions,
                       QObject *parent)
    : QObject(parent), m_path(path), m_exclusions(exclusions) {}

void ScanWorker::startScan()
{
    m_timer.start();
    m_process = new QProcess(this);
    m_process->setReadChannel(QProcess::StandardOutput);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &ScanWorker::onReadyRead);
    connect(m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &ScanWorker::onProcessFinished);

    QStringList args;
    args << "--recursive"
         << "--no-summary"
         << "--stdout"          // merged output to stdout
         << "--exclude-dir=^/proc"
         << "--exclude-dir=^/sys"
         << "--exclude-dir=^/dev"
         << "--exclude-dir=^/run";

    // Do NOT use --infected: we need every line to count scanned files
    // Threats show as "path: ThreatName FOUND", clean as "path: OK"

    for (const QString &excl : m_exclusions) {
        if (QFileInfo(excl).isDir())
            args << ("--exclude-dir=" + excl);
        else
            args << ("--exclude=" + QFileInfo(excl).fileName());
    }
    args << m_path;

    m_process->start("clamscan", args);

    if (!m_process->waitForStarted(5000)) {
        emit scanError(tr("Could not start clamscan. Is ClamAV installed?"));
        m_process->deleteLater();
        m_process = nullptr;
    }
}

// Called via Qt::QueuedConnection → runs in worker thread's event loop
void ScanWorker::stopScan()
{
    m_cancelled = true;
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        // onProcessFinished will handle the rest
    }
}

void ScanWorker::onReadyRead()
{
    while (m_process && m_process->canReadLine()) {
        QString line = QString::fromUtf8(m_process->readLine()).trimmed();
        if (line.isEmpty() || line.startsWith("LibClamAV")) continue;

        if (line.endsWith(" FOUND")) {
            // Format: "/path/to/file: ThreatName FOUND"
            static QRegularExpression re(R"(^(.+): (.+) FOUND$)");
            auto m = re.match(line);
            if (m.hasMatch()) {
                m_infected++;
                m_filesScanned++;
                emit threatFound(m.captured(1), m.captured(2));
            }
        } else if (line.endsWith(": OK")) {
            m_filesScanned++;
        } else if (line.contains(": ") && !line.startsWith("---")) {
            // Catch other non-OK, non-FOUND lines (errors on specific files)
            m_filesScanned++;
        } else {
            continue;
        }

        emit progressUpdate(m_filesScanned, m_infected,
                            line.section(':', 0, 0),
                            m_timer.elapsed());
    }
}

void ScanWorker::onProcessFinished(int exitCode, QProcess::ExitStatus)
{
    // exitCode 0 = clean, 1 = virus found, 2 = error
    if (exitCode == 2 && !m_cancelled)
        emit scanError(
            tr("clamscan finished with errors. "
               "Check that the path is accessible."));

    emit scanFinished(m_filesScanned, m_infected, m_cancelled.load());
    m_process->deleteLater();
    m_process = nullptr;
}
