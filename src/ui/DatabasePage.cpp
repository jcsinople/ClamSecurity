#include "DatabasePage.h"
#include "ClamAvManager.h"
#include "FreshclamConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFont>
#include <QDateTime>
#include <QTimer>
#include <QScrollArea>

DatabasePage::DatabasePage(ClamAvManager *clam, QWidget *parent)
    : QWidget(parent), m_clam(clam),
      m_freshclamCfg(new FreshclamConfigManager(this))
{
    // Outer layout holds the scroll area + the fixed bottom button row
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 8);
    outerLayout->setSpacing(0);

    // Scrollable content widget
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget;
    auto *layout  = new QVBoxLayout(content);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *title = new QLabel(tr("Signature Update"), content);
    QFont f = title->font(); f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    // ── Database Status ───────────────────────────────────────────────────
    auto *group = new QGroupBox(tr("Database Status"), this);
    auto *gl    = new QVBoxLayout(group);

    m_dateLabel = new QLabel(this);
    QFont df = m_dateLabel->font(); df.setPointSize(11);
    m_dateLabel->setFont(df);
    gl->addWidget(m_dateLabel);

    gl->addWidget(new QLabel(ClamAvManager::version(), this));

    m_statusLabel = new QLabel(this);
    gl->addWidget(m_statusLabel);
    layout->addWidget(group);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);
    layout->addWidget(m_progress);

    auto *logGroup = new QGroupBox(tr("freshclam output"), this);
    auto *logLay   = new QVBoxLayout(logGroup);
    m_outputLog = new QPlainTextEdit(this);
    m_outputLog->setReadOnly(true);
    m_outputLog->setMaximumHeight(150);
    m_outputLog->setFont(QFont("Monospace", 8));
    logLay->addWidget(m_outputLog);
    layout->addWidget(logGroup);

    // ── Automatic Updates ─────────────────────────────────────────────────
    auto *autoGroup = new QGroupBox(tr("Automatic Updates (clamav-freshclam)"), this);
    auto *autoLay   = new QVBoxLayout(autoGroup);
    autoLay->setSpacing(8);

    auto *autoDesc = new QLabel(
        tr("When enabled, freshclam will automatically download virus signature "
           "updates in the background at the configured interval."), this);
    autoDesc->setWordWrap(true);
    autoLay->addWidget(autoDesc);

    m_chkAutoUpdate = new QCheckBox(tr("Enable automatic updates"), this);
    autoLay->addWidget(m_chkAutoUpdate);

    auto *freqRow = new QHBoxLayout;
    freqRow->addWidget(new QLabel(tr("Update every"), this));
    m_spinHours = new QSpinBox(this);
    m_spinHours->setRange(1, 24);
    m_spinHours->setSuffix(tr(" h"));
    m_spinHours->setFixedWidth(80);
    freqRow->addWidget(m_spinHours);
    freqRow->addWidget(new QLabel(tr("(1 – 24 hours)"), this));
    freqRow->addStretch();
    autoLay->addLayout(freqRow);

    m_autoStatusLabel = new QLabel(this);
    autoLay->addWidget(m_autoStatusLabel);

    m_btnSaveAuto = new QPushButton(
        QIcon::fromTheme("document-save"), tr("Save & Apply"), this);
    m_btnSaveAuto->setFixedWidth(140);
    autoLay->addWidget(m_btnSaveAuto, 0, Qt::AlignLeft);

    layout->addWidget(autoGroup);
    layout->addStretch();

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea, 1);

    // ── Navigation / manual update buttons (outside scroll) ───────────────
    auto *btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(12, 4, 12, 4);
    m_btnBack   = new QPushButton(QIcon::fromTheme("go-previous"), tr("Back"), this);
    m_btnUpdate = new QPushButton(QIcon::fromTheme("system-software-update"),
                                  tr("Update Now"), this);
    btnRow->addWidget(m_btnBack);
    btnRow->addStretch();
    btnRow->addWidget(m_btnUpdate);
    outerLayout->addLayout(btnRow);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_btnBack,   &QPushButton::clicked, this, &DatabasePage::backRequested);
    connect(m_btnUpdate, &QPushButton::clicked, this, &DatabasePage::onUpdateClicked);
    connect(m_clam, &ClamAvManager::updateOutput,
            this, &DatabasePage::onUpdateOutput);
    connect(m_clam, &ClamAvManager::updateFinished,
            this, &DatabasePage::onUpdateFinished);
    connect(m_btnSaveAuto, &QPushButton::clicked,
            this, &DatabasePage::onSaveAutoUpdate);
    connect(m_freshclamCfg, &FreshclamConfigManager::configSaved,
            this, &DatabasePage::onConfigSaved);

    connect(m_chkAutoUpdate, &QCheckBox::toggled,
            m_spinHours, &QSpinBox::setEnabled);

    refresh();
}

