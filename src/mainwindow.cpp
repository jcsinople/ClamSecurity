#include "mainwindow.h"
#include "ClamAvManager.h"
#include "ClamdConfigManager.h"
#include "QuarantineManager.h"
#include "UFWManager.h"
#include "SystemChecker.h"
#include "AutostartManager.h"
#include "NotificationService.h"
#include "LanguageManager.h"
#include "MonitorWidget.h"
#include "ModuleCard.h"
#include "DetailsDialog.h"
#include "ScanPage.h"
#include "DatabasePage.h"
#include "ExclusionsPage.h"
#include "QuarantinePage.h"
#include "FirewallPage.h"
#include "SettingsPage.h"
#include "ActiveThreatsPage.h"
#include "ScheduledScansPage.h"
#include "SchedulerManager.h"

#include <QDir>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFont>
#include <QCloseEvent>
#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QMessageBox>
#include <QFileInfo>

MainWindow::MainWindow(LanguageManager *langMgr, QWidget *parent)
    : QMainWindow(parent), m_langMgr(langMgr)
{
    setWindowTitle("ClamSecurity");
    setWindowIcon(QIcon::fromTheme("security-high",
                                   QIcon(":/icons/clamsecurity.svg")));
    setFixedSize(800, 530);

    ActiveThreatsPage::clearHistory();   // start each session with a clean slate

    setupManagers();
    setupUI();
    setupTray();
    connectSignals();

    // Apply saved theme
    QSettings s;
    applyTheme(s.value("ui/theme", 0).toInt());

    m_checker->refresh();
}

MainWindow::~MainWindow() = default;

// ─── Managers ────────────────────────────────────────────────────────────────

void MainWindow::setupManagers()
{
    m_clam      = new ClamAvManager(this);
    m_ufw       = new UFWManager(this);
    m_quar      = new QuarantineManager(this);
    m_autostart = new AutostartManager(this);
    m_notif     = new NotificationService(this);
    m_cfgMgr    = new ClamdConfigManager(this);
    m_schedMgr  = new SchedulerManager(this);
    m_checker   = new SystemChecker(m_clam, m_ufw, m_quar, m_cfgMgr, this);
    // m_langMgr received from main() — single instance that owns the installed translators
}

// ─── UI ──────────────────────────────────────────────────────────────────────

void MainWindow::setupUI()
{
    m_stack = new QStackedWidget(this);

    // Build pages
    m_scanPage          = new ScanPage(m_clam, m_quar, m_cfgMgr);
    m_dbPage            = new DatabasePage(m_clam);
    m_exclusionsPage    = new ExclusionsPage(m_clam, m_cfgMgr);
    m_quarantinePage    = new QuarantinePage(m_quar, m_clam, m_cfgMgr);
    m_firewallPage      = new FirewallPage(m_ufw);
    m_settingsPage      = new SettingsPage(m_autostart, m_clam, m_langMgr, m_cfgMgr);
    m_activeThreatsPage  = new ActiveThreatsPage(m_quar, m_clam, m_cfgMgr);
    m_scheduledScansPage = new ScheduledScansPage(m_schedMgr, m_quar, m_clam);

    m_stack->addWidget(buildOverviewPage());     // index 0
    m_stack->addWidget(m_scanPage);              // 1
    m_stack->addWidget(m_dbPage);               // 2
    m_stack->addWidget(m_exclusionsPage);        // 3
    m_stack->addWidget(m_quarantinePage);        // 4
    m_stack->addWidget(m_firewallPage);          // 5
    m_stack->addWidget(m_settingsPage);          // 6
    m_stack->addWidget(m_activeThreatsPage);     // 7
    m_stack->addWidget(m_scheduledScansPage);    // 8

    setCentralWidget(m_stack);

}

