#include "MonitorWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QIcon>

MonitorWidget::MonitorWidget(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setMinimumSize(180, 160);
}

void MonitorWidget::setStatus(Status s)
{
    if (m_status != s) {
        m_status = s;
        update();
    }
}

QSize MonitorWidget::sizeHint() const        { return {200, 170}; }
QSize MonitorWidget::minimumSizeHint() const { return {150, 130}; }

void MonitorWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int W = width();
    const int H = height();

    // Color palette
    QColor frameColor   = palette().color(QPalette::Mid);
    QColor frameDark    = frameColor.darker(140);
    QColor screenColor  = (m_status == Status::Protected) ? QColor(0x4C, 0xAF, 0x50)  // green
                        : (m_status == Status::Warning)  ? QColor(0xFF, 0xB3, 0x00)  // amber
                                                         : QColor(0xF4, 0x43, 0x36); // red
    QColor screenGlow   = screenColor.lighter(160);

    // ---- Monitor body (outer frame) ----
    const int bodyW = int(W * 0.88);
    const int bodyH = int(H * 0.68);
    const int bodyX = (W - bodyW) / 2;
    const int bodyY = int(H * 0.02);

    // Shadow
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 40));
    p.drawRoundedRect(bodyX + 4, bodyY + 4, bodyW, bodyH, 10, 10);

    // Frame
    QLinearGradient frameGrad(bodyX, bodyY, bodyX, bodyY + bodyH);
    frameGrad.setColorAt(0.0, frameColor.lighter(110));
    frameGrad.setColorAt(1.0, frameDark);
    p.setBrush(frameGrad);
    p.setPen(QPen(frameDark, 1));
    p.drawRoundedRect(bodyX, bodyY, bodyW, bodyH, 10, 10);

    // ---- Screen ----
    const int pad    = int(bodyW * 0.06);
    const int scrX   = bodyX + pad;
    const int scrY   = bodyY + pad;
    const int scrW   = bodyW - 2 * pad;
    const int scrH   = bodyH - 2 * pad - 4;

    // Screen glow
    QRadialGradient screenGrad(scrX + scrW / 2, scrY + scrH / 2,
                               qMax(scrW, scrH));
    screenGrad.setColorAt(0.0, screenGlow);
    screenGrad.setColorAt(1.0, screenColor);
    p.setBrush(screenGrad);
    p.setPen(QPen(screenColor.darker(120), 1));
    p.drawRoundedRect(scrX, scrY, scrW, scrH, 5, 5);

    // Icon on screen
    QString iconName = (m_status == Status::Protected) ? "security-high"
                     : (m_status == Status::Warning)  ? "security-medium"
                                                      : "security-low";
    QIcon icon = QIcon::fromTheme(iconName, QIcon::fromTheme("dialog-warning"));
    int iconSz = int(qMin(scrW, scrH) * 0.55);
    QRect iconRect((W - iconSz) / 2,
                   scrY + (scrH - iconSz) / 2,
                   iconSz, iconSz);
    icon.paint(&p, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::On);

    // ---- Stand neck ----
    const int neckW = int(bodyW * 0.10);
    const int neckH = int(H * 0.12);
    const int neckX = (W - neckW) / 2;
    const int neckY = bodyY + bodyH;

    p.setBrush(frameDark);
    p.setPen(Qt::NoPen);
    p.drawRect(neckX, neckY, neckW, neckH);

    // ---- Base ----
    const int baseW = int(bodyW * 0.45);
    const int baseH = int(H * 0.07);
    const int baseX = (W - baseW) / 2;
    const int baseY = neckY + neckH;

    QLinearGradient baseGrad(baseX, baseY, baseX, baseY + baseH);
    baseGrad.setColorAt(0.0, frameColor);
    baseGrad.setColorAt(1.0, frameDark.darker(110));
    p.setBrush(baseGrad);
    p.drawRoundedRect(baseX, baseY, baseW, baseH, 4, 4);
}
