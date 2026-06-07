#include "ActiveThreatsPage.h"
#include "QuarantineManager.h"
#include "ClamAvManager.h"
#include "ClamdConfigManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QFont>
#include <QMessageBox>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QUuid>

// ── Static helpers ─────────────────────────────────────────────────────────────

QString ActiveThreatsPage::dataFilePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/active_threats.json";
}

QList<ActiveThreat> ActiveThreatsPage::loadThreats()
{
    QList<ActiveThreat> result;
    QFile f(dataFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return result;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) return result;

    for (const QJsonValue &v : doc.array()) {
        QJsonObject o = v.toObject();
        ActiveThreat t;
        t.id                  = o["id"].toString();
        t.timestamp           = o["timestamp"].toString();
        t.threatName          = o["threatName"].toString();
        t.preventionWasActive = o["preventionWasActive"].toBool();

        // Sanitize legacy entries where the full journal line was stored
        QString fp = o["filePath"].toString();
        int arrow  = fp.lastIndexOf(" -> ");
        t.filePath = (arrow >= 0) ? fp.mid(arrow + 4).trimmed() : fp;
        result << t;
    }
    return result;
}

void ActiveThreatsPage::saveThreats(const QList<ActiveThreat> &threats)
{
    QJsonArray arr;
    for (const ActiveThreat &t : threats) {
        QJsonObject o;
        o["id"]                  = t.id;
        o["timestamp"]           = t.timestamp;
        o["filePath"]            = t.filePath;
        o["threatName"]          = t.threatName;
        o["preventionWasActive"] = t.preventionWasActive;
        arr.append(o);
    }
    QFile f(dataFilePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(arr).toJson());
}

void ActiveThreatsPage::clearHistory()
{
    saveThreats({});
}

int ActiveThreatsPage::pendingCount()
{
    return loadThreats().size();
}

void ActiveThreatsPage::addThreat(const QString &filePath,
                                   const QString &threatName,
                                   bool preventionActive)
{
    auto threats = loadThreats();

    // Deduplicate: if this file+threat is already tracked, skip
    for (const ActiveThreat &existing : threats) {
        if (existing.filePath == filePath && existing.threatName == threatName)
            return;
    }

    ActiveThreat t;
    t.id                  = QUuid::createUuid().toString(QUuid::WithoutBraces);
    t.timestamp           = QDateTime::currentDateTime().toString(Qt::ISODate);
    t.filePath            = filePath;
    t.threatName          = threatName;
    t.preventionWasActive = preventionActive;

    threats << t;
    saveThreats(threats);
}

// ── Constructor ────────────────────────────────────────────────────────────────