QWidget *MainWindow::buildOverviewPage()
{
    auto *page   = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(20);

    // ── Top panel ───────────────────────────────────────────
    auto *topPanel = new QHBoxLayout;
    topPanel->setSpacing(32);

    m_monitor = new MonitorWidget(page);
    topPanel->addWidget(m_monitor);

    auto *statusBox = new QVBoxLayout;
    statusBox->setSpacing(8);

    m_statusLabel = new QLabel(tr("Checking…"), page);
    QFont bigFont = m_statusLabel->font();
    bigFont.setPointSize(18);
    bigFont.setBold(true);
    m_statusLabel->setFont(bigFont);
    m_statusLabel->setWordWrap(true);

    m_statusSub = new QLabel(page);
    m_statusSub->setWordWrap(true);

    m_btnDetails = new QPushButton(tr("View Details…"), page);
    m_btnDetails->setFixedWidth(160);

    m_realtimeToggle = new QCheckBox(tr("Real-Time Protection (ClamOnAcc)"), page);
    QFont rtFont = m_realtimeToggle->font();
    rtFont.setPointSize(10);
    m_realtimeToggle->setFont(rtFont);
    m_realtimeToggle->setToolTip(
        tr("Enables on-access scanning via clamav-clamonacc.\n"
           "Requires clamav-daemon to be running."));

    statusBox->addStretch();
    statusBox->addWidget(m_statusLabel);
    statusBox->addWidget(m_statusSub);
    statusBox->addSpacing(8);
    statusBox->addWidget(m_btnDetails, 0, Qt::AlignLeft);
    statusBox->addSpacing(4);
    statusBox->addWidget(m_realtimeToggle, 0, Qt::AlignLeft);
    statusBox->addStretch();

    topPanel->addLayout(statusBox, 1);
    layout->addLayout(topPanel);

    // Separator
    auto *sep = new QFrame(page);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    layout->addWidget(sep);

    // ── Module cards grid (4×2) ──────────────────────────────
    auto *grid = new QGridLayout;
    grid->setSpacing(14);

    auto makeCard = [&](const QString &icon, const QString &title,
                        const QString &sub, Page target) {
        auto *card = new ModuleCard(icon, title, sub, page);
        connect(card, &ModuleCard::clicked, this, [this, target]() {
            navigateTo(target);
        });
        return card;
    };

    m_cardScan  = makeCard("system-search",          tr("Scan"),
                            tr("Scan files"),        Page::Scan);
    m_cardDB    = makeCard("system-software-update", tr("Database"),
                            tr("Signatures"),        Page::Database);
    m_cardExcl  = makeCard("folder-open",            tr("Exclusions"),
                            tr("Excluded paths"),    Page::Exclusions);
    m_cardQuar  = makeCard("security-low",           tr("Quarantine"),
                            tr("Isolated files"),    Page::Quarantine);
    m_cardFW    = makeCard("firewall-config",         tr("Firewall"),
                            tr("UFW control"),       Page::Firewall);
    m_cardConf  = makeCard("configure",              tr("Settings"),
                            tr("App settings"),      Page::Settings);
    m_cardActiveThreats = makeCard("dialog-warning",   tr("Active Threats"),
                            tr("Real-time detections"), Page::ActiveThreats);
    m_cardScheduler     = makeCard("appointment-new",   tr("Scheduler"),
                            tr("Automatic scans"),      Page::ScheduledScans);

    // Row 0: Scan, Database, Exclusions, Settings
    grid->addWidget(m_cardScan,  0, 0);
    grid->addWidget(m_cardDB,    0, 1);
    grid->addWidget(m_cardExcl,  0, 2);
    grid->addWidget(m_cardConf,  0, 3);
    // Row 1: Quarantine, Firewall, ActiveThreats, Scheduler
    grid->addWidget(m_cardQuar,          1, 0);
    grid->addWidget(m_cardFW,            1, 1);
    grid->addWidget(m_cardActiveThreats, 1, 2);
    grid->addWidget(m_cardScheduler,     1, 3);

    layout->addLayout(grid);
    layout->addStretch();

    connect(m_btnDetails, &QPushButton::clicked, this, &MainWindow::onShowDetails);
    connect(m_realtimeToggle, &QCheckBox::toggled, this, [this](bool checked) {
        m_realtimeToggle->setEnabled(false);
        if (m_clam->isClamonaacAvailable())
            m_clam->setClamonaacEnabled(checked);
        else
            m_clam->setDaemonEnabled(checked);
        QTimer::singleShot(4000, this, [this]() {
            m_realtimeToggle->setEnabled(true);
            m_checker->refresh();
        });
    });

    return page;
}

