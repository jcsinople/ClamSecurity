#include "FirewallPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFont>
#include <QTimer>
#include <QHeaderView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QMessageBox>

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

    // ── Control ──────────────────────────────────────────────────────────
    auto *ctrlGroup = new QGroupBox(tr("Firewall Control"), this);
    auto *ctrlLay   = new QHBoxLayout(ctrlGroup);
    m_toggle      = new QCheckBox(tr("Enable firewall"), this);
    m_statusLabel = new QLabel(this);
    QFont sf = m_statusLabel->font(); sf.setPointSize(10);
    m_statusLabel->setFont(sf);
    ctrlLay->addWidget(m_toggle);
    ctrlLay->addSpacing(16);
    ctrlLay->addWidget(m_statusLabel);
    ctrlLay->addStretch();
    layout->addWidget(ctrlGroup);

    // ── Rules table ───────────────────────────────────────────────────────
    auto *rulesGroup = new QGroupBox(tr("Firewall Rules"), this);
    auto *rulesLay   = new QVBoxLayout(rulesGroup);

    m_rulesTable = new QTableWidget(this);
    m_rulesTable->setColumnCount(5);
    m_rulesTable->setHorizontalHeaderLabels({
        tr("#"), tr("Action"), tr("Direction"), tr("Port / Address"), tr("From")
    });
    m_rulesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_rulesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_rulesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_rulesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_rulesTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_rulesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rulesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rulesTable->setAlternatingRowColors(true);
    m_rulesTable->setMinimumHeight(180);
    rulesLay->addWidget(m_rulesTable);

    auto *rulesBtnRow = new QHBoxLayout;
    m_btnRefresh = new QPushButton(QIcon::fromTheme("view-refresh"),
                                   tr("Refresh Rules"), this);
    m_btnAdd     = new QPushButton(QIcon::fromTheme("list-add"),
                                   tr("Add Rule"), this);
    m_btnDelete  = new QPushButton(QIcon::fromTheme("list-remove"),
                                   tr("Delete Selected"), this);
    m_btnDelete->setEnabled(false);
    rulesBtnRow->addWidget(m_btnRefresh);
    rulesBtnRow->addWidget(m_btnAdd);
    rulesBtnRow->addStretch();
    rulesBtnRow->addWidget(m_btnDelete);
    rulesLay->addLayout(rulesBtnRow);
    layout->addWidget(rulesGroup);

    // ── Navigation ────────────────────────────────────────────────────────
    auto *navRow = new QHBoxLayout;
    m_btnBack = new QPushButton(QIcon::fromTheme("go-previous"), tr("Back"), this);
    navRow->addWidget(m_btnBack);
    navRow->addStretch();
    layout->addLayout(navRow);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_toggle,     &QCheckBox::toggled,  this, &FirewallPage::onToggleFirewall);
    connect(m_btnRefresh, &QPushButton::clicked, this, &FirewallPage::onRefreshRules);
    connect(m_btnAdd,     &QPushButton::clicked, this, &FirewallPage::onAddRule);
    connect(m_btnDelete,  &QPushButton::clicked, this, &FirewallPage::onDeleteRule);
    connect(m_btnBack,    &QPushButton::clicked, this, &FirewallPage::backRequested);
    connect(m_rulesTable, &QTableWidget::itemSelectionChanged,
            this, &FirewallPage::onSelectionChanged);

    // UFWManager signals
    connect(m_ufw, &UFWManager::statusChanged,
            this, &FirewallPage::onUfwStatusChanged);
    connect(m_ufw, &UFWManager::rulesRefreshed,
            this, &FirewallPage::onRulesRefreshed);
    connect(m_ufw, &UFWManager::error, this, [this](const QString &msg) {
        QMessageBox::warning(this, tr("Firewall Error"), msg);
        m_toggle->setEnabled(true);
    });

    if (!UFWManager::isInstalled()) {
        m_toggle->setEnabled(false);
        m_btnRefresh->setEnabled(false);
        m_btnAdd->setEnabled(false);
        m_statusLabel->setText(tr("UFW not installed — sudo pacman -S ufw"));
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
    // Don't auto-load rules here to avoid unnecessary pkexec prompt on page open
}

void FirewallPage::onToggleFirewall(bool checked)
{
    m_toggle->setEnabled(false);
    m_ufw->setEnabled(checked);
    // Re-enabled via onUfwStatusChanged or error handler (with 8s safety fallback)
    QTimer::singleShot(10000, this, [this]() {
        if (!m_toggle->isEnabled()) {
            m_toggle->setEnabled(true);
            refresh();
        }
    });
}

