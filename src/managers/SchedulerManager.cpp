#include "SchedulerManager.h"
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QUuid>
#include <QStandardPaths>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimer>
#include <algorithm>

SchedulerManager::SchedulerManager(QObject *parent) : QObject(parent)
{
    startLogWatcher();
    // Rewrite service files once the event loop is running so existing schedules
    // pick up the current exclusion list and ExecStartPre line even if they were
    // created with an older version of the app.
    QTimer::singleShot(0, this, &SchedulerManager::syncServiceFiles);
}

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

QString SchedulerManager::threatsFilePath()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base + "/scheduled_threats.json";
}

QString SchedulerManager::calendarSpec(const ScanSchedule &s)
{
    static const QStringList days = {
        "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
    };
    QString t = s.time.isEmpty() ? "02:00" : s.time;
    if (s.frequency == "weekly") {
        QString day = (s.weekday >= 0 && s.weekday < 7) ? days[s.weekday] : "Mon";
        return QString("%1 *-*-* %2:00").arg(day, t);
    }
    if (s.frequency == "monthly")
        return QString("*-*-01 %1:00").arg(t);
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
        sc.notify    = s.value("notify", true).toBool();
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
        s.setValue("notify",    sc.notify);
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
    clearThreatsForSchedule(id);
    QSettings().remove("scheduler/logMtime/" + id);
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
    // Ensure the service file is current (latest exclusions, ExecStartPre) before starting.
    const auto list = schedules();
    auto it = std::find_if(list.begin(), list.end(),
        [&id](const ScanSchedule &s) { return s.id == id; });
    if (it != list.end()) {
        writeServiceFile(*it);
        reloadSystemd();
    }
    QProcess::startDetached("systemctl",
        {"--user", "start", unitName(id) + ".service"});
}

void SchedulerManager::syncServiceFiles()
{
    // Rewrite .service files for all schedules with current exclusions.
    // Calls daemon-reload once. Does NOT stop or restart any timers.
    QDir().mkpath(systemdUserDir());
    for (const ScanSchedule &s : schedules())
        writeServiceFile(s);
    reloadSystemd();
}

void SchedulerManager::refreshAllUnits()
{
    // Full recreation: stop timers, rewrite all files, re-enable timers.
    // Use when a schedule is added, edited, or removed.
    for (const ScanSchedule &s : schedules()) {
        removeSystemdUnit(s.id);
        createSystemdUnit(s);
    }
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

// ── Scan result parsing ────────────────────────────────────────────────────────

ScanSummary SchedulerManager::readScanSummary(const QString &scheduleId) const
{
    ScanSummary summary;
    summary.logPath = logPath(scheduleId);

    QFile f(summary.logPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return summary;

    summary.hasResults = true;
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith("Scanned files:"))
            summary.filesScanned = line.mid(14).trimmed().toInt();
        else if (line.startsWith("Infected files:"))
            summary.infected = line.mid(15).trimmed().toInt();
        else if (line.startsWith("Start Date:")) {
            // Format: "Start Date: 2026:06:08 10:00:00"
            QString dateStr = line.mid(11).trimmed();
            summary.scanTime = QDateTime::fromString(dateStr, "yyyy:MM:dd HH:mm:ss");
        }
    }
    return summary;
}

QList<QPair<QString,QString>> SchedulerManager::readInfectedFiles(const QString &scheduleId) const
{
    QList<QPair<QString,QString>> result;
    QFile f(logPath(scheduleId));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return result;

    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (!line.endsWith(" FOUND"))
            continue;
        // Format: /path/to/file: ThreatName FOUND
        QString withoutFound = line.left(line.length() - 6).trimmed();
        int sep = withoutFound.lastIndexOf(": ");
        if (sep < 0) continue;
        result << qMakePair(withoutFound.left(sep).trimmed(),
                            withoutFound.mid(sep + 2).trimmed());
    }
    return result;
}

static bool logIsComplete(const QString &path)
{
    // Only process once clamscan has fully written the SCAN SUMMARY section.
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    return f.readAll().contains("SCAN SUMMARY");
}

