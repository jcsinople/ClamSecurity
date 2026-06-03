#include "DetailsDialog.h"
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QFont>

static const QString ROW_OK   = "✓  %1";
static const QString ROW_FAIL = "✗  %1";
static const QString ROW_WARN = "⚠  %1";

DetailsDialog::DetailsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("System Status"));
    setMinimumWidth(440);
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    auto *group  = new QGroupBox(tr("Security Components"), this);
    auto *gl     = new QVBoxLayout(group);
    gl->setSpacing(10);

    m_clamavRow    = makeRow(tr("ClamAV installed"));
    m_daemonRow    = makeRow(tr("ClamAV Daemon (clamav-daemon)"));
    m_realtimeRow  = makeRow(tr("Real-Time Protection (clamav-clamonacc)"));
    m_sigRow       = makeRow(tr("Signature database up to date"));
    m_firewallRow  = makeRow(tr("Firewall (UFW) active"));
    m_quarantineRow = makeRow(tr("No files in quarantine"));

    gl->addWidget(m_clamavRow);
    gl->addWidget(m_daemonRow);
    gl->addWidget(m_realtimeRow);
    gl->addWidget(m_sigRow);
    gl->addWidget(m_firewallRow);
    gl->addWidget(m_quarantineRow);

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

    // clamonacc: show "not configured" differently from "stopped"
    if (!status.realtimeAvailable) {
        m_realtimeRow->setText(ROW_WARN.arg(
            tr("Real-Time Protection not configured "
               "(clamav-clamonacc service not found)")));
        QPalette p = m_realtimeRow->palette();
        p.setColor(QPalette::WindowText, QColor(0xF5, 0x7F, 0x17));
        m_realtimeRow->setPalette(p);
    } else {
        setRowStatus(m_realtimeRow, status.realtimeRunning,
                     tr("Real-Time Protection active (clamav-clamonacc)"),
                     tr("clamav-clamonacc is not running"));
    }

    setRowStatus(m_sigRow, status.signaturesRecent,
                 tr("Signatures up to date (< 7 days)"),
                 tr("Signatures outdated — update them now"));

    setRowStatus(m_firewallRow, status.firewallEnabled,
                 tr("UFW firewall active"),
                 tr("Firewall is disabled"));

    setRowStatus(m_quarantineRow, status.quarantineClean,
                 tr("No files in quarantine"),
                 tr("Files in quarantine need review"));
}

QLabel *DetailsDialog::makeRow(const QString &text)
{
    auto *lbl = new QLabel(this);
    QFont f = lbl->font(); f.setPointSize(10); lbl->setFont(f);
    lbl->setText(ROW_FAIL.arg(text));
    return lbl;
}

void DetailsDialog::setRowStatus(QLabel *label, bool ok,
                                  const QString &okText,
                                  const QString &failText)
{
    label->setText(ok ? ROW_OK.arg(okText) : ROW_FAIL.arg(failText));
    QPalette p = label->palette();
    p.setColor(QPalette::WindowText,
               ok ? QColor(0x2E, 0x7D, 0x32) : QColor(0xC6, 0x28, 0x28));
    label->setPalette(p);
}
