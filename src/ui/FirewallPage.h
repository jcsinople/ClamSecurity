#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QPlainTextEdit>

class UFWManager;

class FirewallPage : public QWidget
{
    Q_OBJECT
public:
    explicit FirewallPage(UFWManager *ufw, QWidget *parent = nullptr);

    void refresh();

signals:
    void backRequested();

private slots:
    void onToggleFirewall(bool checked);
    void onRefreshRules();

private:
    void updateStatusLabel(bool enabled);
    UFWManager    *m_ufw;
    QCheckBox     *m_toggle;
    QLabel        *m_statusLabel;
    QPlainTextEdit *m_rulesView;
    QPushButton   *m_btnRefresh;
    QPushButton   *m_btnBack;
};