bool SchedulerManager::isLogProcessed(const QString &scheduleId) const
{
    QString path = logPath(scheduleId);
    if (!QFile::exists(path)) return true;
    if (!logIsComplete(path)) return true; // scan still running — skip silently
    QFileInfo fi(path);
    QSettings s;
    qint64 stored = s.value("scheduler/logMtime/" + scheduleId, 0).toLongLong();
    return fi.lastModified().toSecsSinceEpoch() <= stored;
}

void SchedulerManager::markLogProcessed(const QString &scheduleId)
{
    QFileInfo fi(logPath(scheduleId));
    QSettings s;
    s.setValue("scheduler/logMtime/" + scheduleId, fi.lastModified().toSecsSinceEpoch());
}

// ── Detected threats store ─────────────────────────────────────────────────────

QList<ScheduledThreat> SchedulerManager::scheduledThreats() const
{
    QList<ScheduledThreat> list;
    QFile f(threatsFilePath());
    if (!f.open(QIODevice::ReadOnly)) return list;

    QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    for (const QJsonValue &v : arr) {
        QJsonObject o = v.toObject();
        ScheduledThreat t;
        t.id           = o["id"].toString();
        t.scheduleId   = o["scheduleId"].toString();
        t.scheduleName = o["scheduleName"].toString();
        t.filePath     = o["filePath"].toString();
        t.threatName   = o["threatName"].toString();
        t.detectedAt   = QDateTime::fromString(o["detectedAt"].toString(), Qt::ISODate);
        list << t;
    }
    return list;
}

void SchedulerManager::saveThreats(const QList<ScheduledThreat> &list)
{
    QDir().mkpath(QFileInfo(threatsFilePath()).absolutePath());
    QJsonArray arr;
    for (const ScheduledThreat &t : list) {
        QJsonObject o;
        o["id"]           = t.id;
        o["scheduleId"]   = t.scheduleId;
        o["scheduleName"] = t.scheduleName;
        o["filePath"]     = t.filePath;
        o["threatName"]   = t.threatName;
        o["detectedAt"]   = t.detectedAt.toString(Qt::ISODate);
        arr << o;
    }
    QFile f(threatsFilePath());
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(arr).toJson());
}

void SchedulerManager::addScheduledThreat(const ScheduledThreat &t)
{
    auto list = scheduledThreats();
    // Deduplicate: same file from the same schedule must only appear once
    for (const auto &existing : list) {
        if (existing.filePath == t.filePath && existing.scheduleId == t.scheduleId)
            return;
    }
    list << t;
    saveThreats(list);
}

void SchedulerManager::removeScheduledThreat(const QString &threatId)
{
    auto list = scheduledThreats();
    list.erase(std::remove_if(list.begin(), list.end(),
        [&threatId](const ScheduledThreat &t) { return t.id == threatId; }), list.end());
    saveThreats(list);
}

void SchedulerManager::clearThreatsForSchedule(const QString &scheduleId)
{
    auto list = scheduledThreats();
    list.erase(std::remove_if(list.begin(), list.end(),
        [&scheduleId](const ScheduledThreat &t) { return t.scheduleId == scheduleId; }), list.end());
    saveThreats(list);
}

// ── Log watcher ────────────────────────────────────────────────────────────────