ActiveThreatsPage::ActiveThreatsPage(QuarantineManager  *quar,
                                     ClamAvManager      *clam,
                                     ClamdConfigManager *cfgMgr,
                                     QWidget *parent)
    : QWidget(parent), m_quar(quar), m_clam(clam), m_cfgMgr(cfgMgr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *title = new QLabel(tr("Active Threats (Real-Time)"), this);
    QFont f = title->font(); f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    m_infoLabel = new QLabel(tr("No threats detected."), this);
    layout->addWidget(m_infoLabel);

    // Table — 3 columns: Time, File, Threat (no Status column)
    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({tr("Time"), tr("File"), tr("Threat")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    layout->addWidget(m_table);

    // Action buttons
    auto *actRow    = new QHBoxLayout;
    m_btnQuarantine = new QPushButton(QIcon::fromTheme("security-low"),
                                     tr("Quarantine Selected"), this);
    m_btnDelete     = new QPushButton(QIcon::fromTheme("edit-delete"),
                                     tr("Delete File"), this);
    m_btnExclude    = new QPushButton(QIcon::fromTheme("folder-open"),
                                     tr("Add to Exclusions"), this);
    m_btnClearAll   = new QPushButton(QIcon::fromTheme("edit-clear"),
                                     tr("Clear All"), this);

    actRow->addWidget(m_btnQuarantine);
    actRow->addWidget(m_btnDelete);
    actRow->addWidget(m_btnExclude);
    actRow->addStretch();
    actRow->addWidget(m_btnClearAll);
    layout->addLayout(actRow);

    auto *navRow = new QHBoxLayout;
    m_btnBack = new QPushButton(QIcon::fromTheme("go-previous"), tr("Back"), this);
    navRow->addWidget(m_btnBack);
    navRow->addStretch();
    layout->addLayout(navRow);

    connect(m_btnQuarantine, &QPushButton::clicked, this, &ActiveThreatsPage::onQuarantine);
    connect(m_btnDelete,     &QPushButton::clicked, this, &ActiveThreatsPage::onDeleteFile);
    connect(m_btnExclude,    &QPushButton::clicked, this, &ActiveThreatsPage::onAddExclusion);
    connect(m_btnClearAll,   &QPushButton::clicked, this, &ActiveThreatsPage::onClearAll);
    connect(m_btnBack,       &QPushButton::clicked, this, &ActiveThreatsPage::backRequested);
    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &ActiveThreatsPage::updateActions);

    updateActions();
    refresh();
}

// ── Refresh ────────────────────────────────────────────────────────────────────

void ActiveThreatsPage::refresh()
{
    m_table->blockSignals(true);
    m_table->setRowCount(0);

    auto threats = loadThreats();

    for (const ActiveThreat &t : threats) {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        QString ts = QDateTime::fromString(t.timestamp, Qt::ISODate)
                     .toString("yyyy-MM-dd HH:mm:ss");
        m_table->setItem(row, 0, new QTableWidgetItem(ts));
        m_table->setItem(row, 1, new QTableWidgetItem(t.filePath));
        m_table->setItem(row, 2, new QTableWidgetItem(t.threatName));

        m_table->item(row, 0)->setData(Qt::UserRole, t.id);
    }

    m_table->blockSignals(false);

    int count = threats.size();
    if (count == 0)
        m_infoLabel->setText(tr("No threats detected."));
    else
        m_infoLabel->setText(tr("%n threat(s) detected by real-time protection.", nullptr, count));

    updateActions();
}

// ── Actions ────────────────────────────────────────────────────────────────────

void ActiveThreatsPage::updateActions()
{
    bool hasSelection = !m_table->selectedItems().isEmpty();
    m_btnQuarantine->setEnabled(hasSelection);
    m_btnDelete->setEnabled(hasSelection);
    m_btnExclude->setEnabled(hasSelection);
    m_btnClearAll->setEnabled(m_table->rowCount() > 0);
}

QList<int> ActiveThreatsPage::selectedRows() const
{
    QSet<int> rows;
    for (auto *item : m_table->selectedItems())
        rows.insert(item->row());
    return rows.values();
}

void ActiveThreatsPage::onQuarantine()
{
    if (m_clam->isClamonaacRunning()) {
        ClamdOnAccessConfig cfg = m_cfgMgr->readConfig();
        if (cfg.preventionEnabled) {
            QMessageBox::warning(this, tr("Cannot Quarantine"),
                tr("Real-time protection is active with OnAccessPrevention enabled.\n\n"
                   "ClamAV blocks access to infected files, which prevents moving them "
                   "to quarantine.\n\n"
                   "To quarantine this file, temporarily disable the real-time scanning "
                   "service (clamav-clamonacc) from Settings \342\206\222 General, then try again."));
            return;
        }
    }

    auto rows    = selectedRows();
    auto threats = loadThreats();
    bool any     = false;

    QList<ActiveThreat> remaining;
    for (const ActiveThreat &t : threats) {
        bool doAction = false;
        for (int row : rows) {
            if (m_table->item(row, 0)->data(Qt::UserRole).toString() == t.id) {
                doAction = true;
                break;
            }
        }
        if (doAction) {
            if (m_quar->quarantineFile(t.filePath, t.threatName))
                any = true;
            // Remove from list regardless of quarantine success
        } else {
            remaining << t;
        }
    }

    if (any) {
        saveThreats(remaining);
        refresh();
        QMessageBox::information(this, tr("Quarantine"),
            tr("File quarantined successfully.\n\n"
               "To also exclude it from real-time protection, go to Exclusions "
               "and click \"Apply to Real-Time Protection\"."));
    }
}

void ActiveThreatsPage::onDeleteFile()
{
    auto rows = selectedRows();
    if (rows.isEmpty()) return;

    if (QMessageBox::question(this, tr("Delete Files"),
            tr("Permanently delete the selected file(s)?"),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    auto threats = loadThreats();
    QList<ActiveThreat> remaining;
    for (const ActiveThreat &t : threats) {
        bool doAction = false;
        for (int row : rows) {
            if (m_table->item(row, 0)->data(Qt::UserRole).toString() == t.id) {
                doAction = true;
                break;
            }
        }
        if (doAction)
            QFile::remove(t.filePath);
        else
            remaining << t;
    }
    saveThreats(remaining);
    refresh();
}

void ActiveThreatsPage::onAddExclusion()
{
    auto rows    = selectedRows();
    auto threats = loadThreats();
    QList<ActiveThreat> remaining;

    for (const ActiveThreat &t : threats) {
        bool doAction = false;
        for (int row : rows) {
            if (m_table->item(row, 0)->data(Qt::UserRole).toString() == t.id) {
                doAction = true;
                break;
            }
        }
        if (doAction)
            m_clam->addExclusion(t.filePath);
        else
            remaining << t;
    }
    saveThreats(remaining);
    refresh();

    QMessageBox::information(this, tr("Exclusion Added"),
        tr("File added to exclusions.\n\n"
           "To apply the exclusion to real-time protection, go to Exclusions "
           "and click \"Apply to Real-Time Protection\"."));
}

void ActiveThreatsPage::onClearAll()
{
    if (QMessageBox::question(this, tr("Clear All"),
            tr("Remove all entries from the threat history?"),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    saveThreats({});
    refresh();
}
