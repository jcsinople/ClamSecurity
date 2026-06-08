#include "DetailsDialog.h"
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QFont>
#include <QSizePolicy>

static const QString ROW_OK   = "✓  %1";
static const QString ROW_FAIL = "✗  %1";
static const QString ROW_WARN = "⚠  %1";
static const QString ROW_INFO = "ℹ  %1";

static const QColor COLOR_OK   {0x2E, 0x7D, 0x32};
static const QColor COLOR_FAIL {0xC6, 0x28, 0x28};
static const QColor COLOR_WARN {0xE6, 0x5C, 0x00};

DetailsDialog::DetailsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("System Status"));
    setMinimumWidth(460);
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    auto *group  = new QGroupBox(tr("Security Components"), this);
    auto *gl     = new QVBoxLayout(group);
    gl->setSpacing(10);

    m_clamavRow      = makeRow(tr("ClamAV installed"));
    m_daemonRow      = makeRow(tr("ClamAV Daemon (clamav-daemon)"));
    m_realtimeRow    = makeRow(tr("Real-Time Protection (clamav-clamonacc)"));
    m_preventionRow  = makeRow(tr("OnAccessPrevention"));
    m_sigRow         = makeRow(tr("Signature database up to date"));
    m_firewallRow    = makeRow(tr("Firewall (UFW) active"));
    m_threatsRow     = makeRow(tr("No active threats"));
    m_quarantineRow  = makeRow(tr("Quarantine"));
    m_schedThreatsRow = makeRow(tr("Scheduled scan threats"));

    gl->addWidget(m_clamavRow);
    gl->addWidget(m_daemonRow);
    gl->addWidget(m_realtimeRow);
    gl->addWidget(m_preventionRow);
    gl->addWidget(m_sigRow);
    gl->addWidget(m_firewallRow);
    gl->addWidget(m_threatsRow);
    gl->addWidget(m_quarantineRow);
    gl->addWidget(m_schedThreatsRow);

    layout->addWidget(group);

    auto *bbox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(bbox);
}

void DetailsDialog::updateStatus(const SystemStatus &status)
{
    setRowStatus(m_clamavRow, status.clamavInstalled,
                 tr("ClamAV installed"),
                 tr("ClamAV NOT installed — run: sudo pacman -S clamav"));

    setRowStatus(m_daemonRow, status.daemonRunning,
                 tr("ClamAV Daemon active (on-demand scanning backend)"),
                 tr("clamav-daemon is not running"));

    // clamonacc: distinguish not-configured from stopped
    if (!status.realtimeAvailable) {
        setRowWarn(m_realtimeRow, tr("Real-Time Protection not configured "
                                     "(clamav-clamonacc service not found)"));
    } else {
        setRowStatus(m_realtimeRow, status.realtimeRunning,
                     tr("Real-Time Protection active (clamav-clamonacc)"),
                     tr("Real-Time Protection is disabled (clamav-clamonacc stopped)"));
    }

    // OnAccessPrevention: only meaningful when RT is running
    if (!status.realtimeAvailable) {
        setRowWarn(m_preventionRow, tr("OnAccessPrevention — not applicable "
                                       "(clamav-clamonacc not configured)"));
    } else if (!status.realtimeRunning) {
        setRowWarn(m_preventionRow, tr("OnAccessPrevention — not active "
                                       "(Real-Time Protection is stopped)"));
    } else {
        setRowStatus(m_preventionRow, status.onAccessPreventionEnabled,
                     tr("OnAccessPrevention enabled — threats are blocked on access"),
                     tr("OnAccessPrevention disabled — threats are detected but NOT blocked"));
    }

    setRowStatus(m_sigRow, status.signaturesRecent,
                 tr("Signatures up to date (< 7 days)"),
                 tr("Signatures outdated — update them now"));

    setRowStatus(m_firewallRow, status.firewallEnabled,
                 tr("UFW firewall active"),
                 tr("Firewall is disabled"));

    // Active threats
    if (!status.hasActiveThreats) {
        setRowStatus(m_threatsRow, true,
                     tr("No active threats detected"),
                     {});
    } else if (status.realtimeRunning && status.onAccessPreventionEnabled) {
        setRowWarn(m_threatsRow, tr("Active threats detected — "
                                    "blocked by OnAccessPrevention (review in Active Threats)"));
    } else {
        setRowStatus(m_threatsRow, false,
                     {},
                     tr("Active threats detected without OnAccessPrevention protection — "
                        "review in Active Threats"));
    }

    // Quarantine: informational only
    if (status.quarantineClean) {
        setRowInfo(m_quarantineRow, tr("No files in quarantine"));
    } else {
        setRowInfo(m_quarantineRow, tr("Files in quarantine — already neutralized, "
                                       "no action required"));
    }

    // Scheduled scan threats
    if (status.scheduledThreatsCount == 0) {
        setRowStatus(m_schedThreatsRow, true,
                     tr("No pending threats from scheduled scans"),
                     {});
    } else {
        setRowStatus(m_schedThreatsRow, false,
                     {},
                     tr("%n file(s) detected by scheduled scans — open Scheduled Scans to take action.",
                        nullptr, status.scheduledThreatsCount));
    }
}

QLabel *DetailsDialog::makeRow(const QString &text)
{
    auto *lbl = new QLabel(this);
    QFont f = lbl->font(); f.setPointSize(10); lbl->setFont(f);
    lbl->setWordWrap(true);
    lbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    lbl->setText(ROW_FAIL.arg(text));
    return lbl;
}

void DetailsDialog::setRowStatus(QLabel *label, bool ok,
                                  const QString &okText,
                                  const QString &failText)
{
    label->setText(ok ? ROW_OK.arg(okText) : ROW_FAIL.arg(failText));
    QPalette p = label->palette();
    p.setColor(QPalette::WindowText, ok ? COLOR_OK : COLOR_FAIL);
    label->setPalette(p);
}

void DetailsDialog::setRowWarn(QLabel *label, const QString &text)
{
    label->setText(ROW_WARN.arg(text));
    QPalette p = label->palette();
    p.setColor(QPalette::WindowText, COLOR_WARN);
    label->setPalette(p);
}

void DetailsDialog::setRowInfo(QLabel *label, const QString &text)
{
    label->setText(ROW_INFO.arg(text));
    QPalette p = label->palette();
    p.setColor(QPalette::WindowText, palette().color(QPalette::WindowText));
    label->setPalette(p);
}
