#pragma once
#include <QObject>

class AutostartManager : public QObject
{
    Q_OBJECT
public:
    explicit AutostartManager(QObject *parent = nullptr);

    bool isAutostartEnabled() const;
    void setAutostartEnabled(bool enable, bool startHidden = false);
    bool isStartHidden() const;

private:
    QString autostartFilePath() const;
    QString desktopFileContent(bool startHidden) const;
};
