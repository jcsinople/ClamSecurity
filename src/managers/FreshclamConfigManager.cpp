#include "FreshclamConfigManager.h"
#include <QFile>
#include <QTextStream>
#include <QTemporaryFile>
#include <QRegularExpression>

QString FreshclamConfigManager::configFilePath()
{
    return "/etc/clamav/freshclam.conf";
}

FreshclamConfigManager::FreshclamConfigManager(QObject *parent)
    : QObject(parent) {}

int FreshclamConfigManager::readChecks() const
{
    QFile f(configFilePath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return 12; // default: every 2 hours

    static QRegularExpression re(R"(^\s*Checks\s+(\d+)\s*$)");
    int last = 12;
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine();
        auto m = re.match(line);
        if (m.hasMatch())
            last = m.captured(1).toInt();
    }
    return qBound(1, last, 24);
}

void FreshclamConfigManager::saveChecks(int checks)
{
    if (m_copyProcess) return;  // already saving

    checks = qBound(1, checks, 24);

    QFile src(configFilePath());
    QString original;
    if (src.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&src);
        original = in.readAll();
        src.close();
    }

    // Strip our previously managed block (from marker to end of file)
    int markerPos = original.indexOf(MARKER);
    if (markerPos != -1)
        original = original.left(markerPos).trimmed();

    // Append managed block at the end
    QString managed = QString("\n\n%1\nChecks %2\n").arg(MARKER).arg(checks);
    QString updated = original + managed;

    // Write to a temp file
    QTemporaryFile tmp;
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        emit configSaved(false, tr("Could not create temporary file."));
        return;
    }
    m_tempPath = tmp.fileName();
    QTextStream out(&tmp);
    out << updated;
    tmp.close();

    // Copy over system file using pkexec
    m_copyProcess = new QProcess(this);
    connect(m_copyProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &FreshclamConfigManager::onCopyFinished);
    m_copyProcess->start("pkexec", {"cp", m_tempPath, configFilePath()});
}

void FreshclamConfigManager::onCopyFinished(int exitCode, QProcess::ExitStatus)
{
    QFile::remove(m_tempPath);
    m_tempPath.clear();
    m_copyProcess->deleteLater();
    m_copyProcess = nullptr;

    if (exitCode == 0)
        emit configSaved(true,  tr("Automatic update settings saved."));
    else if (exitCode == 126 || exitCode == 127)
        emit configSaved(false, tr("Operation cancelled by user."));
    else
        emit configSaved(false, tr("Failed to save settings (code %1).").arg(exitCode));
}
