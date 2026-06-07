#include "ScanWorker.h"
#include <QFileInfo>
#include <QRegularExpression>

ScanWorker::ScanWorker(const QString &path,
                       const QStringList &exclusions,
                       const QStringList &extensionExclusions,
                       QObject *parent)
    : QObject(parent), m_path(path), m_exclusions(exclusions),
      m_extensionExclusions(extensionExclusions) {}

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
         << "--stdout"
         << "--exclude-dir=^/proc"
         << "--exclude-dir=^/sys"
         << "--exclude-dir=^/dev"
         << "--exclude-dir=^/run";

    for (const QString &excl : m_exclusions) {
        if (QFileInfo(excl).isDir())
            args << ("--exclude-dir=" + excl);
        else
            args << ("--exclude=^" + QRegularExpression::escape(excl) + "$");
    }

    for (const QString &ext : m_extensionExclusions) {
        QString pattern = ext.startsWith('.') ? "*" + ext : "*." + ext;
        args << ("--exclude=" + pattern);
    }

    args << m_path;

    m_process->start("clamscan", args);

    if (!m_process->waitForStarted(5000)) {
        emit scanError(tr("Could not start clamscan. Is ClamAV installed?"));
        m_process->deleteLater();
        m_process = nullptr;
    }
}

void ScanWorker::stopScan()
{
    m_cancelled = true;
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
    }
}

void ScanWorker::onReadyRead()
{
    while (m_process && m_process->canReadLine()) {
        QString line = QString::fromUtf8(m_process->readLine()).trimmed();
        if (line.isEmpty() || line.startsWith("LibClamAV") ||
            line.startsWith("Scanning "))
            continue;

        // Filter access-denied errors produced when OnAccessPrevention is active.
        // These are not real scan errors — clamd temporarily blocks the file open
        // and then permits it, but clamscan sometimes logs the transient denial.
        if (line.contains("Permission denied") ||
            line.contains("Can't open file") ||
            line.contains("Access denied") ||
            line.contains("Operation not permitted"))
            continue;

        if (line.endsWith(" FOUND")) {
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
    if (exitCode == 2 && !m_cancelled)
        emit scanError(tr("clamscan finished with errors. "
                          "Check that the path is accessible."));

    emit scanFinished(m_filesScanned, m_infected, m_cancelled.load());
    m_process->deleteLater();
    m_process = nullptr;
}
