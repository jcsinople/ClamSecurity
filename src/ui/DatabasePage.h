#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QSpinBox>

class ClamAvManager;
class FreshclamConfigManager;

class DatabasePage : public QWidget
{
    Q_OBJECT
public:
    explicit DatabasePage(ClamAvManager *clam, QWidget *parent = nullptr);

    void refresh();

signals:
    void backRequested();

private slots:
    void onUpdateClicked();
    void onUpdateOutput(const QString &line);
    void onUpdateFinished(bool success, const QString &message);
    void onSaveAutoUpdate();
    void onConfigSaved(bool success, const QString &message);

private:
    ClamAvManager          *m_clam;
    FreshclamConfigManager *m_freshclamCfg;

    // ── Manual update ─────────────────────────────────────────────────────
    QLabel         *m_dateLabel;
    QLabel         *m_statusLabel;
    QProgressBar   *m_progress;
    QPlainTextEdit *m_outputLog;
    QPushButton    *m_btnUpdate;

    // ── Auto update ───────────────────────────────────────────────────────
    QCheckBox  *m_chkAutoUpdate;
    QSpinBox   *m_spinHours;
    QLabel     *m_autoStatusLabel;
    QPushButton *m_btnSaveAuto;

    QPushButton *m_btnBack;
};
