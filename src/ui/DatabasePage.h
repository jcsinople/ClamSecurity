#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QPlainTextEdit>

class ClamAvManager;

class DatabasePage : public QWidget
{
    Q_OBJECT
public:
    explicit DatabasePage(ClamAvManager *clam, QWidget *parent = nullptr);

    void refresh();

signals:
    void backRequested();

private slots:
    void onUpdateClicked();
    void onUpdateOutput(const QString &line);
    void onUpdateFinished(bool success, const QString &message);

private:
    ClamAvManager  *m_clam;
    QLabel         *m_dateLabel;
    QLabel         *m_statusLabel;
    QProgressBar   *m_progress;
    QPlainTextEdit *m_outputLog;
    QPushButton    *m_btnUpdate;
    QPushButton    *m_btnBack;
};
