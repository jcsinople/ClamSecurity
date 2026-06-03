#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QTableWidget>
#include "UFWManager.h"

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
    void onAddRule();
    void onDeleteRule();
    void onSelectionChanged();
    void onRulesRefreshed(const QList<UFWRule> &rules, const QString &rawOutput);
    void onUfwStatusChanged(bool enabled);

private:
    void updateStatusLabel(bool enabled);
    void populateTable(const QList<UFWRule> &rules);

    UFWManager    *m_ufw;
    QCheckBox     *m_toggle;
    QLabel        *m_statusLabel;
    QTableWidget  *m_rulesTable;
    QPushButton   *m_btnRefresh;
    QPushButton   *m_btnAdd;
    QPushButton   *m_btnDelete;
    QPushButton   *m_btnBack;

    QList<UFWRule> m_rules;
};
