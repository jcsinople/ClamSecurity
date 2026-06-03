#pragma once
#include <QWidget>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <QButtonGroup>
#include <QComboBox>

class AutostartManager;
class ClamAvManager;
class LanguageManager;

class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPage(AutostartManager *autostart,
                          ClamAvManager    *clam,
                          LanguageManager  *langMgr,
                          QWidget *parent = nullptr);

    void refresh();

signals:
    void backRequested();
    void themeChangeRequested(int theme);

private slots:
    void onAutostartToggled(bool checked);
    void onStartHiddenToggled(bool checked);
    void onThemeChanged(int id);
    void onLanguageChanged(int index);
    void onRestartDaemon();
    void onInstallServiceMenu();

private:
    AutostartManager *m_autostart;
    ClamAvManager    *m_clam;
    LanguageManager  *m_langMgr;

    QCheckBox     *m_chkAutostart;
    QCheckBox     *m_chkStartHidden;
    QButtonGroup  *m_themeGroup;
    QRadioButton  *m_radioSystem;
    QRadioButton  *m_radioLight;
    QRadioButton  *m_radioDark;
    QComboBox     *m_langCombo;
    QCheckBox     *m_chkDaemon;
    QCheckBox     *m_chkRealtime;
    QPushButton   *m_btnRestartDaemon;
    QPushButton   *m_btnInstallMenu;
    QPushButton   *m_btnBack;
};
