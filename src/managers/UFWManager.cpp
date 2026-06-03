#include "UFWManager.h"
#include <QProcess>

UFWManager::UFWManager(QObject *parent) : QObject(parent) {}

bool UFWManager::isInstalled()
{
    QProcess p;
    p.start("which", {"ufw"});
    p.waitForFinished(3000);
    return p.exitCode() == 0;
}

bool UFWManager::isEnabled()
{
    QProcess p;
    p.start("ufw", {"status"});
    p.waitForFinished(5000);
    return QString::fromUtf8(p.readAllStandardOutput())
               .contains("Status: active", Qt::CaseInsensitive);
}

void UFWManager::setEnabled(bool enable)
{
    QProcess *p = new QProcess(this);
    connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            p, [this, p, enable](int code, QProcess::ExitStatus) {
        if (code == 0)
            emit statusChanged(enable);
        else if (code != 127)
            emit error(tr("Error changing firewall status (code %1).").arg(code));
        p->deleteLater();
    });
    p->start("pkexec", {"ufw", enable ? "enable" : "disable"});
}

QString UFWManager::rulesOutput()
{
    QProcess p;
    p.start("ufw", {"status", "verbose"});
    p.waitForFinished(5000);
    if (p.exitCode() != 0) {
        QProcess p2;
        p2.start("pkexec", {"ufw", "status", "verbose"});
        p2.waitForFinished(10000);
        return QString::fromUtf8(p2.readAllStandardOutput());
    }
    return QString::fromUtf8(p.readAllStandardOutput());
}

QString UFWManager::runCommand(const QStringList &args, bool needsRoot)
{
    QProcess p;
    if (needsRoot)
        p.start("pkexec", args);
    else
        p.start(args.first(), args.mid(1));
    p.waitForFinished(10000);
    return QString::fromUtf8(p.readAllStandardOutput());
}
