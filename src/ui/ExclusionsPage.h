#pragma once
#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QLabel>

class ClamAvManager;
class ClamdConfigManager;

class ExclusionsPage : public QWidget
{
    Q_OBJECT
public:
    explicit ExclusionsPage(ClamAvManager     *clam,
                            ClamdConfigManager *cfgMgr,
                            QWidget *parent = nullptr);

    void refresh();

signals:
    void backRequested();
    void exclusionsChanged();

private slots:
    void onAddFile();
    void onAddFolder();
    void onRemoveSelected();
    void onAddExtension();
    void onRemoveExtension();
    void onApplyToRT();

private:
    void buildClamdConfig();

    ClamAvManager     *m_clam;
    ClamdConfigManager *m_cfgMgr;

    // Paths tab
    QListWidget *m_list;
    QPushButton *m_btnAddFile;
    QPushButton *m_btnAddFolder;
    QPushButton *m_btnRemove;

    // Extensions tab
    QListWidget *m_extList;
    QPushButton *m_btnAddExt;
    QPushButton *m_btnRemoveExt;

    // Bottom
    QPushButton *m_btnApplyToRT;
    QPushButton *m_btnBack;
    QLabel      *m_statusLabel;
};