// ─── System Tray ─────────────────────────────────────────────────────────────

void MainWindow::setupTray()
{
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(QIcon::fromTheme("security-high",
                                     QIcon(":/icons/clamsecurity.svg")));
    m_tray->setToolTip("ClamSecurity");

    m_trayMenu = new QMenu(this);

    auto *actShow = m_trayMenu->addAction(
        QIcon::fromTheme("security-high"), tr("Open ClamSecurity"));
    m_trayMenu->addSeparator();

    auto *actScanHome = m_trayMenu->addAction(
        QIcon::fromTheme("system-search"), tr("Scan HOME"));
    auto *actUpdate = m_trayMenu->addAction(
        QIcon::fromTheme("system-software-update"), tr("Update signatures"));

    m_trayMenu->addSeparator();
    m_trayActRealtime = m_trayMenu->addAction(
        QIcon::fromTheme("media-record"), tr("Real-Time Protection"));
    m_trayActRealtime->setCheckable(true);

    m_trayMenu->addSeparator();
    auto *actQuit = m_trayMenu->addAction(
        QIcon::fromTheme("application-exit"), tr("Quit"));

    m_tray->setContextMenu(m_trayMenu);
    m_tray->show();

    connect(actShow,      &QAction::triggered, this, &MainWindow::showAndRaise);
    connect(actScanHome,  &QAction::triggered, this, &MainWindow::onTrayActionScanHome);
    connect(actUpdate,    &QAction::triggered, this, &MainWindow::onTrayActionUpdateDB);
    connect(m_trayActRealtime, &QAction::triggered,
            this, &MainWindow::onTrayActionToggleRealtime);
    connect(actQuit,      &QAction::triggered, this, &MainWindow::onTrayActionQuit);
    connect(m_tray, &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayActivated);
}

// ─── Signal wiring ───────────────────────────────────────────────────────────

