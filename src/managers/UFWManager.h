#pragma once
#include <QObject>
#include <QProcess>

class UFWManager : public QObject
{
    Q_OBJECT
public:
    explicit UFWManager(QObject *parent = nullptr);

    static bool isInstalled();

    bool isEnabled();
    void setEnabled(bool enable);

    QString rulesOutput();

signals:
    void statusChanged(bool enabled);
    void error(const QString &message);

private:
    static QString runCommand(const QStringList &args, bool needsRoot = false);
};