void SchedulerManager::startLogWatcher()
{
    QString logsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/scanlogs";
    QDir().mkpath(logsDir);

    m_logWatcher = new QFileSystemWatcher(this);
    m_logWatcher->addPath(logsDir);

    // Watch any log files that already exist (e.g. from a previous run).
    for (const ScanSchedule &s : schedules()) {
        const QString lp = logPath(s.id);
        if (QFile::exists(lp))
            m_logWatcher->addPath(lp);
    }

    // Directory change: a log was created or deleted.
    // inotify removes the watch on a deleted file, so we must re-add any log
    // files that appeared (e.g. clamscan just created a fresh log after
    // ExecStartPre deleted the old one).
    connect(m_logWatcher, &QFileSystemWatcher::directoryChanged,
            this, [this]() {
        for (const ScanSchedule &s : schedules()) {
            const QString lp = logPath(s.id);
            if (QFile::exists(lp) && !m_logWatcher->files().contains(lp))
                m_logWatcher->addPath(lp);
        }
        // Check for any already-complete logs
        for (const ScanSchedule &s : schedules()) {
            if (!isLogProcessed(s.id))
                emit scanLogUpdated(s.id);
        }
    });

    // File change: clamscan is writing to the log.
    // On Linux/inotify, Qt removes the watch after the file is deleted.
    // Re-add if the file was recreated at the same path.
    connect(m_logWatcher, &QFileSystemWatcher::fileChanged,
            this, [this](const QString &path) {
        if (QFile::exists(path) && !m_logWatcher->files().contains(path))
            m_logWatcher->addPath(path);

        QFileInfo fi(path);
        const QString id = fi.completeBaseName();
        if (!isLogProcessed(id))
            emit scanLogUpdated(id);
    });
}

// ── Systemd unit management ────────────────────────────────────────────────────

void SchedulerManager::writeServiceFile(const ScanSchedule &s)
{
    QString lp   = logPath(s.id);
    QString name = unitName(s.id);

    QDir().mkpath(QFileInfo(lp).absolutePath());

    // Ensure the new (or existing) log file is watched
    if (m_logWatcher && QFile::exists(lp) && !m_logWatcher->files().contains(lp))
        m_logWatcher->addPath(lp);

    // Build exclusion arguments from current app settings
    QSettings qs;
    QStringList excludePaths = qs.value("scan/exclusions", QStringList()).toStringList();
    QStringList excludeExts  = qs.value("scan/extension_exclusions", QStringList()).toStringList();

    QStringList excludeArgs;
    for (const QString &p : excludePaths) {
        // Anchor at start so /foo/bar doesn't accidentally match /other/foo/bar
        QString escaped = "^" + QRegularExpression::escape(p);
        if (QFileInfo(p).isDir())
            excludeArgs << "--exclude-dir=" + escaped;
        else
            excludeArgs << "--exclude=" + escaped + "$";
    }
    for (const QString &ext : excludeExts) {
        QString e = ext.startsWith('.') ? ext : '.' + ext;
        excludeArgs << "--exclude=" + QRegularExpression::escape(e) + "$";
    }

    // clamscan: detect and log. quarantining is handled by the app after reading the log.
    // ExecStartPre deletes the old log so each run produces a fresh file and the app
    // never re-processes results from previous executions.
    // --no-summary is intentionally omitted: the SCAN SUMMARY section serves as a
    // completion marker so we only process the log once clamscan has fully finished.
    QString cmd = QString("/usr/bin/clamscan --recursive --infected --log=\"%1\"").arg(lp);
    if (!excludeArgs.isEmpty())
        cmd += ' ' + excludeArgs.join(' ');
    cmd += " \"" + s.path + '"';

    QFile f(systemdUserDir() + "/" + name + ".service");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << "[Unit]\n"
            << "Description=ClamSecurity Scheduled Scan: " << s.name << "\n\n"
            << "[Service]\n"
            << "Type=oneshot\n"
            << "SuccessExitStatus=0 1\n"
            << "ExecStartPre=/bin/rm -f " << lp << "\n"
            << "ExecStart=" << cmd << "\n";
    }
}

void SchedulerManager::writeUnitFiles(const ScanSchedule &s)
{
    writeServiceFile(s);

    QString name = unitName(s.id);
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

void SchedulerManager::createSystemdUnit(const ScanSchedule &s)
{
    QDir().mkpath(systemdUserDir());
    writeUnitFiles(s);
    reloadSystemd();

    if (s.enabled)
        QProcess::startDetached("systemctl",
            {"--user", "enable", "--now", unitName(s.id) + ".timer"});
}

void SchedulerManager::removeSystemdUnit(const QString &id)
{
    QString name = unitName(id);
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
