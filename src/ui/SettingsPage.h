#pragma once
#include <QWidget>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QScrollArea>
#include <QNetworkAccessManager>
#include <QTabWidget>
#include <QStackedWidget>

class AutostartManager;
class ClamAvManager;
class LanguageManager;
class ClamdConfigManager;

class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPage(AutostartManager   *autostart,
                          ClamAvManager      *clam,
                          LanguageManager    *langMgr,
                          ClamdConfigManager *cfgMgr,
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
    void onSaveProtection();
    void onCheckForUpdates();
    void onPreventionToggled(bool checked);

private:
    void buildGeneralTab(QWidget *tab);
    void buildProtectionTab(QWidget *tab);
    void buildAboutTab(QWidget *tab);
    void loadProtectionSettings();

    AutostartManager   *m_autostart;
    ClamAvManager      *m_clam;
    LanguageManager    *m_langMgr;
    ClamdConfigManager *m_cfgMgr;

    // ── General tab ───────────────────────────────────────────────────────
    QCheckBox    *m_chkAutostart;
    QCheckBox    *m_chkStartHidden;
    QButtonGroup *m_themeGroup;
    QRadioButton *m_radioSystem;
    QRadioButton *m_radioLight;
    QRadioButton *m_radioDark;
    QComboBox    *m_langCombo;
    QCheckBox    *m_chkDaemon;
    QCheckBox    *m_chkRealtime;
    QPushButton  *m_btnRestartDaemon;
    QPushButton  *m_btnInstallMenu;

    // ── Protection tab ────────────────────────────────────────────────────
    QCheckBox    *m_chkPrevention;
    QLabel       *m_lblPreventionInfo;
    QListWidget  *m_watchedList;
    QPushButton  *m_btnAddWatched;
    QPushButton  *m_btnRemoveWatched;
    QStackedWidget *m_protStack;   // 0=basic, 1=advanced scroll area
    QPushButton  *m_btnViewAdvanced;
    QPushButton  *m_btnViewBasic;
    // Advanced params
    QSpinBox     *m_spinMaxFileSize;
    QSpinBox     *m_spinMaxThreads;
    QCheckBox    *m_chkDenyOnError;
    QLineEdit    *m_editExcludeUname;
    QCheckBox    *m_chkExtraScanning;
    QCheckBox    *m_chkDisableDDD;
    // Save button + status
    QPushButton  *m_btnSaveProtection;
    QLabel       *m_lblProtStatus;

    // ── About tab ─────────────────────────────────────────────────────────
    QPushButton          *m_btnCheckUpdates;
    QLabel               *m_lblUpdateStatus;
    QNetworkAccessManager *m_nam;

    // Back button (outside tabs)
    QPushButton *m_btnBack;
};
