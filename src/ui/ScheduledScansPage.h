#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include "SchedulerManager.h"

class QuarantineManager;
class ClamAvManager;

class ScheduledScansPage : public QWidget
{
    Q_OBJECT
public:
    explicit ScheduledScansPage(SchedulerManager *mgr,
                                QuarantineManager *qmgr,
                                ClamAvManager *clam,
                                QWidget *parent = nullptr);

    void refresh();

signals:
    void backRequested();
    void threatNotification(const QString &scheduleName, int count);

private slots:
    void onAdd();
    void onEdit();
    void onRemove();
    void onToggleEnabled();
    void onRunNow();
    void onSelectionChanged();
    void onScanLogUpdated(const QString &scheduleId);

    void onThreatQuarantine();
    void onThreatExclude();
    void onThreatDelete();
    void onThreatSelectionChanged();

private:
    void openScheduleDialog(ScanSchedule *schedule);
    void buildSchedulesTab(QWidget *tab);
    void buildThreatsTab(QWidget *tab);
    void processNewResults(const QString &scheduleId, bool notify = true);
    void refreshThreats();

    SchedulerManager  *m_mgr;
    QuarantineManager *m_qmgr;
    ClamAvManager     *m_clam;

    QTabWidget    *m_tabs;

    // Schedules tab
    QLabel        *m_infoLabel;
    QTableWidget  *m_table;
    QPushButton   *m_btnAdd;
    QPushButton   *m_btnEdit;
    QPushButton   *m_btnRemove;
    QPushButton   *m_btnToggle;
    QPushButton   *m_btnRunNow;
    QPushButton   *m_btnBack;

    // Threats tab
    QTableWidget  *m_threatsTable;
    QPushButton   *m_btnQuarantine;
    QPushButton   *m_btnExclude;
    QPushButton   *m_btnDeleteFile;
};
