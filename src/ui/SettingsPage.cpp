#include "SettingsPage.h"
#include "AutostartManager.h"
#include "ClamAvManager.h"
#include "LanguageManager.h"
#include "ClamdConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFont>
#include <QMessageBox>
#include <QProcess>
#include <QTimer>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QApplication>
#include <QCoreApplication>
#include <QFileDialog>
#include <QScrollArea>
#include <QRegularExpression>
#include <QFrame>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QUrl>
#include <QLocalServer>

static const char *APP_VERSION = "1.0.0";

// ── Constructor ────────────────────────────────────────────────────────────────

SettingsPage::SettingsPage(AutostartManager   *autostart,
                            ClamAvManager      *clam,
                            LanguageManager    *langMgr,
                            ClamdConfigManager *cfgMgr,
                            QWidget *parent)
    : QWidget(parent), m_autostart(autostart), m_clam(clam),
      m_langMgr(langMgr), m_cfgMgr(cfgMgr)
{
    m_nam = new QNetworkAccessManager(this);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(20, 20, 20, 20);
    outerLayout->setSpacing(12);

    auto *title = new QLabel(tr("Settings"), this);
    QFont f = title->font(); f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    outerLayout->addWidget(title);

    // ── Tab widget ────────────────────────────────────────────────────────
    auto *tabs = new QTabWidget(this);

    auto *generalTab    = new QWidget(tabs);
    auto *protectionTab = new QWidget(tabs);
    auto *aboutTab      = new QWidget(tabs);

    buildGeneralTab(generalTab);
    buildProtectionTab(protectionTab);
    buildAboutTab(aboutTab);

    tabs->addTab(generalTab,    tr("General"));
    tabs->addTab(protectionTab, tr("Protection"));
    tabs->addTab(aboutTab,      tr("About"));

    outerLayout->addWidget(tabs, 1);

    // Back button outside tabs
    auto *navRow = new QHBoxLayout;
    m_btnBack = new QPushButton(QIcon::fromTheme("go-previous"), tr("Back"), this);
    navRow->addWidget(m_btnBack);
    navRow->addStretch();
    outerLayout->addLayout(navRow);

    connect(m_btnBack, &QPushButton::clicked, this, &SettingsPage::backRequested);

    refresh();
}

// ── Build tabs ─────────────────────────────────────────────────────────────────

