#pragma once
#include <QObject>
#include <QProcess>

class NotificationService : public QObject
{
    Q_OBJECT
public:
    explicit NotificationService(QObject *parent = nullptr);
    ~NotificationService();

    void send(const QString &summary,
              const QString &body,
              const QString &icon = "security-high",
              int urgency = 1);

    void startLogWatcher();
    void stopLogWatcher();

signals:
    void threatDetectedInLog(const QString &filePath, const QString &threat);

private slots:
    void onJournalOutput();

private:
    QProcess *m_journalProcess = nullptr;
};
