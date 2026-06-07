#pragma once
#include <QObject>
#include <QProcess>

class FreshclamConfigManager : public QObject
{
    Q_OBJECT
public:
    explicit FreshclamConfigManager(QObject *parent = nullptr);

    int  readChecks() const;       // checks per day (1-24); default 12 = every 2 h
    void saveChecks(int checks);   // writes Checks directive at end of freshclam.conf

    static QString configFilePath();

signals:
    void configSaved(bool success, const QString &message);

private slots:
    void onCopyFinished(int exitCode, QProcess::ExitStatus status);

private:
    static constexpr const char *MARKER = "# ClamSecurity: managed settings";

    QString   m_tempPath;
    QProcess *m_copyProcess = nullptr;
};
