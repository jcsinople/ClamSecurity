#pragma once
#include <QWidget>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <QButtonGroup>

class AutostartManager;
class ClamAvManager;

class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPage(AutostartManager *autostart,
                          ClamAvManager    *clam,
                          QWidget *parent = nullptr);

    void refresh();

signals:
    void backRequested();
    void themeChangeRequested(int theme);

private slots:
    void onAutostartToggled(bool checked);
    void onStartHiddenToggled(bool checked);
    void onThemeChanged(int id);
    void onRestartDaemon();
    void onInstallServiceMenu();

private:
    AutostartManager *m_autostart;
    ClamAvManager    *m_clam;

    QCheckBox     *m_chkAutostart;
    QCheckBox     *m_chkStartHidden;
    QButtonGroup  *m_themeGroup;
    QRadioButton  *m_radioSystem;
    QRadioButton  *m_radioLight;
    QRadioButton  *m_radioDark;
    QPushButton   *m_btnRestartDaemon;
    QPushButton   *m_btnInstallMenu;
    QPushButton   *m_btnBack;
};