void FirewallPage::onUfwStatusChanged(bool enabled)
{
    m_toggle->blockSignals(true);
    m_toggle->setChecked(enabled);
    m_toggle->blockSignals(false);
    m_toggle->setEnabled(true);
    updateStatusLabel(enabled);
    // No automatic rules refresh — avoids second pkexec password prompt
}

void FirewallPage::onRefreshRules()
{
    m_btnRefresh->setEnabled(false);
    m_rulesTable->setRowCount(0);
    m_ufw->refreshRules();
    QTimer::singleShot(15000, m_btnRefresh,
        [this]() { m_btnRefresh->setEnabled(true); });
}

void FirewallPage::onRulesRefreshed(const QList<UFWRule> &rules, const QString &)
{
    m_btnRefresh->setEnabled(true);
    populateTable(rules);
}

void FirewallPage::populateTable(const QList<UFWRule> &rules)
{
    m_rules = rules;
    m_rulesTable->setRowCount(0);
    for (const UFWRule &r : rules) {
        int row = m_rulesTable->rowCount();
        m_rulesTable->insertRow(row);
        m_rulesTable->setItem(row, 0, new QTableWidgetItem(QString::number(r.number)));
        auto *actItem = new QTableWidgetItem(r.action);
        actItem->setForeground(
            r.action == "ALLOW"
            ? QColor(0x2E, 0x7D, 0x32)   // green
            : QColor(0xC6, 0x28, 0x28));  // red
        m_rulesTable->setItem(row, 1, actItem);
        m_rulesTable->setItem(row, 2, new QTableWidgetItem(r.direction));
        m_rulesTable->setItem(row, 3, new QTableWidgetItem(r.to));
        m_rulesTable->setItem(row, 4, new QTableWidgetItem(r.from));
    }
    onSelectionChanged();
}

void FirewallPage::onAddRule()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Add Firewall Rule"));
    dlg.setMinimumWidth(360);

    auto *form   = new QFormLayout(&dlg);
    auto *cmbAction = new QComboBox(&dlg);
    cmbAction->addItems({"allow", "deny", "limit", "reject"});
    auto *cmbDir = new QComboBox(&dlg);
    cmbDir->addItems({"in", "out"});
    auto *edtPort = new QLineEdit(&dlg);
    edtPort->setPlaceholderText("22/tcp   or   80   or   8080:8090/tcp");
    auto *edtFrom = new QLineEdit("Anywhere", &dlg);
    edtFrom->setPlaceholderText("Anywhere  or  192.168.1.0/24");

    form->addRow(tr("Action:"),    cmbAction);
    form->addRow(tr("Direction:"), cmbDir);
    form->addRow(tr("Port / Address:"), edtPort);
    form->addRow(tr("From (source):"),  edtFrom);

    auto *note = new QLabel(
        tr("<small>Examples: <b>22/tcp</b>, <b>80</b>, <b>443/tcp</b>, "
           "<b>8080:8090/tcp</b></small>"), &dlg);
    note->setTextFormat(Qt::RichText);
    form->addRow(note);

    auto *bbox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(bbox);
    connect(bbox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;
    if (edtPort->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Validation"),
                             tr("Port / Address cannot be empty."));
        return;
    }
    m_ufw->addRule(cmbDir->currentText(),
                   cmbAction->currentText(),
                   edtPort->text().trimmed(),
                   edtFrom->text().trimmed());
}

void FirewallPage::onDeleteRule()
{
    int row = m_rulesTable->currentRow();
    if (row < 0 || row >= m_rules.size()) return;

    const UFWRule &r = m_rules[row];
    auto ret = QMessageBox::question(this, tr("Confirm deletion"),
        tr("Delete rule #%1: %2 %3 %4 from %5?")
            .arg(r.number).arg(r.action).arg(r.direction).arg(r.to).arg(r.from));
    if (ret != QMessageBox::Yes) return;

    m_ufw->deleteRule(r.number);
}

void FirewallPage::onSelectionChanged()
{
    m_btnDelete->setEnabled(m_rulesTable->currentRow() >= 0);
}

void FirewallPage::updateStatusLabel(bool enabled)
{
    QPalette p = m_statusLabel->palette();
    if (enabled) {
        m_statusLabel->setText(tr("✓  Firewall is ACTIVE"));
        p.setColor(QPalette::WindowText, QColor(0x2E, 0x7D, 0x32));
    } else {
        m_statusLabel->setText(tr("✗  Firewall is INACTIVE"));
        p.setColor(QPalette::WindowText, QColor(0xC6, 0x28, 0x28));
    }
    m_statusLabel->setPalette(p);
}
