#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>

class QuarantineManager;
class ClamAvManager;
class ClamdConfigManager;

struct ActiveThreat {
    QString id;
    QString timestamp;
    QString filePath;
    QString threatName;
    bool    preventionWasActive = false;
};

class ActiveThreatsPage : public QWidget
{
    Q_OBJECT
public:
    explicit ActiveThreatsPage(QuarantineManager  *quar,
                               ClamAvManager      *clam,
                               ClamdConfigManager *cfgMgr,
                               QWidget *parent = nullptr);

    void refresh();

    static void addThreat(const QString &filePath,
                          const QString &threatName,
                          bool preventionActive);

    static void clearHistory();
    static int  pendingCount();

signals:
    void backRequested();

private slots:
    void onQuarantine();
    void onDeleteFile();
    void onAddExclusion();
    void onClearAll();

private:
    static QString dataFilePath();
    static QList<ActiveThreat> loadThreats();
    static void saveThreats(const QList<ActiveThreat> &threats);

    void updateActions();
    QList<int> selectedRows() const;

    QuarantineManager  *m_quar;
    ClamAvManager      *m_clam;
    ClamdConfigManager *m_cfgMgr;

    QLabel        *m_infoLabel;
    QTableWidget  *m_table;
    QPushButton   *m_btnQuarantine;
    QPushButton   *m_btnDelete;
    QPushButton   *m_btnExclude;
    QPushButton   *m_btnClearAll;
    QPushButton   *m_btnBack;
};