void MainWindow::connectSignals()
{
    connect(m_checker, &SystemChecker::statusChanged,
            this, &MainWindow::onStatusChanged);

    // Real-time threat detection: add to active threats + single notification
    connect(m_notif, &NotificationService::threatDetectedInLog,
            this, [this](const QString &filePath, const QString &threat) {
        // Add to persistent threat log (addThreat deduplicates internally)
        ActiveThreatsPage::addThreat(filePath, threat,
                                     m_clam->isClamonaacRunning());
        // Send one notification (deduplication is already done in NotificationService)
        m_notif->send(tr("Threat detected!"),
                      tr("ClamAV: %1 in %2").arg(threat,
                          QFileInfo(filePath).fileName().isEmpty()
                              ? filePath : QFileInfo(filePath).fileName()),
                      "security-low", 2);
        // Auto-refresh the page if it's currently open
        if (m_stack->currentWidget() == m_activeThreatsPage)
            m_activeThreatsPage->refresh();
        // Update status
        m_checker->refresh();
        // Update active threats card subtitle
        if (m_cardActiveThreats) {
            m_cardActiveThreats->setSubtitle(tr("New threat!"));
            m_cardActiveThreats->setStatusColor(QColor(0xC6, 0x28, 0x28));
        }
    });

    // Config saved notification
    connect(m_cfgMgr, &ClamdConfigManager::configSaved,
            this, [this](bool success, const QString &msg) {
        if (!success) {
            m_tray->showMessage(tr("ClamSecurity"),
                                tr("Configuration error: %1").arg(msg),
                                QSystemTrayIcon::Warning, 4000);
        }
    });

    // Back navigation from each page
    connect(m_scanPage,           &ScanPage::backRequested,
            this, &MainWindow::navigateBack);
    connect(m_dbPage,             &DatabasePage::backRequested,
            this, &MainWindow::navigateBack);
    connect(m_exclusionsPage,     &ExclusionsPage::backRequested,
            this, &MainWindow::navigateBack);
    connect(m_exclusionsPage,     &ExclusionsPage::exclusionsChanged,
            m_schedMgr, &SchedulerManager::syncServiceFiles);
    connect(m_quarantinePage,     &QuarantinePage::backRequested,
            this, &MainWindow::navigateBack);
    connect(m_firewallPage,       &FirewallPage::backRequested,
            this, &MainWindow::navigateBack);
    connect(m_settingsPage,       &SettingsPage::backRequested,
            this, &MainWindow::navigateBack);
    connect(m_activeThreatsPage,    &ActiveThreatsPage::backRequested,
            this, &MainWindow::navigateBack);
    connect(m_scheduledScansPage,   &ScheduledScansPage::backRequested,
            this, &MainWindow::navigateBack);
    connect(m_scheduledScansPage, &ScheduledScansPage::threatNotification,
            this, [this](const QString &scanName, int count, bool autoQuarantined) {
                const QString body = autoQuarantined
                    ? tr("%n threat(s) found in scheduled scan \"%2\".\n"
                         "Files were moved to quarantine automatically.", nullptr, count)
                          .arg(scanName)
                    : tr("%n threat(s) found in scheduled scan \"%2\".\n"
                         "Open the Scheduled Scans panel to take action.", nullptr, count)
                          .arg(scanName);
                m_notif->send(tr("Threat detected"), body, "security-low", 2);
            });

    // Refresh quarantine card immediately when a file is quarantined (e.g. by a
    // scheduled scan running in the background) without waiting for a full status refresh.
    connect(m_quar, &QuarantineManager::quarantineChanged, this, [this]() {
        updateCardSubtitles(m_checker->currentStatus());
    });

    // Refresh scheduler card and status sub-text when a scan log is updated.
    connect(m_schedMgr, &SchedulerManager::scanLogUpdated, this, [this](const QString &) {
        const SystemStatus s = m_checker->currentStatus();
        updateStatusDisplay(s);
        updateCardSubtitles(s);
    });

    connect(m_settingsPage, &SettingsPage::themeChangeRequested,
            this, &MainWindow::applyTheme);
    connect(m_settingsPage, &SettingsPage::serviceStateChanged,
            m_checker, &SystemChecker::refresh);

    m_notif->startLogWatcher();
}

// ─── Status update ───────────────────────────────────────────────────────────

void MainWindow::onStatusChanged(const SystemStatus &status)
{
    updateStatusDisplay(status);
    updateCardSubtitles(status);
}

