#include "DetailsDialog.h"
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QFont>

static const QString ROW_OK   = "✓  %1";
static const QString ROW_FAIL = "✗  %1";

DetailsDialog::DetailsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Estado del Sistema"));
    setMinimumWidth(400);
    setModal(true);

    auto *layout = new QVBoxLayout(this);

    auto *group = new QGroupBox(tr("Componentes de Seguridad"), this);
    auto *gl    = new QVBoxLayout(group);
    gl->setSpacing(10);

    m_clamavRow    = makeRow(tr("ClamAV instalado"));
    m_daemonRow    = makeRow(tr("Protección en Tiempo Real (daemon)"));
    m_sigRow       = makeRow(tr("Base de datos de firmas reciente"));
    m_firewallRow  = makeRow(tr("Firewall (UFW) activo"));
    m_quarantineRow = makeRow(tr("Sin archivos en cuarentena"));

    gl->addWidget(m_clamavRow);
    gl->addWidget(m_daemonRow);
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
                 tr("ClamAV instalado"),
                 tr("ClamAV NO instalado — instala con: sudo pacman -S clamav"));

    setRowStatus(m_daemonRow, status.daemonRunning,
                 tr("Protección en Tiempo Real activa"),
                 tr("Daemon clamav-daemon inactivo"));

    setRowStatus(m_sigRow, status.signaturesRecent,
                 tr("Firmas actualizadas (< 7 días)"),
                 tr("Firmas desactualizadas — actualiza las firmas"));

    setRowStatus(m_firewallRow, status.firewallEnabled,
                 tr("Firewall UFW activo"),
                 tr("Firewall desactivado"));

    setRowStatus(m_quarantineRow, status.quarantineClean,
                 tr("Sin archivos en cuarentena"),
                 tr("Hay archivos en cuarentena pendientes de revisión"));
}

QLabel *DetailsDialog::makeRow(const QString &text)
{
    auto *lbl = new QLabel(this);
    QFont f = lbl->font();
    f.setPointSize(10);
    lbl->setFont(f);
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