void DatabasePage::refresh()
{
    m_outputLog->clear();

    // ── Signature date ────────────────────────────────────────────────────
    QDateTime sigDate = m_clam->signatureDate();
    if (sigDate.isValid()) {
        int days = sigDate.daysTo(QDateTime::currentDateTime());
        m_dateLabel->setText(
            tr("Last update: %1  (%2 days ago)")
            .arg(sigDate.toString("yyyy-MM-dd HH:mm")).arg(days));
        QPalette p = m_statusLabel->palette();
        if (days > 7) {
            m_statusLabel->setText(tr("⚠  Signatures are out of date."));
            p.setColor(QPalette::WindowText, QColor(0xC6, 0x28, 0x28));
        } else {
            m_statusLabel->setText(tr("✓  Signatures are up to date."));
            p.setColor(QPalette::WindowText, QColor(0x2E, 0x7D, 0x32));
        }
        m_statusLabel->setPalette(p);
    } else {
        m_dateLabel->setText(tr("Could not read signature date."));
    }

    // ── Auto-update state ─────────────────────────────────────────────────
    bool enabled = m_clam->isFreshclamEnabled();
    m_chkAutoUpdate->blockSignals(true);
    m_chkAutoUpdate->setChecked(enabled);
    m_chkAutoUpdate->blockSignals(false);

    int checks = m_freshclamCfg->readChecks();
    int hours  = qBound(1, qRound(24.0 / checks), 24);
    m_spinHours->setValue(hours);
    m_spinHours->setEnabled(enabled);

    m_autoStatusLabel->clear();
    m_btnSaveAuto->setEnabled(true);
}

// ── Manual update ─────────────────────────────────────────────────────────────

void DatabasePage::onUpdateClicked()
{
    m_outputLog->clear();
    m_progress->setVisible(true);
    m_btnUpdate->setEnabled(false);
    m_statusLabel->setText(tr("Updating signatures…"));
    m_clam->forceUpdate();
}

void DatabasePage::onUpdateOutput(const QString &line)
{
    m_outputLog->appendPlainText(line);
}

void DatabasePage::onUpdateFinished(bool success, const QString &message)
{
    m_progress->setVisible(false);
    m_btnUpdate->setEnabled(true);
    m_statusLabel->setText(message);
    if (success) refresh();
}

// ── Auto-update ───────────────────────────────────────────────────────────────

void DatabasePage::onSaveAutoUpdate()
{
    m_btnSaveAuto->setEnabled(false);
    m_autoStatusLabel->setText(tr("Saving…"));

    bool enable = m_chkAutoUpdate->isChecked();
    int  hours  = m_spinHours->value();
    int  checks = qMax(1, qRound(24.0 / hours));

    if (enable) {
        // Save frequency to freshclam.conf first; on success, enable service
        m_freshclamCfg->saveChecks(checks);
    } else {
        // Disable the service right away (no conf change needed)
        m_clam->setFreshclamEnabled(false);
        // Simulate async feedback after service has time to stop
        QTimer::singleShot(3000, this, [this]() {
            bool running = m_clam->isFreshclamEnabled();
            onConfigSaved(!running, running
                ? tr("Could not disable the service — check system logs.")
                : tr("Automatic updates disabled."));
        });
    }
}

void DatabasePage::onConfigSaved(bool success, const QString &message)
{
    if (success && m_chkAutoUpdate->isChecked()) {
        // Conf saved OK → now enable the service
        m_clam->setFreshclamEnabled(true);
    }

    m_btnSaveAuto->setEnabled(true);

    QPalette p = m_autoStatusLabel->palette();
    p.setColor(QPalette::WindowText,
               success ? QColor(0x2E, 0x7D, 0x32) : QColor(0xC6, 0x28, 0x28));
    m_autoStatusLabel->setPalette(p);
    m_autoStatusLabel->setText(success ? "✓  " + message : "✗  " + message);
}