void SettingsPage::buildGeneralTab(QWidget *tab)
{
    auto *scroll = new QScrollArea(tab);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *w      = new QWidget(scroll);
    auto *layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);

    // --- Autostart ---
    auto *startGroup = new QGroupBox(tr("System Startup"), w);
    auto *startLay   = new QVBoxLayout(startGroup);
    m_chkAutostart   = new QCheckBox(tr("Start ClamSecurity with the system"), w);
    m_chkStartHidden = new QCheckBox(tr("Start hidden in System Tray"), w);
    m_chkStartHidden->setEnabled(false);
    startLay->addWidget(m_chkAutostart);
    startLay->addWidget(m_chkStartHidden);
    layout->addWidget(startGroup);

    // --- Theme ---
    auto *themeGroup = new QGroupBox(tr("Visual Theme"), w);
    auto *themeLay   = new QHBoxLayout(themeGroup);
    m_themeGroup  = new QButtonGroup(this);
    m_radioSystem = new QRadioButton(tr("System"), w);
    m_radioLight  = new QRadioButton(tr("Light"),  w);
    m_radioDark   = new QRadioButton(tr("Dark"),   w);
    m_themeGroup->addButton(m_radioSystem, 0);
    m_themeGroup->addButton(m_radioLight,  1);
    m_themeGroup->addButton(m_radioDark,   2);
    themeLay->addWidget(m_radioSystem);
    themeLay->addWidget(m_radioLight);
    themeLay->addWidget(m_radioDark);
    layout->addWidget(themeGroup);

    // --- Language ---
    auto *langGroup = new QGroupBox(tr("Language"), w);
    auto *langLay   = new QHBoxLayout(langGroup);
    langLay->addWidget(new QLabel(tr("Interface language:"), w));
    m_langCombo = new QComboBox(w);
    for (const auto &lang : LanguageManager::available())
        m_langCombo->addItem(lang.name, lang.code);
    langLay->addWidget(m_langCombo);
    langLay->addStretch();
    layout->addWidget(langGroup);

    // --- ClamAV service ---
    auto *clamGroup = new QGroupBox(tr("ClamAV Service"), w);
    auto *clamLay   = new QVBoxLayout(clamGroup);
    m_chkDaemon = new QCheckBox(tr("ClamAV Daemon (clamav-daemon)"), w);
    m_chkDaemon->setToolTip(tr("Scanning backend — required for on-demand scans "
                               "and real-time protection."));
    clamLay->addWidget(m_chkDaemon);

    m_chkRealtime = new QCheckBox(tr("Real-Time Protection (clamav-clamonacc)"), w);
    m_chkRealtime->setToolTip(tr("On-access scanning via ClamOnAcc. "
                                 "Requires clamav-daemon to be running."));
    clamLay->addWidget(m_chkRealtime);

    m_btnRestartDaemon = new QPushButton(
        QIcon::fromTheme("system-restart"),
        tr("Restart clamav-daemon (requires password)"), w);
    m_btnInstallMenu = new QPushButton(
        QIcon::fromTheme("document-save"),
        tr("Install Dolphin Service Menu"), w);
    clamLay->addWidget(m_btnRestartDaemon);
    clamLay->addWidget(m_btnInstallMenu);
    layout->addWidget(clamGroup);

    layout->addStretch();
    scroll->setWidget(w);

    auto *tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scroll);

    // Connections
    connect(m_chkAutostart,   &QCheckBox::toggled,
            this, &SettingsPage::onAutostartToggled);
    connect(m_chkStartHidden, &QCheckBox::toggled,
            this, &SettingsPage::onStartHiddenToggled);
    connect(m_themeGroup, &QButtonGroup::idClicked,
            this, &SettingsPage::onThemeChanged);
    connect(m_langCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SettingsPage::onLanguageChanged);

    connect(m_chkDaemon, &QCheckBox::toggled, this, [this](bool checked) {
        m_chkDaemon->setEnabled(false);
        m_clam->setDaemonEnabled(checked);
        QTimer::singleShot(4000, m_chkDaemon, [this]() {
            m_chkDaemon->setEnabled(true);
            m_chkDaemon->blockSignals(true);
            m_chkDaemon->setChecked(m_clam->isDaemonRunning());
            m_chkDaemon->blockSignals(false);
            emit serviceStateChanged();
        });
    });
    connect(m_chkRealtime, &QCheckBox::toggled, this, [this](bool checked) {
        m_chkRealtime->setEnabled(false);
        m_clam->setClamonaacEnabled(checked);
        QTimer::singleShot(4000, m_chkRealtime, [this]() {
            m_chkRealtime->setEnabled(true);
            m_chkRealtime->blockSignals(true);
            m_chkRealtime->setChecked(m_clam->isClamonaacRunning());
            m_chkRealtime->blockSignals(false);
            emit serviceStateChanged();
        });
    });
    connect(m_btnRestartDaemon, &QPushButton::clicked,
            this, &SettingsPage::onRestartDaemon);
    connect(m_btnInstallMenu, &QPushButton::clicked,
            this, &SettingsPage::onInstallServiceMenu);
}

