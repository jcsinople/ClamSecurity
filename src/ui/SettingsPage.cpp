#include "SettingsPage.h"
#include "AutostartManager.h"
#include "ClamAvManager.h"
#include "LanguageManager.h"
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

SettingsPage::SettingsPage(AutostartManager *autostart,
                            ClamAvManager    *clam,
                            LanguageManager  *langMgr,
                            QWidget *parent)
    : QWidget(parent), m_autostart(autostart), m_clam(clam), m_langMgr(langMgr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *title = new QLabel(tr("Settings"), this);
    QFont f = title->font(); f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    // --- Autostart ---
    auto *startGroup = new QGroupBox(tr("System Startup"), this);
    auto *startLay   = new QVBoxLayout(startGroup);
    m_chkAutostart   = new QCheckBox(tr("Start ClamSecurity with the system"), this);
    m_chkStartHidden = new QCheckBox(tr("Start hidden in System Tray"), this);
    m_chkStartHidden->setEnabled(false);
    startLay->addWidget(m_chkAutostart);
    startLay->addWidget(m_chkStartHidden);
    layout->addWidget(startGroup);

    // --- Theme ---
    auto *themeGroup = new QGroupBox(tr("Visual Theme"), this);
    auto *themeLay   = new QHBoxLayout(themeGroup);
    m_themeGroup  = new QButtonGroup(this);
    m_radioSystem = new QRadioButton(tr("System"), this);
    m_radioLight  = new QRadioButton(tr("Light"),  this);
    m_radioDark   = new QRadioButton(tr("Dark"),   this);
    m_themeGroup->addButton(m_radioSystem, 0);
    m_themeGroup->addButton(m_radioLight,  1);
    m_themeGroup->addButton(m_radioDark,   2);
    themeLay->addWidget(m_radioSystem);
    themeLay->addWidget(m_radioLight);
    themeLay->addWidget(m_radioDark);
    layout->addWidget(themeGroup);

    // --- Language ---
    auto *langGroup = new QGroupBox(tr("Language"), this);
    auto *langLay   = new QHBoxLayout(langGroup);
    langLay->addWidget(new QLabel(tr("Interface language:"), this));
    m_langCombo = new QComboBox(this);
    for (const auto &lang : LanguageManager::available())
        m_langCombo->addItem(lang.name, lang.code);
    langLay->addWidget(m_langCombo);
    langLay->addStretch();
    layout->addWidget(langGroup);

    // --- ClamAV service ---
    auto *clamGroup = new QGroupBox(tr("ClamAV Service"), this);
    auto *clamLay   = new QVBoxLayout(clamGroup);
    m_chkRealtime = new QCheckBox(tr("Enable Real-Time Protection (clamav-daemon)"), this);
    clamLay->addWidget(m_chkRealtime);

    m_btnRestartDaemon = new QPushButton(
        QIcon::fromTheme("system-restart"),
        tr("Restart clamav-daemon (requires password)"), this);
    m_btnInstallMenu = new QPushButton(
        QIcon::fromTheme("document-save"),
        tr("Install Dolphin Service Menu"), this);
    clamLay->addWidget(m_btnRestartDaemon);
    clamLay->addWidget(m_btnInstallMenu);
    layout->addWidget(clamGroup);

    layout->addStretch();

    auto *navRow = new QHBoxLayout;
    m_btnBack = new QPushButton(QIcon::fromTheme("go-previous"), tr("Back"), this);
    navRow->addWidget(m_btnBack);
    navRow->addStretch();
    layout->addLayout(navRow);

    connect(m_chkAutostart,   &QCheckBox::toggled,
            this, &SettingsPage::onAutostartToggled);
    connect(m_chkStartHidden, &QCheckBox::toggled,
            this, &SettingsPage::onStartHiddenToggled);
    connect(m_themeGroup, &QButtonGroup::idClicked,
            this, &SettingsPage::onThemeChanged);
    connect(m_langCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SettingsPage::onLanguageChanged);
    connect(m_chkRealtime, &QCheckBox::toggled, this, [this](bool checked) {
        m_chkRealtime->setEnabled(false);
        m_clam->setDaemonEnabled(checked);
        QTimer::singleShot(4000, m_chkRealtime, [this]() {
            m_chkRealtime->setEnabled(true);
            m_chkRealtime->blockSignals(true);
            m_chkRealtime->setChecked(m_clam->isDaemonRunning());
            m_chkRealtime->blockSignals(false);
        });
    });
    connect(m_btnRestartDaemon, &QPushButton::clicked,
            this, &SettingsPage::onRestartDaemon);
    connect(m_btnInstallMenu, &QPushButton::clicked,
            this, &SettingsPage::onInstallServiceMenu);
    connect(m_btnBack, &QPushButton::clicked,
            this, &SettingsPage::backRequested);

    refresh();
}

void SettingsPage::refresh()
{
    m_chkAutostart->blockSignals(true);
    m_chkStartHidden->blockSignals(true);
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

    // Select current language in combo
    QString cur = m_langMgr->current();
    for (int i = 0; i < m_langCombo->count(); ++i) {
        if (m_langCombo->itemData(i).toString() == cur) {
            m_langCombo->setCurrentIndex(i);
            break;
        }
    }

    m_chkRealtime->setChecked(m_clam->isDaemonRunning());
    m_chkRealtime->blockSignals(false);
    m_chkAutostart->blockSignals(false);
    m_chkStartHidden->blockSignals(false);
    m_langCombo->blockSignals(false);
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
        // Restart the application to apply the new language immediately
        QProcess::startDetached(QApplication::applicationFilePath(),
                                QApplication::arguments());
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

    // Use absolute path to the binary so Dolphin/KIO can find it regardless of PATH
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
        "Exec=" + appExec + " --scan %F\n";  // %F = multiple files support

    QFile f(destFile);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << content;
        f.close();
        // KDE requires +x on user-local service menu files as a trust marker
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
