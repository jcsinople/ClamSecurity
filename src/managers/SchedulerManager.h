#pragma once
#include <QObject>
#include <QStringList>
#include <QList>
#include <QDateTime>

class QFileSystemWatcher;

struct ScanSchedule {
    QString id;
    QString name;
    QString path;
    QString frequency;   // "daily" | "weekly" | "monthly"
    QString time;        // "HH:MM"
    int     weekday   = 0;    // 0=Mon … 6=Sun (only for weekly)
    bool    enabled   = true;
    bool    quarantine = false;
    bool    notify    = true;  // send system notification when threats detected
};

struct ScanSummary {
    bool      hasResults   = false;
    QDateTime scanTime;
    int       filesScanned = 0;
    int       infected     = 0;
    QString   logPath;
};

struct ScheduledThreat {
    QString   id;
    QString   scheduleId;
    QString   scheduleName;
    QString   filePath;
    QString   threatName;
    QDateTime detectedAt;
};

class SchedulerManager : public QObject
{
    Q_OBJECT
public:
    explicit SchedulerManager(QObject *parent = nullptr);

    QList<ScanSchedule> schedules() const;
    void addSchedule(ScanSchedule s);
    void updateSchedule(const ScanSchedule &s);
    void removeSchedule(const QString &id);
    void setScheduleEnabled(const QString &id, bool enable);
    void runNow(const QString &id);

    QString scheduleDescription(const ScanSchedule &s) const;
    void refreshAllUnits();

    // Scan results
    ScanSummary readScanSummary(const QString &scheduleId) const;
    QList<QPair<QString,QString>> readInfectedFiles(const QString &scheduleId) const;
    bool isLogProcessed(const QString &scheduleId) const;
    void markLogProcessed(const QString &scheduleId);

    // Detected threats store (only for non-auto-quarantine scans)
    QList<ScheduledThreat> scheduledThreats() const;
    void addScheduledThreat(const ScheduledThreat &t);
    void removeScheduledThreat(const QString &threatId);
    void clearThreatsForSchedule(const QString &scheduleId);

signals:
    void scanLogUpdated(const QString &scheduleId);

private:
    void saveAll(const QList<ScanSchedule> &list);
    void createSystemdUnit(const ScanSchedule &s);
    void removeSystemdUnit(const QString &id);
    void reloadSystemd();
    void saveThreats(const QList<ScheduledThreat> &list);
    void startLogWatcher();

    static QString unitName(const QString &id);
    static QString systemdUserDir();
    static QString calendarSpec(const ScanSchedule &s);
    static QString logPath(const QString &id);
    static QString threatsFilePath();

    QFileSystemWatcher *m_logWatcher = nullptr;
};