void SettingsPage::buildProtectionTab(QWidget *tab)
{
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);

    m_protStack = new QStackedWidget(tab);

    // ── Basic view (index 0) — wrapped in scroll area ─────────────────────
    auto *basicScroll = new QScrollArea(m_protStack);
    basicScroll->setWidgetResizable(true);
    basicScroll->setFrameShape(QFrame::NoFrame);
    auto *basicWidget = new QWidget(basicScroll);
    auto *basicLay    = new QVBoxLayout(basicWidget);
    basicLay->setContentsMargins(4, 4, 4, 4);
    basicLay->setSpacing(10);

    // OnAccessPrevention group
    auto *prevGroup = new QGroupBox(tr("On-Access Protection"), basicWidget);
    auto *prevLay   = new QVBoxLayout(prevGroup);

    m_chkPrevention = new QCheckBox(
        tr("Block threats automatically (OnAccessPrevention)"), basicWidget);
    m_lblPreventionInfo = new QLabel(basicWidget);
    m_lblPreventionInfo->setWordWrap(true);

    prevLay->addWidget(m_chkPrevention);
    prevLay->addWidget(m_lblPreventionInfo);
    basicLay->addWidget(prevGroup);

    // Monitored folders group
    auto *watchGroup = new QGroupBox(tr("Monitored Folders"), basicWidget);
    auto *watchLay   = new QVBoxLayout(watchGroup);

    m_watchedList = new QListWidget(basicWidget);
    watchLay->addWidget(m_watchedList);

    auto *watchActRow  = new QHBoxLayout;
    m_btnAddWatched    = new QPushButton(QIcon::fromTheme("folder-new"),
                                         tr("Add Folder"), basicWidget);
    m_btnRemoveWatched = new QPushButton(QIcon::fromTheme("list-remove"),
                                         tr("Remove Selected"), basicWidget);
    m_btnRemoveWatched->setEnabled(false);
    watchActRow->addWidget(m_btnAddWatched);
    watchActRow->addStretch();
    watchActRow->addWidget(m_btnRemoveWatched);
    watchLay->addLayout(watchActRow);

    watchLay->addWidget(new QLabel(
        tr("Note: Exclusions are synchronized from the Scan Exclusions page."), basicWidget));

    basicLay->addWidget(watchGroup);

    m_btnViewAdvanced = new QPushButton(tr("Show Advanced Settings"), basicWidget);
    basicLay->addWidget(m_btnViewAdvanced);
    basicLay->addStretch();
    basicScroll->setWidget(basicWidget);
    m_protStack->addWidget(basicScroll);

    // ── Advanced view (index 1) ───────────────────────────────────────────
    auto *advScroll = new QScrollArea(m_protStack);
    advScroll->setWidgetResizable(true);
    auto *advWidget = new QWidget(advScroll);
    auto *advLay    = new QVBoxLayout(advWidget);
    advLay->setContentsMargins(8, 8, 8, 8);
    advLay->setSpacing(10);

    auto *advGroup = new QGroupBox(tr("Advanced On-Access Parameters"), advWidget);
    auto *advGLay  = new QVBoxLayout(advGroup);

    // MaxFileSize
    auto *fsRow = new QHBoxLayout;
    fsRow->addWidget(new QLabel(tr("Max file size to scan (MB):"), advWidget));
    m_spinMaxFileSize = new QSpinBox(advWidget);
    m_spinMaxFileSize->setRange(1, 100);
    m_spinMaxFileSize->setValue(10);
    fsRow->addWidget(m_spinMaxFileSize);
    fsRow->addStretch();
    advGLay->addLayout(fsRow);

    // MaxThreads
    auto *thrRow = new QHBoxLayout;
    thrRow->addWidget(new QLabel(tr("Max scanning threads:"), advWidget));
    m_spinMaxThreads = new QSpinBox(advWidget);
    m_spinMaxThreads->setRange(1, 16);
    m_spinMaxThreads->setValue(4);
    thrRow->addWidget(m_spinMaxThreads);
    thrRow->addStretch();
    advGLay->addLayout(thrRow);

    // DenyOnError
    m_chkDenyOnError = new QCheckBox(tr("Deny access on scan error (DenyOnError)"), advWidget);
    advGLay->addWidget(m_chkDenyOnError);

    // ExcludeUname
    auto *unameRow = new QHBoxLayout;
    unameRow->addWidget(new QLabel(tr("Exclude user name (ExcludeUname):"), advWidget));
    m_editExcludeUname = new QLineEdit(advWidget);
    m_editExcludeUname->setText("clamav");
    unameRow->addWidget(m_editExcludeUname);
    advGLay->addLayout(unameRow);

    // ExtraScanning
    m_chkExtraScanning = new QCheckBox(tr("Extra scanning (OnAccessExtraScanning)"), advWidget);
    advGLay->addWidget(m_chkExtraScanning);

    // DisableDDD
    m_chkDisableDDD = new QCheckBox(tr("Disable dynamic directory determination (DisableDDD)"), advWidget);
    advGLay->addWidget(m_chkDisableDDD);

    advLay->addWidget(advGroup);

    m_btnViewBasic = new QPushButton(tr("Show Basic Settings"), advWidget);
    advLay->addWidget(m_btnViewBasic);
    advLay->addStretch();

    advScroll->setWidget(advWidget);
    m_protStack->addWidget(advScroll);

    layout->addWidget(m_protStack, 1);

    // Save & Apply row
    auto *saveRow      = new QHBoxLayout;
    m_btnSaveProtection = new QPushButton(
        QIcon::fromTheme("document-save"),
        tr("Save && Apply"), tab);
    m_lblProtStatus    = new QLabel(tab);
    saveRow->addWidget(m_btnSaveProtection);
    saveRow->addWidget(m_lblProtStatus);
    saveRow->addStretch();
    layout->addLayout(saveRow);

    // Connections
    connect(m_chkPrevention, &QCheckBox::toggled,
            this, &SettingsPage::onPreventionToggled);
    connect(m_btnAddWatched, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select folder to monitor"), QDir::homePath(),
            QFileDialog::ShowDirsOnly);
        if (!dir.isEmpty())
            m_watchedList->addItem(dir);
    });
    connect(m_btnRemoveWatched, &QPushButton::clicked, this, [this]() {
        for (auto *item : m_watchedList->selectedItems())
            delete item;
    });
    connect(m_watchedList, &QListWidget::itemSelectionChanged, this, [this]() {
        m_btnRemoveWatched->setEnabled(!m_watchedList->selectedItems().isEmpty());
    });
    connect(m_btnViewAdvanced, &QPushButton::clicked, this, [this]() {
        m_protStack->setCurrentIndex(1);
    });
    connect(m_btnViewBasic, &QPushButton::clicked, this, [this]() {
        m_protStack->setCurrentIndex(0);
    });
    connect(m_btnSaveProtection, &QPushButton::clicked,
            this, &SettingsPage::onSaveProtection);

    // ExcludeUname warning
    connect(m_editExcludeUname, &QLineEdit::textChanged, this, [this](const QString &val) {
        if (val != "clamav") {
            QMessageBox::warning(this, tr("Warning"),
                tr("Changing this value may cause ClamAV to scan itself, "
                   "leading to infinite loops or performance issues.\n\n"
                   "The default value 'clamav' is strongly recommended."));
        }
    });

    // Connect configSaved signal to status label
    connect(m_cfgMgr, &ClamdConfigManager::configSaved,
            this, [this](bool success, const QString &msg) {
        m_lblProtStatus->setText(msg);
        QPalette p = m_lblProtStatus->palette();
        p.setColor(QPalette::WindowText,
                   success ? QColor(0x2E, 0x7D, 0x32) : QColor(0xC6, 0x28, 0x28));
        m_lblProtStatus->setPalette(p);
    });
}

