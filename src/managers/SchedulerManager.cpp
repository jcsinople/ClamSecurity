#include "SchedulerManager.h"
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QUuid>
#include <QStandardPaths>
#include <QFileInfo>
#include <algorithm>

SchedulerManager::SchedulerManager(QObject *parent) : QObject(parent) {}

// ── Static helpers ─────────────────────────────────────────────────────────────

QString SchedulerManager::systemdUserDir()
{
    return QDir::homePath() + "/.config/systemd/user";
}

QString SchedulerManager::unitName(const QString &id)
{
    return "clamsecurity-scan-" + id;
}

QString SchedulerManager::logPath(const QString &id)
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base + "/scanlogs/" + id + ".log";
}

QString SchedulerManager::calendarSpec(const ScanSchedule &s)
{
    static const QStringList days = {
        "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
    };
    // time is "HH:MM"
    QString t = s.time.isEmpty() ? "02:00" : s.time;
    if (s.frequency == "weekly") {
        QString day = (s.weekday >= 0 && s.weekday < 7) ? days[s.weekday] : "Mon";
        return QString("%1 *-*-* %2:00").arg(day, t);
    }
    if (s.frequency == "monthly")
        return QString("*-*-01 %1:00").arg(t);
    // default: daily
    return QString("*-*-* %1:00").arg(t);
}

// ── Persistence ────────────────────────────────────────────────────────────────

QList<ScanSchedule> SchedulerManager::schedules() const
{
    QSettings s;
    int count = s.beginReadArray("scheduler/schedules");
    QList<ScanSchedule> list;
    for (int i = 0; i < count; ++i) {
        s.setArrayIndex(i);
        ScanSchedule sc;
        sc.id        = s.value("id").toString();
        sc.name      = s.value("name").toString();
        sc.path      = s.value("path").toString();
        sc.frequency = s.value("frequency", "daily").toString();
        sc.time      = s.value("time", "02:00").toString();
        sc.weekday   = s.value("weekday", 0).toInt();
        sc.enabled   = s.value("enabled", true).toBool();
        sc.quarantine = s.value("quarantine", false).toBool();
        if (!sc.id.isEmpty())
            list << sc;
    }
    s.endArray();
    return list;
}

void SchedulerManager::saveAll(const QList<ScanSchedule> &list)
{
    QSettings s;
    s.remove("scheduler/schedules");
    s.beginWriteArray("scheduler/schedules", list.size());
    for (int i = 0; i < list.size(); ++i) {
        s.setArrayIndex(i);
        const ScanSchedule &sc = list[i];
        s.setValue("id",        sc.id);
        s.setValue("name",      sc.name);
        s.setValue("path",      sc.path);
        s.setValue("frequency", sc.frequency);
        s.setValue("time",      sc.time);
        s.setValue("weekday",   sc.weekday);
        s.setValue("enabled",   sc.enabled);
        s.setValue("quarantine", sc.quarantine);
    }
    s.endArray();
}

// ── Public API ─────────────────────────────────────────────────────────────────

void SchedulerManager::addSchedule(ScanSchedule s)
{
    if (s.id.isEmpty())
        s.id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);

    QList<ScanSchedule> list = schedules();
    list << s;
    saveAll(list);
    createSystemdUnit(s);
}

void SchedulerManager::updateSchedule(const ScanSchedule &s)
{
    QList<ScanSchedule> list = schedules();
    for (auto &sc : list) {
        if (sc.id == s.id) { sc = s; break; }
    }
    saveAll(list);
    removeSystemdUnit(s.id);
    createSystemdUnit(s);
}

void SchedulerManager::removeSchedule(const QString &id)
{
    QList<ScanSchedule> list = schedules();
    list.erase(std::remove_if(list.begin(), list.end(),
        [&id](const ScanSchedule &s) { return s.id == id; }), list.end());
    saveAll(list);
    removeSystemdUnit(id);
}

void SchedulerManager::setScheduleEnabled(const QString &id, bool enable)
{
    QList<ScanSchedule> list = schedules();
    for (auto &s : list) {
        if (s.id == id) { s.enabled = enable; break; }
    }
    saveAll(list);

    QString timer = unitName(id) + ".timer";
    if (enable) {
        QProcess::startDetached("systemctl", {"--user", "enable", "--now", timer});
    } else {
        QProcess::startDetached("systemctl", {"--user", "disable", "--now", timer});
    }
}

void SchedulerManager::runNow(const QString &id)
{
    QProcess::startDetached("systemctl",
        {"--user", "start", unitName(id) + ".service"});
}

QString SchedulerManager::scheduleDescription(const ScanSchedule &s) const
{
    if (!s.enabled) return tr("Disabled");

    static const QStringList days = {
        tr("Mon"), tr("Tue"), tr("Wed"), tr("Thu"),
        tr("Fri"), tr("Sat"), tr("Sun")
    };

    if (s.frequency == "weekly") {
        QString day = (s.weekday >= 0 && s.weekday < 7) ? days[s.weekday] : "Mon";
        return tr("Every %1 at %2").arg(day, s.time);
    }
    if (s.frequency == "monthly")
        return tr("Monthly (1st) at %1").arg(s.time);
    return tr("Daily at %1").arg(s.time);
}

// ── Systemd unit management ────────────────────────────────────────────────────

void SchedulerManager::createSystemdUnit(const ScanSchedule &s)
{
    QDir().mkpath(systemdUserDir());

    QString lp = logPath(s.id);
    QDir().mkpath(QFileInfo(lp).absolutePath());

    QString name = unitName(s.id);

    // Build ExecStart
    QString cmd = QString("/usr/bin/clamscan --recursive --infected --no-summary"
                          " --log=%1 \"%2\"").arg(lp, s.path);
    if (s.quarantine) {
        QString qdir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                       + "/quarantine";
        QDir().mkpath(qdir);
        cmd += " --move=\"" + qdir + "\"";
    }

    // .service
    {
        QFile f(systemdUserDir() + "/" + name + ".service");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            out << "[Unit]\n"
                << "Description=ClamSecurity Scheduled Scan: " << s.name << "\n\n"
                << "[Service]\n"
                << "Type=oneshot\n"
                << "ExecStart=" << cmd << "\n";
        }
    }

    // .timer
    {
        QFile f(systemdUserDir() + "/" + name + ".timer");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            out << "[Unit]\n"
                << "Description=ClamSecurity Scan Timer: " << s.name << "\n\n"
                << "[Timer]\n"
                << "OnCalendar=" << calendarSpec(s) << "\n"
                << "Persistent=true\n\n"
                << "[Install]\n"
                << "WantedBy=timers.target\n";
        }
    }

    reloadSystemd();

    if (s.enabled)
        QProcess::startDetached("systemctl",
            {"--user", "enable", "--now", name + ".timer"});
}

void SchedulerManager::removeSystemdUnit(const QString &id)
{
    QString name = unitName(id);
    // Disable first (fire-and-wait)
    QProcess p;
    p.start("systemctl", {"--user", "disable", "--now", name + ".timer"});
    p.waitForFinished(5000);

    QFile::remove(systemdUserDir() + "/" + name + ".service");
    QFile::remove(systemdUserDir() + "/" + name + ".timer");
    reloadSystemd();
}

void SchedulerManager::reloadSystemd()
{
    QProcess::startDetached("systemctl", {"--user", "daemon-reload"});
}
