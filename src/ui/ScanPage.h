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
class ClamdConfigManager;

// Per-threat data stored in each list item
struct ThreatItem {
    QString filePath;
    QString threatName;
    bool    actionTaken = false;  // quarantined or ignored
};
Q_DECLARE_METATYPE(ThreatItem)

class ScanPage : public QWidget
{
    Q_OBJECT
public:
    explicit ScanPage(ClamAvManager      *clam,
                      QuarantineManager  *quar,
                      ClamdConfigManager *cfgMgr,
                      QWidget *parent = nullptr);

    void startScan(const QString &path);

signals:
    void backRequested();
    // Internal: routed to worker via QueuedConnection to avoid cross-thread crash
    void requestStopScan();

private slots:
    void onScanHome();
    void onScanCustom();
    void onStopScan();
    void onProgress(int count, int infected, const QString &file, qint64 elapsedMs);
    void onThreatFound(const QString &file, const QString &threat);
    void onScanFinished(int scanned, int infected, bool cancelled);
    void onTotalCountReady();

    void onQuarantineChecked();
    void onIgnoreChecked();
    void onExcludeFileChecked();
    void onExcludeFolderChecked();
    void onItemChanged(QListWidgetItem *item);

    void onBackClicked();

private:
    void setScanningState(bool scanning);
    void clearResults();
    bool hasUnhandledThreats() const;
    QList<QListWidgetItem*> checkedItems() const;
    void formatStats(int files, int threats, qint64 elapsedMs);
    // Returns true if the user cancelled due to real-time conflict
    bool checkRealtimeConflict(const QString &scanPath);

    ClamAvManager      *m_clam;
    QuarantineManager  *m_quar;
    ClamdConfigManager *m_cfgMgr;
    QThread     *m_thread       = nullptr;
    ScanWorker  *m_worker       = nullptr;
    QProcess    *m_countProcess = nullptr;
    int          m_totalFiles   = 0;
    QString      m_currentPath;

    // UI
    QLabel       *m_pathLabel;
    QLabel       *m_statusLabel;
    QLabel       *m_statsLabel;
    QProgressBar *m_progress;
    QLabel       *m_currentFileLabel;
    QListWidget  *m_threatList;
    QPushButton  *m_btnQuarantine;
    QPushButton  *m_btnIgnore;
    QPushButton  *m_btnExclFile;
    QPushButton  *m_btnExclFolder;
    QPushButton  *m_btnHome;
    QPushButton  *m_btnCustom;
    QPushButton  *m_btnStop;
    QPushButton  *m_btnBack;
};
