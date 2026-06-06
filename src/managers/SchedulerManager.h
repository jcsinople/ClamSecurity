#pragma once
#include <QObject>
#include <QStringList>
#include <QList>

struct ScanSchedule {
    QString id;
    QString name;
    QString path;
    QString frequency;   // "daily" | "weekly" | "monthly"
    QString time;        // "HH:MM"
    int     weekday = 0; // 0=Mon … 6=Sun (only for weekly)
    bool    enabled    = true;
    bool    quarantine = false;
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

private:
    void saveAll(const QList<ScanSchedule> &list);
    void createSystemdUnit(const ScanSchedule &s);
    void removeSystemdUnit(const QString &id);
    void reloadSystemd();

    static QString unitName(const QString &id);
    static QString systemdUserDir();
    static QString calendarSpec(const ScanSchedule &s);
    static QString logPath(const QString &id);
};