void SettingsPage::buildAboutTab(QWidget *tab)
{
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(12, 20, 12, 12);
    layout->setSpacing(14);

    // Logo / icon
    auto *iconLabel = new QLabel(tab);
    QIcon appIcon = QIcon::fromTheme("security-high",
                                     QIcon(":/icons/clamsecurity.svg"));
    iconLabel->setPixmap(appIcon.pixmap(64, 64));
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    // App name
    auto *nameLabel = new QLabel("<b>ClamSecurity v" + QString(APP_VERSION) + "</b>", tab);
    QFont nf = nameLabel->font(); nf.setPointSize(14);
    nameLabel->setFont(nf);
    nameLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(nameLabel);

    // Description
    auto *descLabel = new QLabel(
        tr("Open-source security front-end for ClamAV on Linux"), tab);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    // GitHub link
    auto *linkLabel = new QLabel(
        "<a href=\"https://github.com/jcsinople/ClamSecurity\">"
        "https://github.com/jcsinople/ClamSecurity</a>", tab);
    linkLabel->setOpenExternalLinks(true);
    linkLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(linkLabel);

    // ClamAV version
    QString clamVer = ClamAvManager::version();
    auto *clamLabel = new QLabel(tr("ClamAV: %1").arg(clamVer.isEmpty()
                                                      ? tr("not installed") : clamVer), tab);
    clamLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(clamLabel);

    layout->addSpacing(10);

    // Check for updates
    auto *updateRow  = new QHBoxLayout;
    m_btnCheckUpdates = new QPushButton(
        QIcon::fromTheme("system-software-update"),
        tr("Check for Updates"), tab);
    updateRow->addStretch();
    updateRow->addWidget(m_btnCheckUpdates);
    updateRow->addStretch();
    layout->addLayout(updateRow);

    m_lblUpdateStatus = new QLabel(tab);
    m_lblUpdateStatus->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_lblUpdateStatus);

    layout->addStretch();

    connect(m_btnCheckUpdates, &QPushButton::clicked,
            this, &SettingsPage::onCheckForUpdates);
}