void MainWindow::updateStatusDisplay(const SystemStatus &status)
{
    MonitorWidget::Status monState;
    QString labelText;
    QColor  labelColor;
    QString trayIcon;

    // Pending scheduled threats require immediate action → always Alert level.
    const ProtectionLevel effectiveLevel =
        (m_schedMgr->scheduledThreats().size() > 0)
            ? ProtectionLevel::Alert
            : status.level;

    switch (effectiveLevel) {
    case ProtectionLevel::Protected:
        monState   = MonitorWidget::Status::Protected;
        labelText  = tr("Your computer is protected");
        labelColor = QColor(0x1B, 0x5E, 0x20);
        trayIcon   = "security-high";
        break;
    case ProtectionLevel::Warning:
        monState   = MonitorWidget::Status::Warning;
        labelText  = tr("Warning: Review recommended");
        labelColor = QColor(0xE6, 0x5C, 0x00);
        trayIcon   = "security-medium";
        break;
    case ProtectionLevel::Alert:
    default:
        monState   = MonitorWidget::Status::Alert;
        labelText  = tr("Attention: Action required");
        labelColor = QColor(0xB7, 0x1C, 0x1C);
        trayIcon   = "security-low";
        break;
    }

    m_monitor->setStatus(monState);
    m_statusLabel->setText(labelText);

    QPalette p = m_statusLabel->palette();
    p.setColor(QPalette::WindowText, labelColor);
    m_statusLabel->setPalette(p);

    QStringList allIssues = status.issues;
    const int stCount = m_schedMgr->scheduledThreats().size();
    if (stCount > 0)
        allIssues << tr("⚠ %n file(s) found by scheduled scans — open Scheduled Scans to take action.",
                        nullptr, stCount);
    m_statusSub->setText(allIssues.isEmpty()
        ? tr("All components are working correctly.")
        : allIssues.join("\n"));

    // Tray icon
    m_tray->setIcon(QIcon::fromTheme(trayIcon, QIcon(":/icons/clamsecurity.svg")));

    bool rtActive = status.realtimeAvailable ? status.realtimeRunning
                                             : status.daemonRunning;
    m_realtimeToggle->blockSignals(true);
    m_realtimeToggle->setChecked(rtActive);
    m_realtimeToggle->setEnabled(status.realtimeAvailable || status.clamavInstalled);
    m_realtimeToggle->blockSignals(false);
    m_trayActRealtime->blockSignals(true);
    m_trayActRealtime->setChecked(rtActive);
    m_trayActRealtime->blockSignals(false);
}

void MainWindow::updateCardSubtitles(const SystemStatus &status)
{
    auto setCard = [](ModuleCard *card, const QString &text, bool ok) {
        card->setSubtitle(text);
        card->setStatusColor(ok ? QColor(0x2E, 0x7D, 0x32)
                                : QColor(0xC6, 0x28, 0x28));
    };

    setCard(m_cardDB,   status.signaturesRecent
                        ? tr("Up to date")   : tr("Outdated"),
            status.signaturesRecent);
    setCard(m_cardScan, status.clamavInstalled
                        ? tr("ClamAV ready") : tr("Not installed"),
            status.clamavInstalled);
    setCard(m_cardQuar,
            tr("%n file(s)", nullptr, m_quar->entryCount()),
            m_quar->entryCount() == 0);
    setCard(m_cardFW,   status.firewallEnabled
                        ? tr("Active")       : tr("Inactive"),
            status.firewallEnabled);

    QStringList exclList = m_clam->exclusions();
    m_cardExcl->setSubtitle(tr("%n excluded path(s)", nullptr, exclList.size()));
    m_cardExcl->setStatusColor(palette().color(QPalette::Text));

    // Scheduler card: highlight pending threats
    int schedCount   = m_schedMgr->schedules().size();
    int pendingThreats = m_schedMgr->scheduledThreats().size();
    if (pendingThreats > 0) {
        m_cardScheduler->setSubtitle(tr("%n pending threat(s)", nullptr, pendingThreats));
        m_cardScheduler->setStatusColor(QColor(0xC6, 0x28, 0x28));
    } else {
        m_cardScheduler->setSubtitle(schedCount == 0
            ? tr("No schedules")
            : tr("%n schedule(s)", nullptr, schedCount));
        m_cardScheduler->setStatusColor(palette().color(QPalette::Text));
    }
}

// ─── Navigation ──────────────────────────────────────────────────────────────

void MainWindow::navigateTo(Page page)
{
    int idx = static_cast<int>(page);
    m_stack->setCurrentIndex(idx);


    // Refresh data when entering a page
    switch (page) {
    case Page::Database:      m_dbPage->refresh();             break;
    case Page::Exclusions:    m_exclusionsPage->refresh();     break;
    case Page::Quarantine:    m_quarantinePage->refresh();     break;
    case Page::Firewall:      m_firewallPage->refresh();       break;
    case Page::Settings:      m_settingsPage->refresh();       break;
    case Page::ActiveThreats:
        m_activeThreatsPage->refresh();
        m_cardActiveThreats->setSubtitle(tr("Real-time detections"));
        m_cardActiveThreats->setStatusColor(palette().color(QPalette::Text));
        break;
    case Page::ScheduledScans:
        m_scheduledScansPage->refresh();
        break;
    default: break;
    }
}

