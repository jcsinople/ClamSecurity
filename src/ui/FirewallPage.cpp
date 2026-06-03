#include "FirewallPage.h"
#include "UFWManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFont>
#include <QTimer>

FirewallPage::FirewallPage(UFWManager *ufw, QWidget *parent)
    : QWidget(parent), m_ufw(ufw)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *title = new QLabel(tr("Firewall (UFW)"), this);
    QFont f = title->font(); f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    auto *ctrlGroup = new QGroupBox(tr("Control del Firewall"), this);
    auto *ctrlLay   = new QHBoxLayout(ctrlGroup);

    m_toggle = new QCheckBox(tr("Activar firewall"), this);
    m_statusLabel = new QLabel(this);
    QFont sf = m_statusLabel->font(); sf.setPointSize(10);
    m_statusLabel->setFont(sf);

    ctrlLay->addWidget(m_toggle);
    ctrlLay->addSpacing(16);
    ctrlLay->addWidget(m_statusLabel);
    ctrlLay->addStretch();
    layout->addWidget(ctrlGroup);

    auto *rulesGroup = new QGroupBox(tr("Estado y reglas activas"), this);
    auto *rulesLay   = new QVBoxLayout(rulesGroup);
    m_rulesView = new QPlainTextEdit(this);
    m_rulesView->setReadOnly(true);
    QFont mono("Monospace", 9);
    m_rulesView->setFont(mono);
    m_rulesView->setMinimumHeight(180);
    rulesLay->addWidget(m_rulesView);

    m_btnRefresh = new QPushButton(QIcon::fromTheme("view-refresh"),
                                   tr("Refrescar Reglas"), this);
    rulesLay->addWidget(m_btnRefresh);
    layout->addWidget(rulesGroup);
    layout->addStretch();

    auto *navRow = new QHBoxLayout;
    m_btnBack = new QPushButton(QIcon::fromTheme("go-previous"), tr("Volver"), this);
    navRow->addWidget(m_btnBack);
    navRow->addStretch();
    layout->addLayout(navRow);

    connect(m_toggle,     &QCheckBox::toggled,   this, &FirewallPage::onToggleFirewall);
    connect(m_btnRefresh, &QPushButton::clicked,  this, &FirewallPage::onRefreshRules);
    connect(m_btnBack,    &QPushButton::clicked,  this, &FirewallPage::backRequested);
    connect(m_ufw, &UFWManager::statusChanged, this, [this](bool enabled) {
        m_toggle->blockSignals(true);
        m_toggle->setChecked(enabled);
        m_toggle->blockSignals(false);
        m_toggle->setEnabled(true);
        updateStatusLabel(enabled);
        onRefreshRules();
    });

    if (!UFWManager::isInstalled()) {
        m_toggle->setEnabled(false);
        m_btnRefresh->setEnabled(false);
        m_statusLabel->setText(tr("UFW no instalado — sudo pacman -S ufw"));
    } else {
        refresh();
    }
}

void FirewallPage::refresh()
{
    bool enabled = m_ufw->isEnabled();
    m_toggle->blockSignals(true);
    m_toggle->setChecked(enabled);
    m_toggle->blockSignals(false);
    updateStatusLabel(enabled);
    m_rulesView->setPlainText(m_ufw->rulesOutput());
}

void FirewallPage::onToggleFirewall(bool checked)
{
    m_toggle->setEnabled(false);
    m_ufw->setEnabled(checked);
    // UFWManager::statusChanged re-enables the toggle on completion
    QTimer::singleShot(8000, this, [this]() {
        // Fallback: re-enable if signal never arrived
        if (!m_toggle->isEnabled()) {
            m_toggle->setEnabled(true);
            refresh();
        }
    });
}

void FirewallPage::onRefreshRules()
{
    m_rulesView->setPlainText(m_ufw->rulesOutput());
}

void FirewallPage::updateStatusLabel(bool enabled)
{
    QPalette p = m_statusLabel->palette();
    if (enabled) {
        m_statusLabel->setText(tr("✓  Firewall ACTIVO"));
        p.setColor(QPalette::WindowText, QColor(0x2E, 0x7D, 0x32));
    } else {
        m_statusLabel->setText(tr("✗  Firewall INACTIVO"));
        p.setColor(QPalette::WindowText, QColor(0xC6, 0x28, 0x28));
    }
    m_statusLabel->setPalette(p);
}
