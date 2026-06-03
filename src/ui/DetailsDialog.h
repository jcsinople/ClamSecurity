#pragma once
#include <QDialog>
#include <QLabel>
#include "SystemChecker.h"

class DetailsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DetailsDialog(QWidget *parent = nullptr);
    void updateStatus(const SystemStatus &status);

private:
    QLabel *makeRow(const QString &text);
    void setRowStatus(QLabel *label, bool ok, const QString &okText, const QString &failText);

    QLabel *m_clamavRow;
    QLabel *m_daemonRow;
    QLabel *m_sigRow;
    QLabel *m_firewallRow;
    QLabel *m_quarantineRow;
};
