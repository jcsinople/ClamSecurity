#pragma once
#include <QObject>
#include <QStringList>
#include <QProcess>

struct ClamdOnAccessConfig {
    bool        preventionEnabled  = true;
    int         maxFileSizeMB      = 10;
    int         maxThreads         = 4;
    bool        denyOnError        = false;
    QString     excludeUname       = "clamav";
    QStringList includePaths;             // OnAccessIncludePath
    QStringList onAccessExcludePaths;     // OnAccessExcludePath (dirs from ExclusionsPage)
    QStringList excludeFilePatterns;      // ExcludePath regex (files + extensions from ExclusionsPage)
    bool        extraScanning      = true;
    bool        disableDDD         = false;
};

class ClamdConfigManager : public QObject
{
    Q_OBJECT
public:
    explicit ClamdConfigManager(QObject *parent = nullptr);

    ClamdOnAccessConfig readConfig() const;
    void saveConfig(const ClamdOnAccessConfig &cfg);

    static QString defaultIncludePath();
    static QString configFilePath();

signals:
    void configSaved(bool success, const QString &message);

private slots:
    void onCopyFinished(int exitCode, QProcess::ExitStatus status);

private:
    QString buildSection(const ClamdOnAccessConfig &cfg) const;
    QString m_tempPath;
    QProcess *m_copyProcess = nullptr;
};
