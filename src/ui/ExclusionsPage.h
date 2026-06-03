#pragma once
#include <QWidget>
#include <QListWidget>
#include <QPushButton>

class ClamAvManager;

class ExclusionsPage : public QWidget
{
    Q_OBJECT
public:
    explicit ExclusionsPage(ClamAvManager *clam, QWidget *parent = nullptr);

    void refresh();

signals:
    void backRequested();

private slots:
    void onAddFile();
    void onAddFolder();
    void onRemoveSelected();

private:
    ClamAvManager *m_clam;
    QListWidget   *m_list;
    QPushButton   *m_btnAddFile;
    QPushButton   *m_btnAddFolder;
    QPushButton   *m_btnRemove;
    QPushButton   *m_btnBack;
};
