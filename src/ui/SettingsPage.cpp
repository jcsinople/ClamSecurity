#include "SettingsPage.h"
#include "AutostartManager.h"
#include "ClamAvManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFont>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QTextStream>

SettingsPage::SettingsPage(AutostartManager *autostart,
                            ClamAvManager    *clam,
                            QWidget *parent)
    : QWidget(parent), m_autostart(autostart), m_clam(clam)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *title = new QLabel(tr("Configuración"), this);
    QFont f = title->font(); f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    // --- Autostart ---
    auto *startGroup = new QGroupBox(tr("Inicio del Sistema"), this);
    auto *startLay   = new QVBoxLayout(startGroup);
    m_chkAutostart   = new QCheckBox(tr("Iniciar ClamSecurity con el sistema"), this);
    m_chkStartHidden = new QCheckBox(tr("Arrancar oculto en el System Tray"), this);
    m_chkStartHidden->setEnabled(false);
    startLay->addWidget(m_chkAutostart);
    startLay->addWidget(m_chkStartHidden);
    layout->addWidget(startGroup);

    // --- Theme ---
    auto *themeGroup = new QGroupBox(tr("Tema Visual"), this);
    auto *themeLay   = new QHBoxLayout(themeGroup);
    m_themeGroup = new QButtonGroup(this);
    m_radioSystem = new QRadioButton(tr("Sistema"), this);
    m_radioLight  = new QRadioButton(tr("Claro"),   this);
    m_radioDark   = new QRadioButton(tr("Oscuro"),  this);
    m_themeGroup->addButton(m_radioSystem, 0);
    m_themeGroup->addButton(m_radioLight,  1);
    m_themeGroup->addButton(m_radioDark,   2);
    themeLay->addWidget(m_radioSystem);
    themeLay->addWidget(m_radioLight);
    themeLay->addWidget(m_radioDark);
    layout->addWidget(themeGroup);

    // --- ClamAV service ---
    auto *clamGroup = new QGroupBox(tr("Servicio ClamAV"), this);
    auto *clamLay   = new QVBoxLayout(clamGroup);
    m_btnRestartDaemon = new QPushButton(
        QIcon::fromTheme("system-restart"),
        tr("Reiniciar clamav-daemon (requiere contraseña)"), this);
    m_btnInstallMenu = new QPushButton(
        QIcon::fromTheme("document-save"),
        tr("Instalar Service Menu de Dolphin"), this);
    clamLay->addWidget(m_btnRestartDaemon);
    clamLay->addWidget(m_btnInstallMenu);
    layout->addWidget(clamGroup);

    layout->addStretch();

    auto *navRow = new QHBoxLayout;
    m_btnBack = new QPushButton(QIcon::fromTheme("go-previous"), tr("Volver"), this);
    navRow->addWidget(m_btnBack);
    navRow->addStretch();
    layout->addLayout(navRow);

    connect(m_chkAutostart,   &QCheckBox::toggled,
            this, &SettingsPage::onAutostartToggled);
    connect(m_chkStartHidden, &QCheckBox::toggled,
            this, &SettingsPage::onStartHiddenToggled);
    connect(m_themeGroup, &QButtonGroup::idClicked,
            this, &SettingsPage::onThemeChanged);
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
    m_chkAutostart->setChecked(m_autostart->isAutostartEnabled());
    m_chkStartHidden->setChecked(m_autostart->isStartHidden());
    m_chkStartHidden->setEnabled(m_chkAutostart->isChecked());
    m_chkAutostart->blockSignals(false);
    m_chkStartHidden->blockSignals(false);

    QSettings s;
    int theme = s.value("ui/theme", 0).toInt();
    if      (theme == 1) m_radioLight->setChecked(true);
    else if (theme == 2) m_radioDark->setChecked(true);
    else                 m_radioSystem->setChecked(true);
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

void SettingsPage::onRestartDaemon()
{
    QProcess::startDetached("pkexec",
        {"systemctl", "restart", "clamav-daemon"});
    QMessageBox::information(this, tr("Servicio"),
        tr("Se solicitó el reinicio de clamav-daemon.\nSe pedirá tu contraseña mediante pkexec."));
}

void SettingsPage::onInstallServiceMenu()
{
    QString destDir  = QDir::homePath() + "/.local/share/kio/servicemenus";
    QString destFile = destDir + "/org.kde.clamsecurity.desktop";
    QDir().mkpath(destDir);

    const QString content =
        "[Desktop Entry]\n"
        "Type=Service\n"
        "ServiceTypes=KonqPopupMenu/Plugin\n"
        "MimeType=all/all;\n"
        "Actions=ScanWithClamSecurity;\n"
        "X-KDE-Priority=TopLevel\n"
        "\n"
        "[Desktop Action ScanWithClamSecurity]\n"
        "Name=Escanear con ClamSecurity\n"
        "Name[es]=Escanear con ClamSecurity\n"
        "Icon=security-high\n"
        "Exec=ClamSecurity --scan %f\n";

    QFile f(destFile);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << content;
        QMessageBox::information(this, tr("Service Menu instalado"),
            tr("Instalado en:\n%1\n\nReinicia Dolphin para activarlo.").arg(destFile));
    } else {
        QMessageBox::warning(this, tr("Error"),
            tr("No se pudo escribir:\n%1").arg(destFile));
    }
}