// ── refresh ────────────────────────────────────────────────────────────────────

void SettingsPage::refresh()
{
    // General tab
    m_chkAutostart->blockSignals(true);
    m_chkStartHidden->blockSignals(true);
    m_chkDaemon->blockSignals(true);
    m_chkRealtime->blockSignals(true);
    m_langCombo->blockSignals(true);

    m_chkAutostart->setChecked(m_autostart->isAutostartEnabled());
    m_chkStartHidden->setChecked(m_autostart->isStartHidden());
    m_chkStartHidden->setEnabled(m_chkAutostart->isChecked());

    QSettings s;
    int theme = s.value("ui/theme", 0).toInt();
    if      (theme == 1) m_radioLight->setChecked(true);
    else if (theme == 2) m_radioDark->setChecked(true);
    else                 m_radioSystem->setChecked(true);

    QString cur = s.value("ui/language", "en").toString();
    for (int i = 0; i < m_langCombo->count(); ++i) {
        if (m_langCombo->itemData(i).toString() == cur) {
            m_langCombo->setCurrentIndex(i);
            break;
        }
    }

    m_chkDaemon->setChecked(m_clam->isDaemonRunning());
    m_chkDaemon->blockSignals(false);

    bool rtAvail = m_clam->isClamonaacAvailable();
    m_chkRealtime->setEnabled(rtAvail);
    m_chkRealtime->setChecked(rtAvail && m_clam->isClamonaacRunning());
    if (!rtAvail)
        m_chkRealtime->setText(
            tr("Real-Time Protection (clamav-clamonacc — not configured on this system)"));
    m_chkRealtime->blockSignals(false);
    m_chkAutostart->blockSignals(false);
    m_chkStartHidden->blockSignals(false);
    m_langCombo->blockSignals(false);

    // Protection tab
    loadProtectionSettings();
}

