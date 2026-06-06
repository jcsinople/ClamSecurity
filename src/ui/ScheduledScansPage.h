#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include "SchedulerManager.h"

class ScheduledScansPage : public QWidget
{
    Q_OBJECT
public:
    explicit ScheduledScansPage(SchedulerManager *mgr, QWidget *parent = nullptr);

    void refresh();

signals:
    void backRequested();

private slots:
    void onAdd();
    void onEdit();
    void onRemove();
    void onToggleEnabled();
    void onRunNow();
    void onSelectionChanged();

private:
    void openScheduleDialog(ScanSchedule *schedule);

    SchedulerManager *m_mgr;

    QLabel        *m_infoLabel;
    QTableWidget  *m_table;
    QPushButton   *m_btnAdd;
    QPushButton   *m_btnEdit;
    QPushButton   *m_btnRemove;
    QPushButton   *m_btnToggle;
    QPushButton   *m_btnRunNow;
    QPushButton   *m_btnBack;
};
