#include "UFWManager.h"
#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

UFWManager::UFWManager(QObject *parent) : QObject(parent) {}

bool UFWManager::isInstalled()
{
    QProcess p;
    p.start("which", {"ufw"});
    p.waitForFinished(3000);
    return p.exitCode() == 0;
}

// Read /etc/ufw/ufw.conf — world-readable, no root needed
bool UFWManager::isEnabled() const
{
    QFile conf("/etc/ufw/ufw.conf");
    if (conf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&conf);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.startsWith('#')) continue;
            if (line.startsWith("ENABLED=", Qt::CaseInsensitive))
                return line.endsWith("yes", Qt::CaseInsensitive);
        }
    }
    return false;
}

void UFWManager::setEnabled(bool enable)
{
    QProcess *p = new QProcess(this);
    connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            p, [this, p, enable](int code, QProcess::ExitStatus) {
        if (code == 0)
            emit statusChanged(enable);
        else if (code != 127)   // 127 = user cancelled pkexec
            emit error(tr("Error changing firewall status (code %1).").arg(code));
        p->deleteLater();
    });
    p->start("pkexec", {"ufw", enable ? "enable" : "disable"});
}

void UFWManager::refreshRules()
{
    QProcess *p = new QProcess(this);
    connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            p, [this, p](int code, QProcess::ExitStatus) {
        if (code == 0) {
            QString out = QString::fromUtf8(p->readAllStandardOutput());
            emit rulesRefreshed(parseNumbered(out), out);
        } else if (code != 127) {
            emit error(tr("Could not read firewall rules (code %1).").arg(code));
        }
        p->deleteLater();
    });
    p->start("pkexec", {"ufw", "status", "numbered"});
}

void UFWManager::addRule(const QString &direction, const QString &action,
                          const QString &port,     const QString &from)
{
    QStringList args = {"ufw"};
    if (!direction.isEmpty() && direction.toLower() != "in")
        args << direction.toLower();
    args << action.toLower() << port;
    if (!from.isEmpty() && from.toLower() != "anywhere")
        args << "from" << from;

    QProcess *p = new QProcess(this);
    connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            p, [this, p](int code, QProcess::ExitStatus) {
        if (code == 0)
            refreshRules();
        else if (code != 127)
            emit error(tr("Could not add rule (code %1).").arg(code));
        p->deleteLater();
    });
    p->start("pkexec", args);
}

void UFWManager::deleteRule(int ruleNumber)
{
    QProcess *p = new QProcess(this);
    // ufw delete with --force skips the interactive "Deleting: ... (y|n)?" prompt
    connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            p, [this, p](int code, QProcess::ExitStatus) {
        if (code == 0)
            refreshRules();
        else if (code != 127)
            emit error(tr("Could not delete rule (code %1).").arg(code));
        p->deleteLater();
    });
    p->start("pkexec", {"ufw", "--force", "delete", QString::number(ruleNumber)});
}

// Parse "ufw status numbered" output
QList<UFWRule> UFWManager::parseNumbered(const QString &output)
{
    QList<UFWRule> rules;
    // Example line: "[ 1] 22/tcp                     ALLOW IN    Anywhere"
    // or:           "[ 2] Anywhere on eth0            ALLOW FWD   192.168.1.0/24"
    static QRegularExpression re(
        R"(\[\s*(\d+)\]\s+(.+?)\s{2,}(ALLOW|DENY|LIMIT|REJECT)\s*(IN|OUT|FWD)?\s+(.*?)(\s+\(v6\))?\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    for (const QString &line : output.split('\n')) {
        auto m = re.match(line.trimmed());
        if (!m.hasMatch()) continue;
        UFWRule r;
        r.number    = m.captured(1).toInt();
        r.to        = m.captured(2).trimmed();
        r.action    = m.captured(3).toUpper();
        r.direction = m.captured(4).isEmpty() ? "IN" : m.captured(4).toUpper();
        r.from      = m.captured(5).trimmed();
        r.ipv6      = !m.captured(6).isEmpty();
        rules << r;
    }
    return rules;
}