void MainWindow::navigateBack()
{
    m_stack->setCurrentIndex(0);
    m_checker->refresh();
}

// ─── Dialog / details ────────────────────────────────────────────────────────

void MainWindow::onShowDetails()
{
    DetailsDialog dlg(this);
    SystemStatus s = m_checker->currentStatus();
    s.scheduledThreatsCount = m_schedMgr->scheduledThreats().size();
    dlg.updateStatus(s);
    dlg.exec();
}

// ─── Tray slots ──────────────────────────────────────────────────────────────

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger)
        showAndRaise();
}

void MainWindow::showAndRaise()
{
    if (!isVisible()) {
        // Position must be set BEFORE show() on Wayland — the compositor ignores
        // move() calls on already-mapped surfaces. setGeometry centers the client
        // area; the title bar sits above it, making the overall window visually centered.
        const QRect screen = QApplication::primaryScreen()->availableGeometry();
        setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
                                        size(), screen));
    }
    show();
    raise();
    activateWindow();
}

void MainWindow::onTrayActionScanHome()
{
    showAndRaise();
    navigateTo(Page::Scan);
    m_scanPage->startScan(QDir::homePath());
}

void MainWindow::onTrayActionUpdateDB()
{
    showAndRaise();
    navigateTo(Page::Database);
    m_clam->forceUpdate();
}

void MainWindow::onTrayActionToggleRealtime()
{
    bool enable = m_trayActRealtime->isChecked();
    m_trayActRealtime->setEnabled(false);
    if (m_clam->isClamonaacAvailable())
        m_clam->setClamonaacEnabled(enable);
    else
        m_clam->setDaemonEnabled(enable);
    QTimer::singleShot(5000, this, [this]() {
        m_trayActRealtime->setEnabled(true);
        m_checker->refresh();
    });
}

void MainWindow::onTrayActionQuit()
{
    m_notif->stopLogWatcher();
    QApplication::quit();
}

// ─── Window close ────────────────────────────────────────────────────────────

void MainWindow::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
    m_tray->showMessage(
        tr("ClamSecurity"),
        tr("ClamSecurity is still running in the System Tray."),
        QSystemTrayIcon::Information, 3000);
}

// ─── Theme ───────────────────────────────────────────────────────────────────

void MainWindow::applyTheme(int theme)
{
    QPalette pal;
    if (theme == 1) {
        pal = QStyleFactory::create("Fusion")->standardPalette();
    } else if (theme == 2) {
        pal = QStyleFactory::create("Fusion")->standardPalette();
        pal.setColor(QPalette::Window,          QColor(0x2D, 0x2D, 0x2D));
        pal.setColor(QPalette::WindowText,      Qt::white);
        pal.setColor(QPalette::Base,            QColor(0x1E, 0x1E, 0x1E));
        pal.setColor(QPalette::AlternateBase,   QColor(0x2A, 0x2A, 0x2A));
        pal.setColor(QPalette::Text,            Qt::white);
        pal.setColor(QPalette::Button,          QColor(0x3C, 0x3C, 0x3C));
        pal.setColor(QPalette::ButtonText,      Qt::white);
        pal.setColor(QPalette::Highlight,       QColor(0x42, 0xA5, 0xF5));
        pal.setColor(QPalette::HighlightedText, Qt::black);
        pal.setColor(QPalette::Mid,             QColor(0x55, 0x55, 0x55));
        pal.setColor(QPalette::Dark,            QColor(0x20, 0x20, 0x20));
        pal.setColor(QPalette::Shadow,          QColor(0x10, 0x10, 0x10));
    } else {
        pal = QApplication::style()->standardPalette();
    }
    QApplication::setPalette(pal);
}

// ─── startScanWithPath ────────────────────────────────────────────────────────

void MainWindow::startScanWithPath(const QString &path)
{
    navigateTo(Page::Scan);
    m_scanPage->startScan(path);
}
