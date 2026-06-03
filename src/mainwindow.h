#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QToolBar>
#include <QAction>
#include "SystemChecker.h"
#include "LanguageManager.h"

class MonitorWidget;
class ModuleCard;
class ClamAvManager;
class UFWManager;
class QuarantineManager;
class AutostartManager;
class NotificationService;
class ScanPage;
class DatabasePage;
class ExclusionsPage;
class QuarantinePage;
class FirewallPage;
class SettingsPage;

enum class Page {
    Overview  = 0,
    Scan      = 1,
    Database  = 2,
    Exclusions = 3,
    Quarantine = 4,
    Firewall  = 5,
    Settings  = 6
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void startScanWithPath(const QString &path);
    void applyTheme(int theme);  // 0=system 1=light 2=dark

protected:
    void closeEvent(QCloseEvent *event) override;

public slots:
    void showAndRaise();

private slots:
    void onStatusChanged(const SystemStatus &status);
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onShowDetails();
    void navigateTo(Page page);
    void navigateBack();

    void onTrayActionScanHome();
    void onTrayActionUpdateDB();
    void onTrayActionToggleRealtime();
    void onTrayActionQuit();

private:
    void setupManagers();
    void setupUI();
    void setupTray();
    void connectSignals();
    void updateStatusDisplay(const SystemStatus &status);
    void updateCardSubtitles(const SystemStatus &status);
    QWidget *buildOverviewPage();

    // Core managers
    ClamAvManager     *m_clam;
    UFWManager        *m_ufw;
    QuarantineManager *m_quar;
    AutostartManager  *m_autostart;
    NotificationService *m_notif;
    SystemChecker       *m_checker;
    LanguageManager     *m_langMgr;

    // UI
    QStackedWidget *m_stack;
    QToolBar       *m_toolbar;
    QAction        *m_actBack;

    // Overview widgets
    MonitorWidget *m_monitor;
    QLabel        *m_statusLabel;
    QLabel        *m_statusSub;
    QPushButton   *m_btnDetails;
    QCheckBox     *m_realtimeToggle;

    // Module cards (overview grid)
    ModuleCard *m_cardScan;
    ModuleCard *m_cardDB;
    ModuleCard *m_cardExcl;
    ModuleCard *m_cardQuar;
    ModuleCard *m_cardFW;
    ModuleCard *m_cardConf;

    // Module pages
    ScanPage       *m_scanPage;
    DatabasePage   *m_dbPage;
    ExclusionsPage *m_exclusionsPage;
    QuarantinePage *m_quarantinePage;
    FirewallPage   *m_firewallPage;
    SettingsPage   *m_settingsPage;

    // Tray
    QSystemTrayIcon *m_tray;
    QMenu           *m_trayMenu;
    QAction         *m_trayActRealtime;
};
