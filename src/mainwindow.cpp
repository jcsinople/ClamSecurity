#include "mainwindow.h"
#include "ClamAvManager.h"
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("ClamSecurity");
    setWindowIcon(QIcon::fromTheme("security-high",
                                   QIcon(":/icons/clamsecurity.svg")));
    setMinimumSize(720, 560);
    resize(800, 600);

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
    m_checker   = new SystemChecker(m_clam, m_ufw, m_quar, this);
    m_langMgr   = new LanguageManager(qApp, this);
}

// ─── UI ──────────────────────────────────────────────────────────────────────

void MainWindow::setupUI()
{
    m_stack = new QStackedWidget(this);

    // Build pages
    m_scanPage       = new ScanPage(m_clam, m_quar);
    m_dbPage         = new DatabasePage(m_clam);
    m_exclusionsPage = new ExclusionsPage(m_clam);
    m_quarantinePage = new QuarantinePage(m_quar);
    m_firewallPage   = new FirewallPage(m_ufw);
    m_settingsPage   = new SettingsPage(m_autostart, m_clam, m_langMgr);

    m_stack->addWidget(buildOverviewPage());  // index 0
    m_stack->addWidget(m_scanPage);           // 1
    m_stack->addWidget(m_dbPage);             // 2
    m_stack->addWidget(m_exclusionsPage);     // 3
    m_stack->addWidget(m_quarantinePage);     // 4
    m_stack->addWidget(m_firewallPage);       // 5
    m_stack->addWidget(m_settingsPage);       // 6

    setCentralWidget(m_stack);

    // Toolbar with back button
    m_toolbar = addToolBar(tr("Navigation"));
    m_toolbar->setMovable(false);
    m_toolbar->setVisible(false);
    m_actBack = m_toolbar->addAction(
        QIcon::fromTheme("go-previous"), tr("Back to overview"));
    connect(m_actBack, &QAction::triggered, this, &MainWindow::navigateBack);
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

    statusBox->addStretch();
    statusBox->addWidget(m_statusLabel);
    statusBox->addWidget(m_statusSub);
    statusBox->addSpacing(8);
    statusBox->addWidget(m_btnDetails, 0, Qt::AlignLeft);
    statusBox->addStretch();

    topPanel->addLayout(statusBox, 1);
    layout->addLayout(topPanel);

    // Separator
    auto *sep = new QFrame(page);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    layout->addWidget(sep);

    // ── Module cards grid ────────────────────────────────────
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
    m_cardFW    = makeCard("network-firewall",       tr("Firewall"),
                            tr("UFW control"),       Page::Firewall);
    m_cardConf  = makeCard("configure",              tr("Settings"),
                            tr("App settings"),      Page::Settings);

    grid->addWidget(m_cardScan,  0, 0);
    grid->addWidget(m_cardDB,    0, 1);
    grid->addWidget(m_cardExcl,  0, 2);
    grid->addWidget(m_cardQuar,  1, 0);
    grid->addWidget(m_cardFW,    1, 1);
    grid->addWidget(m_cardConf,  1, 2);

    layout->addLayout(grid);
    layout->addStretch();

    connect(m_btnDetails, &QPushButton::clicked, this, &MainWindow::onShowDetails);

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

    connect(m_notif, &NotificationService::threatDetectedInLog,
            this, [this](const QString &, const QString &threat) {
        m_notif->send(tr("¡Amenaza detectada!"),
                      tr("ClamAV bloqueó: %1").arg(threat),
                      "security-low", 2);
        m_checker->refresh();
    });

    // Back navigation from each page
    connect(m_scanPage,       &ScanPage::backRequested,       this, &MainWindow::navigateBack);
    connect(m_dbPage,         &DatabasePage::backRequested,   this, &MainWindow::navigateBack);
    connect(m_exclusionsPage, &ExclusionsPage::backRequested, this, &MainWindow::navigateBack);
    connect(m_quarantinePage, &QuarantinePage::backRequested, this, &MainWindow::navigateBack);
    connect(m_firewallPage,   &FirewallPage::backRequested,   this, &MainWindow::navigateBack);
    connect(m_settingsPage,   &SettingsPage::backRequested,   this, &MainWindow::navigateBack);

    connect(m_settingsPage, &SettingsPage::themeChangeRequested,
            this, &MainWindow::applyTheme);

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
    bool ok = status.overallProtected;
    m_monitor->setStatus(ok ? MonitorWidget::Status::Protected
                            : MonitorWidget::Status::Alert);

    m_statusLabel->setText(ok ? tr("Your computer is protected")
                              : tr("Attention: Action required"));

    QPalette p = m_statusLabel->palette();
    p.setColor(QPalette::WindowText,
               ok ? QColor(0x1B, 0x5E, 0x20) : QColor(0xB7, 0x1C, 0x1C));
    m_statusLabel->setPalette(p);

    if (status.issues.isEmpty())
        m_statusSub->setText(tr("All components are working correctly."));
    else
        m_statusSub->setText(status.issues.join("\n"));

    // Tray icon
    m_tray->setIcon(QIcon::fromTheme(
        ok ? "security-high" : "security-low",
        QIcon(":/icons/clamsecurity.svg")));
    m_trayActRealtime->setChecked(status.daemonRunning);
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
}

// ─── Navigation ──────────────────────────────────────────────────────────────

void MainWindow::navigateTo(Page page)
{
    int idx = static_cast<int>(page);
    m_stack->setCurrentIndex(idx);
    m_toolbar->setVisible(idx != 0);

    // Refresh data when entering a page
    switch (page) {
    case Page::Database:   m_dbPage->refresh();         break;
    case Page::Exclusions: m_exclusionsPage->refresh(); break;
    case Page::Quarantine: m_quarantinePage->refresh(); break;
    case Page::Firewall:   m_firewallPage->refresh();   break;
    case Page::Settings:   m_settingsPage->refresh();   break;
    default: break;
    }
}

void MainWindow::navigateBack()
{
    m_stack->setCurrentIndex(0);
    m_toolbar->setVisible(false);
    m_checker->refresh();
}

// ─── Dialog / details ────────────────────────────────────────────────────────

void MainWindow::onShowDetails()
{
    DetailsDialog dlg(this);
    dlg.updateStatus(m_checker->currentStatus());
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
    m_clam->setDaemonEnabled(enable);
    QTimer::singleShot(3000, m_checker, &SystemChecker::refresh);
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
        // Light
        pal = QStyleFactory::create("Fusion")->standardPalette();
    } else if (theme == 2) {
        // Dark
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
        // System default
        pal = QApplication::style()->standardPalette();
    }
    QApplication::setPalette(pal);
}

// ─── startScanWithPath (called from main.cpp for --scan argument) ─────────────

void MainWindow::startScanWithPath(const QString &path)
{
    navigateTo(Page::Scan);
    m_scanPage->startScan(path);
}
