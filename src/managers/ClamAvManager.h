#pragma once
#include <QObject>
#include <QDateTime>
#include <QStringList>
#include <QProcess>

class ClamAvManager : public QObject
{
    Q_OBJECT
public:
    explicit ClamAvManager(QObject *parent = nullptr);

    static bool isInstalled();
    static QString version();
    QDateTime signatureDate() const;

    bool isDaemonRunning() const;
    void setDaemonEnabled(bool enable);

    bool isFreshclamRunning() const;

    QStringList exclusions() const;
    void setExclusions(const QStringList &paths);
    void addExclusion(const QString &path);
    void removeExclusion(const QString &path);

    void forceUpdate();

signals:
    void updateOutput(const QString &line);
    void updateFinished(bool success, const QString &message);
    void daemonStatusChanged(bool running);

private slots:
    void onUpdateReadyRead();
    void onUpdateFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess *m_updateProcess = nullptr;
};
