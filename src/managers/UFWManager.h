#pragma once
#include <QObject>
#include <QProcess>

struct UFWRule {
    int     number;
    QString action;      // ALLOW, DENY, LIMIT, REJECT
    QString direction;   // IN, OUT, FWD
    QString to;          // port/address
    QString from;        // source
    bool    ipv6 = false;
};

class UFWManager : public QObject
{
    Q_OBJECT
public:
    explicit UFWManager(QObject *parent = nullptr);

    static bool isInstalled();

    // Reads /etc/ufw/ufw.conf — no root required, reliable
    bool isEnabled() const;

    // pkexec ufw enable/disable — one password prompt, no auto rule-refresh
    void setEnabled(bool enable);

    // pkexec ufw status numbered — one password prompt
    void refreshRules();

    // pkexec ufw DIRECTION ACTION PORT [from SRC]
    void addRule(const QString &direction, const QString &action,
                 const QString &port,     const QString &from = "Anywhere");

    // pkexec ufw delete NUMBER
    void deleteRule(int ruleNumber);

signals:
    void statusChanged(bool enabled);
    void rulesRefreshed(const QList<UFWRule> &rules, const QString &rawOutput);
    void error(const QString &message);

private:
    static QList<UFWRule> parseNumbered(const QString &output);
};
