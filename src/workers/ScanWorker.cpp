#include "ScanWorker.h"
#include <QFileInfo>
#include <QRegularExpression>

ScanWorker::ScanWorker(const QString &path,
                       const QStringList &exclusions,
                       QObject *parent)
    : QObject(parent), m_path(path), m_exclusions(exclusions) {}

void ScanWorker::startScan()
{
    m_process = new QProcess(this);
    m_process->setReadChannel(QProcess::StandardOutput);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &ScanWorker::onReadyRead);
    connect(m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &ScanWorker::onProcessFinished);

    QStringList args;
    args << "--recursive"
         << "--infected"
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

void ScanWorker::stopScan()
{
    m_cancelled = true;
    if (m_process) {
        m_process->terminate();
        m_process->waitForFinished(3000);
        m_process->kill();
    }
}

void ScanWorker::onReadyRead()
{
    while (m_process && m_process->canReadLine()) {
        QString line = QString::fromUtf8(m_process->readLine()).trimmed();
        if (line.isEmpty()) continue;

        if (line.endsWith(" FOUND")) {
            static QRegularExpression re("^(.+): (.+) FOUND$");
            QRegularExpressionMatch m = re.match(line);
            if (m.hasMatch()) {
                m_infected++;
                emit threatFound(m.captured(1), m.captured(2));
            }
        } else if (line.contains(':') && !line.startsWith("LibClamAV")) {
            m_filesScanned++;
            emit progressUpdate(m_filesScanned, line.section(':', 0, 0));
        }
    }
}

void ScanWorker::onProcessFinished(int exitCode, QProcess::ExitStatus)
{
    if (exitCode == 2 && !m_cancelled)
        emit scanError(tr("clamscan finished with errors. Check that the file/directory is accessible."));

    emit scanFinished(m_filesScanned, m_infected, m_cancelled);
    m_process->deleteLater();
    m_process = nullptr;
}
