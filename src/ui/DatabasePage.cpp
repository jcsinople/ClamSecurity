#include "DatabasePage.h"
#include "ClamAvManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFont>
#include <QDateTime>

DatabasePage::DatabasePage(ClamAvManager *clam, QWidget *parent)
    : QWidget(parent), m_clam(clam)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *title = new QLabel(tr("Signature Update"), this);
    QFont f = title->font(); f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

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

    layout->addStretch();

    auto *btnRow  = new QHBoxLayout;
    m_btnBack     = new QPushButton(QIcon::fromTheme("go-previous"), tr("Back"), this);
    m_btnUpdate   = new QPushButton(QIcon::fromTheme("system-software-update"),
                                    tr("Update Now"), this);
    btnRow->addWidget(m_btnBack);
    btnRow->addStretch();
    btnRow->addWidget(m_btnUpdate);
    layout->addLayout(btnRow);

    connect(m_btnBack,   &QPushButton::clicked, this, &DatabasePage::backRequested);
    connect(m_btnUpdate, &QPushButton::clicked, this, &DatabasePage::onUpdateClicked);
    connect(m_clam, &ClamAvManager::updateOutput,
            this, &DatabasePage::onUpdateOutput);
    connect(m_clam, &ClamAvManager::updateFinished,
            this, &DatabasePage::onUpdateFinished);

    refresh();
}

void DatabasePage::refresh()
{
    m_outputLog->clear();
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
}

void DatabasePage::onUpdateClicked()
{
    m_outputLog->clear();
    m_progress->setVisible(true);
    m_btnUpdate->setEnabled(false);
    m_statusLabel->setText(tr("Updating signatures…"));
    m_clam->forceUpdate();
}

void DatabasePage::onUpdateOutput(const QString &line)   { m_outputLog->appendPlainText(line); }

void DatabasePage::onUpdateFinished(bool success, const QString &message)
{
    m_progress->setVisible(false);
    m_btnUpdate->setEnabled(true);
    m_statusLabel->setText(message);
    if (success) refresh();
}
