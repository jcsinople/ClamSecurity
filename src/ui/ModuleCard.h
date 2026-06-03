#pragma once
#include <QFrame>
#include <QLabel>
#include <QString>
#include <QIcon>

class ModuleCard : public QFrame
{
    Q_OBJECT
public:
    explicit ModuleCard(const QString &iconName,
                        const QString &title,
                        const QString &subtitle,
                        QWidget *parent = nullptr);

    void setSubtitle(const QString &text);
    void setStatusColor(const QColor &color);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QLabel *m_iconLabel;
    QLabel *m_titleLabel;
    QLabel *m_subtitleLabel;
    bool    m_hovered = false;
};
