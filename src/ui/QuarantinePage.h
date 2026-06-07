#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>

class QuarantineManager;
class ClamAvManager;
class ClamdConfigManager;

class QuarantinePage : public QWidget
{
    Q_OBJECT
public:
    explicit QuarantinePage(QuarantineManager  *quar,
                            ClamAvManager      *clam,
                            ClamdConfigManager *cfgMgr,
                            QWidget *parent = nullptr);

    void refresh();

signals:
    void backRequested();

private slots:
    void onRestore();
    void onDelete();
    void onDeleteAll();
    void onSelectionChanged();

private:
    QuarantineManager  *m_quar;
    ClamAvManager      *m_clam;
    ClamdConfigManager *m_cfgMgr;
    QTableWidget      *m_table;
    QLabel            *m_countLabel;
    QPushButton       *m_btnRestore;
    QPushButton       *m_btnDelete;
    QPushButton       *m_btnDeleteAll;
    QPushButton       *m_btnBack;
};