void SettingsPage::loadProtectionSettings()
{
    ClamdOnAccessConfig cfg = m_cfgMgr->readConfig();

    m_chkPrevention->blockSignals(true);
    m_chkPrevention->setChecked(cfg.preventionEnabled);
    m_chkPrevention->blockSignals(false);

    // Update prevention info label
    if (cfg.preventionEnabled)
        m_lblPreventionInfo->setText(
            tr("ClamAV will physically block access to infected files."));
    else
        m_lblPreventionInfo->setText(
            tr("ClamAV will detect threats but not prevent access to infected files."));

    // Watched paths
    m_watchedList->clear();
    for (const QString &p : cfg.includePaths)
        m_watchedList->addItem(p);

    // Advanced params
    m_spinMaxFileSize->setValue(cfg.maxFileSizeMB);
    m_spinMaxThreads->setValue(cfg.maxThreads);
    m_chkDenyOnError->setChecked(cfg.denyOnError);
    m_editExcludeUname->blockSignals(true);
    m_editExcludeUname->setText(cfg.excludeUname);
    m_editExcludeUname->blockSignals(false);
    m_chkExtraScanning->setChecked(cfg.extraScanning);
    m_chkDisableDDD->setChecked(cfg.disableDDD);
}

// ── Slots ──────────────────────────────────────────────────────────────────────

void SettingsPage::onPreventionToggled(bool checked)
{
    QString msg;
    if (checked) {
        msg = tr("Enabling this will physically block access to infected files.\n"
                 "ClamAV will prevent you from opening any file it identifies as a threat.\n\n"
                 "Are you sure?");
    } else {
        msg = tr("Disabling this will allow ClamAV to detect threats but NOT block them.\n"
                 "Files identified as malicious will still be accessible.\n\n"
                 "Are you sure?");
    }

    if (QMessageBox::question(this, tr("Change Prevention Mode"),
                              msg, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        m_chkPrevention->blockSignals(true);
        m_chkPrevention->setChecked(!checked);
        m_chkPrevention->blockSignals(false);
        return;
    }

    if (checked)
        m_lblPreventionInfo->setText(
            tr("ClamAV will physically block access to infected files."));
    else
        m_lblPreventionInfo->setText(
            tr("ClamAV will detect threats but not prevent access to infected files."));
}

void SettingsPage::onSaveProtection()
{
    ClamdOnAccessConfig cfg;
    cfg.preventionEnabled = m_chkPrevention->isChecked();

    // Collect watched paths from list widget
    for (int i = 0; i < m_watchedList->count(); ++i)
        cfg.includePaths << m_watchedList->item(i)->text();

    // If empty, add default
    if (cfg.includePaths.isEmpty()) {
        QString dl = ClamdConfigManager::defaultIncludePath();
        if (!dl.isEmpty())
            cfg.includePaths << dl;
    }

    cfg.maxFileSizeMB  = m_spinMaxFileSize->value();
    cfg.maxThreads     = m_spinMaxThreads->value();
    cfg.denyOnError    = m_chkDenyOnError->isChecked();
    cfg.excludeUname   = m_editExcludeUname->text().trimmed();
    cfg.extraScanning  = m_chkExtraScanning->isChecked();
    cfg.disableDDD     = m_chkDisableDDD->isChecked();

    // Directories → OnAccessExcludePath, files → ExcludePath (PCRE)
    for (const QString &path : m_clam->exclusions()) {
        if (QFileInfo(path).isDir())
            cfg.onAccessExcludePaths << path;
        else
            cfg.excludeFilePatterns << ("^" + QRegularExpression::escape(path) + "$");
    }
    for (const QString &ext : m_clam->excludedExtensions()) {
        QString escaped = ext;
        escaped.replace('.', "\\.");
        cfg.excludeFilePatterns << (escaped + "$");
    }

    m_lblProtStatus->setText(tr("Saving…"));
    m_cfgMgr->saveConfig(cfg);
}

void SettingsPage::onCheckForUpdates()
{
    m_btnCheckUpdates->setEnabled(false);
    m_lblUpdateStatus->setText(tr("Checking…"));

    QNetworkRequest request(
        QUrl("https://api.github.com/repos/jcsinople/ClamSecurity/releases/latest"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QString("ClamSecurity/%1").arg(APP_VERSION));

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_btnCheckUpdates->setEnabled(true);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            m_lblUpdateStatus->setText(tr("Error: %1").arg(reply->errorString()));
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QString tagName = doc.object()["tag_name"].toString();

        // Strip leading "v" if present
        if (tagName.startsWith('v'))
            tagName = tagName.mid(1);

        if (tagName.isEmpty()) {
            m_lblUpdateStatus->setText(tr("Could not determine latest version."));
            return;
        }

        if (tagName == QLatin1String(APP_VERSION)) {
            m_lblUpdateStatus->setText(tr("Up to date (v%1)").arg(APP_VERSION));
        } else {
            m_lblUpdateStatus->setText(
                tr("New version %1 available! (current: v%2)")
                .arg(tagName, APP_VERSION));
        }
    });
}

