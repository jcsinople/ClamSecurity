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
         << "--stdout";

    // Built-in exclusions for virtual/special filesystems
    args << "--exclude-dir=^/proc"
         << "--exclude-dir=^/sys"
         << "--exclude-dir=^/dev"
         << "--exclude-dir=^/run";

    for (const QString &excl : m_exclusions) {
        QFileInfo fi(excl);
        if (fi.isDir())
            args << ("--exclude-dir=" + excl);
        else
            args << ("--exclude=" + QFileInfo(excl).fileName());
    }

    args << m_path;
    m_process->start("clamscan", args);

    if (!m_process->waitForStarted(5000)) {
        emit scanError(tr("No se pudo iniciar clamscan. ¿Está instalado ClamAV?"));
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

        // Format: "/path/to/file: ThreatName FOUND"
        if (line.endsWith(" FOUND")) {
            static QRegularExpression re("^(.+): (.+) FOUND$");
            QRegularExpressionMatch m = re.match(line);
            if (m.hasMatch()) {
                QString filePath  = m.captured(1);
                QString threatName = m.captured(2);
                m_infected++;
                emit threatFound(filePath, threatName);
            }
        } else if (line.contains(':') && !line.startsWith("LibClamAV")) {
            m_filesScanned++;
            emit progressUpdate(m_filesScanned, line.section(':', 0, 0));
        }
    }
}

void ScanWorker::onProcessFinished(int exitCode, QProcess::ExitStatus)
{
    // clamscan exit codes: 0 = clean, 1 = virus found, 2 = error
    if (exitCode == 2 && !m_cancelled)
        emit scanError(tr("clamscan terminó con errores. Revisa que el archivo/directorio sea accesible."));

    emit scanFinished(m_filesScanned, m_infected, m_cancelled);
    m_process->deleteLater();
    m_process = nullptr;
}
