#pragma once
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QElapsedTimer>
#include <atomic>

class ScanWorker : public QObject
{
    Q_OBJECT
public:
    explicit ScanWorker(const QString &path,
                        const QStringList &exclusions,
                        QObject *parent = nullptr);

public slots:
    void startScan();
    // Safe to call from any thread — queued via signal connection
    void stopScan();

signals:
    void progressUpdate(int filesScanned, int infected,
                        const QString &currentFile, qint64 elapsedMs);
    void threatFound(const QString &filePath, const QString &threatName);
    void scanFinished(int scanned, int infected, bool cancelled);
    void scanError(const QString &message);

private slots:
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QString     m_path;
    QStringList m_exclusions;
    QProcess   *m_process    = nullptr;
    int         m_filesScanned = 0;
    int         m_infected     = 0;
    std::atomic<bool> m_cancelled{false};
    QElapsedTimer     m_timer;
};