void SettingsPage::onAutostartToggled(bool checked)
{
    m_chkStartHidden->setEnabled(checked);
    m_autostart->setAutostartEnabled(checked, m_chkStartHidden->isChecked());
}

void SettingsPage::onStartHiddenToggled(bool checked)
{
    m_autostart->setAutostartEnabled(true, checked);
}

void SettingsPage::onThemeChanged(int id)
{
    QSettings s;
    s.setValue("ui/theme", id);
    emit themeChangeRequested(id);
}

void SettingsPage::onLanguageChanged(int index)
{
    QString code = m_langCombo->itemData(index).toString();
    if (m_langMgr->apply(code)) {
        QMessageBox::information(this, tr("Language changed"),
            tr("The language will be applied after restarting ClamSecurity.\n\n"
               "Restart now?"));
        // Remove the IPC socket so the new instance starts as the primary,
        // not as a second instance that would just bring up this dying window.
        QLocalServer::removeServer(
            QStringLiteral("ClamSecurity-") + QString::fromLocal8Bit(qgetenv("USER")));
        // Start fresh without --hidden so the window is always shown after restart.
        QProcess::startDetached(QApplication::applicationFilePath(), {});
        QApplication::quit();
    }
}

void SettingsPage::onRestartDaemon()
{
    QProcess::startDetached("pkexec",
        {"systemctl", "restart", "clamav-daemon"});
    QMessageBox::information(this, tr("Service"),
        tr("Restart of clamav-daemon was requested.\n"
           "You will be prompted for your password via pkexec."));
}

void SettingsPage::onInstallServiceMenu()
{
    QString destDir  = QDir::homePath() + "/.local/share/kio/servicemenus";
    QString destFile = destDir + "/org.kde.clamsecurity.desktop";
    QDir().mkpath(destDir);

    QString appExec = QCoreApplication::applicationFilePath();

    const QString content =
        "[Desktop Entry]\n"
        "Type=Service\n"
        "ServiceTypes=KonqPopupMenu/Plugin\n"
        "MimeType=all/all;\n"
        "Actions=ScanWithClamSecurity;\n"
        "X-KDE-Priority=TopLevel\n"
        "\n"
        "[Desktop Action ScanWithClamSecurity]\n"
        "Name=Scan with ClamSecurity\n"
        "Name[es]=Escanear con ClamSecurity\n"
        "Icon=security-high\n"
        "Exec=" + appExec + " --scan %F\n";

    QFile f(destFile);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << content;
        f.close();
        f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                         QFile::ReadGroup | QFile::ExeGroup |
                         QFile::ReadOther  | QFile::ExeOther);
        QMessageBox::information(this, tr("Service Menu installed"),
            tr("Installed at:\n%1\n\nRestart Dolphin to activate it.").arg(destFile));
    } else {
        QMessageBox::warning(this, tr("Error"),
            tr("Could not write:\n%1").arg(destFile));
    }
}
