#include "ModuleCard.h"
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QEnterEvent>

ModuleCard::ModuleCard(const QString &iconName,
                       const QString &title,
                       const QString &subtitle,
                       QWidget *parent)
    : QFrame(parent)
{
    setFixedSize(150, 130);
    setCursor(Qt::PointingHandCursor);
    setFrameShape(QFrame::NoFrame);

    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(6);
    layout->setContentsMargins(12, 16, 12, 12);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    QIcon icon = QIcon::fromTheme(iconName);
    m_iconLabel->setPixmap(icon.pixmap(36, 36));

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(10);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setWordWrap(true);

    m_subtitleLabel = new QLabel(subtitle, this);
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    QFont subFont = m_subtitleLabel->font();
    subFont.setPointSize(8);
    m_subtitleLabel->setFont(subFont);
    m_subtitleLabel->setWordWrap(true);

    layout->addWidget(m_iconLabel);
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_subtitleLabel);
}

void ModuleCard::setSubtitle(const QString &text)
{
    m_subtitleLabel->setText(text);
}

void ModuleCard::setStatusColor(const QColor &color)
{
    QPalette p = m_subtitleLabel->palette();
    p.setColor(QPalette::WindowText, color);
    m_subtitleLabel->setPalette(p);
}

void ModuleCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked();
    QFrame::mousePressEvent(event);
}

void ModuleCard::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QFrame::enterEvent(event);
}

void ModuleCard::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QFrame::leaveEvent(event);
}

void ModuleCard::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QColor bg = palette().color(QPalette::Base);
    if (m_hovered)
        bg = bg.lighter(115);

    p.setPen(QPen(palette().color(QPalette::Mid), 1));
    p.setBrush(bg);
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 10, 10);

    QFrame::paintEvent(event);
}
