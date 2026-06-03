#pragma once
#include <QObject>
#include <QProcess>
#include <QStringList>

class ScanWorker : public QObject
{
    Q_OBJECT
public:
    explicit ScanWorker(const QString &path,
                        const QStringList &exclusions,
                        QObject *parent = nullptr);

public slots:
    void startScan();
    void stopScan();

signals:
    void progressUpdate(int filesScanned, const QString &currentFile);
    void threatFound(const QString &filePath, const QString &threatName);
    void scanFinished(int scanned, int infected, bool cancelled);
    void scanError(const QString &message);

private slots:
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QString     m_path;
    QStringList m_exclusions;
    QProcess   *m_process = nullptr;
    int         m_filesScanned = 0;
    int         m_infected = 0;
    bool        m_cancelled = false;
};
