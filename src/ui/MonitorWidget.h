#pragma once
#include <QWidget>

class MonitorWidget : public QWidget
{
    Q_OBJECT
public:
    enum class Status { Protected, Alert };

    explicit MonitorWidget(QWidget *parent = nullptr);
    void setStatus(Status s);
    Status status() const { return m_status; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Status m_status = Status::Protected;
};
