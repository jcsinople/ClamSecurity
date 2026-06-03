#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QListWidget>
#include <QThread>
#include "ScanWorker.h"

class ClamAvManager;
class QuarantineManager;

class ScanPage : public QWidget
{
    Q_OBJECT
public:
    explicit ScanPage(ClamAvManager *clam,
                      QuarantineManager *quar,
                      QWidget *parent = nullptr);

    void startScan(const QString &path);

signals:
    void backRequested();

private slots:
    void onScanHome();
    void onScanCustom();
    void onStopScan();
    void onProgress(int count, const QString &file);
    void onThreatFound(const QString &file, const QString &threat);
    void onScanFinished(int scanned, int infected, bool cancelled);
    void onQuarantineSelected();

private:
    void setScanningState(bool scanning);
    void clearResults();

    ClamAvManager    *m_clam;
    QuarantineManager *m_quar;
    QThread          *m_thread  = nullptr;
    ScanWorker       *m_worker  = nullptr;

    QLabel       *m_statusLabel;
    QLabel       *m_statsLabel;
    QProgressBar *m_progress;
    QLabel       *m_currentFileLabel;
    QListWidget  *m_threatList;
    QPushButton  *m_btnHome;
    QPushButton  *m_btnCustom;
    QPushButton  *m_btnStop;
    QPushButton  *m_btnQuarantine;
    QPushButton  *m_btnBack;
};
