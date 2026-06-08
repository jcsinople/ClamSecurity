#include "ScheduledScansPage.h"
#include "SchedulerManager.h"
#include "QuarantineManager.h"
#include "ClamAvManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QHeaderView>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QTimeEdit>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>
#include <QPushButton>
#include <QCoreApplication>
#include <QUuid>
#include <QFile>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>

// ── Schedule edit dialog ───────────────────────────────────────────────────────

class ScheduleDialog : public QDialog
{
    Q_DECLARE_TR_FUNCTIONS(ScheduleDialog)
public:
    explicit ScheduleDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(tr("Schedule Scan"));
        setMinimumWidth(420);

        auto *form = new QFormLayout(this);
        form->setContentsMargins(16, 16, 16, 16);
        form->setSpacing(10);

        m_nameEdit = new QLineEdit(this);
        m_nameEdit->setPlaceholderText(tr("e.g. Daily Downloads Scan"));
        form->addRow(tr("Name:"), m_nameEdit);

        auto *pathRow = new QHBoxLayout;
        m_pathEdit = new QLineEdit(this);
        m_pathEdit->setPlaceholderText(tr("Folder to scan"));
        auto *browseBtn = new QPushButton(tr("Browse…"), this);
        browseBtn->setFixedWidth(90);
        pathRow->addWidget(m_pathEdit);
        pathRow->addWidget(browseBtn);
        form->addRow(tr("Target:"), pathRow);

        m_freqCombo = new QComboBox(this);
        m_freqCombo->addItem(tr("Daily"),   "daily");
        m_freqCombo->addItem(tr("Weekly"),  "weekly");
        m_freqCombo->addItem(tr("Monthly"), "monthly");
        form->addRow(tr("Frequency:"), m_freqCombo);

        m_dayCombo = new QComboBox(this);
        m_dayCombo->addItems({tr("Monday"), tr("Tuesday"), tr("Wednesday"),
                              tr("Thursday"), tr("Friday"), tr("Saturday"), tr("Sunday")});
        form->addRow(tr("Day:"), m_dayCombo);

        m_timeEdit = new QTimeEdit(this);
        m_timeEdit->setDisplayFormat("HH:mm");
        m_timeEdit->setTime(QTime(2, 0));
        form->addRow(tr("Time:"), m_timeEdit);

        // Quarantine checkbox + behavior hint
        m_quarChk = new QCheckBox(tr("Auto-quarantine detected threats"), this);
        form->addRow(QString(), m_quarChk);

        m_behaviorLabel = new QLabel(this);
        m_behaviorLabel->setWordWrap(true);
        m_behaviorLabel->setStyleSheet("color: palette(mid); font-size: 11px;");
        form->addRow(QString(), m_behaviorLabel);

        // Notification checkbox
        m_notifyChk = new QCheckBox(tr("Send system notification if threats are detected"), this);
        m_notifyChk->setChecked(true);
        form->addRow(QString(), m_notifyChk);

        auto *btns = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        form->addRow(btns);

        // Show/hide day selector
        auto updateDayVisibility = [this]() {
            bool weekly = (m_freqCombo->currentData().toString() == "weekly");
            m_dayCombo->setVisible(weekly);
            auto *lbl = qobject_cast<QLabel*>(
                qobject_cast<QFormLayout*>(layout())->labelForField(m_dayCombo));
            if (lbl) lbl->setVisible(weekly);
        };
        updateDayVisibility();
        connect(m_freqCombo, qOverload<int>(&QComboBox::currentIndexChanged),
                this, [updateDayVisibility](int) { updateDayVisibility(); });

        // Update behavior hint
        auto updateHint = [this]() {
            if (m_quarChk->isChecked()) {
                m_behaviorLabel->setText(
                    tr("Threats will be moved to quarantine automatically and "
                       "registered in the Quarantine section."));
            } else {
                m_behaviorLabel->setText(
                    tr("If threats are detected they will be listed in the "
                       "Detected Threats tab so you can take action. The file "
                       "will NOT be disabled automatically."));
            }
        };
        updateHint();
        connect(m_quarChk, &QCheckBox::toggled, this, [updateHint](bool) { updateHint(); });

        connect(browseBtn, &QPushButton::clicked, this, [this]() {
            QString dir = QFileDialog::getExistingDirectory(
                this, tr("Select folder to scan"),
                QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                QFileDialog::ShowDirsOnly);
            if (!dir.isEmpty())
                m_pathEdit->setText(dir);
        });

        connect(btns, &QDialogButtonBox::accepted, this, [this]() {
            if (m_nameEdit->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, tr("Validation"),
                    tr("Please enter a name for this schedule."));
                return;
            }
            if (m_pathEdit->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, tr("Validation"),
                    tr("Please select a target folder."));
                return;
            }
            accept();
        });
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    void setSchedule(const ScanSchedule &s)
    {
        m_nameEdit->setText(s.name);
        m_pathEdit->setText(s.path);
        for (int i = 0; i < m_freqCombo->count(); ++i) {
            if (m_freqCombo->itemData(i).toString() == s.frequency) {
                m_freqCombo->setCurrentIndex(i); break;
            }
        }
        m_dayCombo->setCurrentIndex(qBound(0, s.weekday, 6));
        QTime t = QTime::fromString(s.time, "HH:mm");
        if (t.isValid()) m_timeEdit->setTime(t);
        m_quarChk->setChecked(s.quarantine);
        m_notifyChk->setChecked(s.notify);
    }

    void applyTo(ScanSchedule &s) const
    {
        s.name      = m_nameEdit->text().trimmed();
        s.path      = m_pathEdit->text().trimmed();
        s.frequency = m_freqCombo->currentData().toString();
        s.weekday   = m_dayCombo->currentIndex();
        s.time      = m_timeEdit->time().toString("HH:mm");
        s.quarantine = m_quarChk->isChecked();
        s.notify    = m_notifyChk->isChecked();
    }

private:
    QLineEdit  *m_nameEdit;
    QLineEdit  *m_pathEdit;
    QComboBox  *m_freqCombo;
    QComboBox  *m_dayCombo;
    QTimeEdit  *m_timeEdit;
    QCheckBox  *m_quarChk;
    QCheckBox  *m_notifyChk;
    QLabel     *m_behaviorLabel;
};

// ── ScheduledScansPage ─────────────────────────────────────────────────────────

ScheduledScansPage::ScheduledScansPage(SchedulerManager *mgr,
                                       QuarantineManager *qmgr,
                                       ClamAvManager *clam,
                                       QWidget *parent)
    : QWidget(parent), m_mgr(mgr), m_qmgr(qmgr), m_clam(clam)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);

    auto *title = new QLabel(tr("Scheduled Scans"), this);
    QFont f = title->font(); f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    root->addWidget(title);

    m_tabs = new QTabWidget(this);

    auto *schedTab  = new QWidget;
    auto *threatsTab = new QWidget;
    buildSchedulesTab(schedTab);
    buildThreatsTab(threatsTab);

    m_tabs->addTab(schedTab,   QIcon::fromTheme("view-calendar"), tr("Schedules"));
    m_tabs->addTab(threatsTab, QIcon::fromTheme("security-low"),  tr("Detected Threats"));

    root->addWidget(m_tabs);

    // Nav row
    auto *navRow = new QHBoxLayout;
    m_btnBack = new QPushButton(QIcon::fromTheme("go-previous"), tr("Back"), this);
    navRow->addWidget(m_btnBack);
    navRow->addStretch();
    root->addLayout(navRow);

    connect(m_btnBack, &QPushButton::clicked, this, &ScheduledScansPage::backRequested);

    // React to scan log updates from systemd timer
    connect(m_mgr, &SchedulerManager::scanLogUpdated,
            this, &ScheduledScansPage::onScanLogUpdated);

    refresh();
}

// ── Build tabs ─────────────────────────────────────────────────────────────────

void ScheduledScansPage::buildSchedulesTab(QWidget *tab)
{
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(10);

    m_infoLabel = new QLabel(tab);
    m_infoLabel->setWordWrap(true);
    layout->addWidget(m_infoLabel);

    m_table = new QTableWidget(0, 5, tab);
    m_table->setHorizontalHeaderLabels({
        tr("Name"), tr("Target"), tr("Schedule"), tr("Last Result"), tr("Status")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    layout->addWidget(m_table);

    // Action buttons
    auto *actRow = new QHBoxLayout;
    m_btnAdd    = new QPushButton(QIcon::fromTheme("list-add"),       tr("Add"),            tab);
    m_btnEdit   = new QPushButton(QIcon::fromTheme("document-edit"),  tr("Edit"),           tab);
    m_btnRemove = new QPushButton(QIcon::fromTheme("list-remove"),    tr("Remove"),         tab);
    m_btnToggle = new QPushButton(QIcon::fromTheme("media-playback-pause"),
                                  tr("Enable/Disable"), tab);
    m_btnRunNow = new QPushButton(QIcon::fromTheme("system-run"),     tr("Run Now"),        tab);

    m_btnEdit->setEnabled(false);
    m_btnRemove->setEnabled(false);
    m_btnToggle->setEnabled(false);
    m_btnRunNow->setEnabled(false);

    actRow->addWidget(m_btnAdd);
    actRow->addWidget(m_btnEdit);
    actRow->addWidget(m_btnRemove);
    actRow->addWidget(m_btnToggle);
    actRow->addWidget(m_btnRunNow);
    actRow->addStretch();
    layout->addLayout(actRow);

    connect(m_btnAdd,    &QPushButton::clicked, this, &ScheduledScansPage::onAdd);
    connect(m_btnEdit,   &QPushButton::clicked, this, &ScheduledScansPage::onEdit);
    connect(m_btnRemove, &QPushButton::clicked, this, &ScheduledScansPage::onRemove);
    connect(m_btnToggle, &QPushButton::clicked, this, &ScheduledScansPage::onToggleEnabled);
    connect(m_btnRunNow, &QPushButton::clicked, this, &ScheduledScansPage::onRunNow);
    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &ScheduledScansPage::onSelectionChanged);
}

void ScheduledScansPage::buildThreatsTab(QWidget *tab)
{
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(10);

    auto *info = new QLabel(
        tr("Files detected as threats by scheduled scans (auto-quarantine disabled).\n"
           "Choose an action for each file: quarantine it, add its folder to exclusions, "
           "or delete the file permanently."), tab);
    info->setWordWrap(true);
    layout->addWidget(info);

    m_threatsTable = new QTableWidget(0, 4, tab);
    m_threatsTable->setHorizontalHeaderLabels({
        tr("File"), tr("Threat"), tr("Detected by"), tr("Date")
    });
    m_threatsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_threatsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_threatsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_threatsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_threatsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_threatsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_threatsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_threatsTable->setAlternatingRowColors(true);
    layout->addWidget(m_threatsTable);

    auto *actRow = new QHBoxLayout;
    m_btnQuarantine = new QPushButton(QIcon::fromTheme("security-high"),
                                     tr("Move to Quarantine"), tab);
    m_btnExclude    = new QPushButton(QIcon::fromTheme("list-add"),
                                     tr("Add Folder to Exclusions"), tab);
    m_btnDeleteFile = new QPushButton(QIcon::fromTheme("edit-delete"),
                                     tr("Delete File"), tab);

    m_btnQuarantine->setEnabled(false);
    m_btnExclude->setEnabled(false);
    m_btnDeleteFile->setEnabled(false);

    actRow->addWidget(m_btnQuarantine);
    actRow->addWidget(m_btnExclude);
    actRow->addWidget(m_btnDeleteFile);
    actRow->addStretch();
    layout->addLayout(actRow);

    connect(m_btnQuarantine, &QPushButton::clicked, this, &ScheduledScansPage::onThreatQuarantine);
    connect(m_btnExclude,    &QPushButton::clicked, this, &ScheduledScansPage::onThreatExclude);
    connect(m_btnDeleteFile, &QPushButton::clicked, this, &ScheduledScansPage::onThreatDelete);
    connect(m_threatsTable, &QTableWidget::itemSelectionChanged,
            this, &ScheduledScansPage::onThreatSelectionChanged);
}

// ── Refresh ────────────────────────────────────────────────────────────────────

void ScheduledScansPage::refresh()
{
    m_table->setRowCount(0);

    const auto list = m_mgr->schedules();
    for (const ScanSchedule &s : list) {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        // Build "Last Result" cell
        ScanSummary summary = m_mgr->readScanSummary(s.id);
        QString resultText;
        QColor  resultColor;
        if (!summary.hasResults) {
            resultText  = tr("—");
            resultColor = QColor(0x9E, 0x9E, 0x9E);
        } else if (summary.infected > 0) {
            resultText  = tr("⚠ %n threat(s)", nullptr, summary.infected);
            if (summary.scanTime.isValid())
                resultText += " · " + summary.scanTime.toString("dd/MM/yyyy");
            resultColor = QColor(0xC6, 0x28, 0x28);
        } else {
            resultText  = tr("✓ Clean");
            if (summary.scanTime.isValid())
                resultText += " · " + summary.scanTime.toString("dd/MM/yyyy");
            resultColor = QColor(0x2E, 0x7D, 0x32);
        }

        auto *nameItem   = new QTableWidgetItem(s.name);
        auto *pathItem   = new QTableWidgetItem(s.path);
        auto *schedItem  = new QTableWidgetItem(m_mgr->scheduleDescription(s));
        auto *resultItem = new QTableWidgetItem(resultText);
        auto *statusItem = new QTableWidgetItem(s.enabled ? tr("Enabled") : tr("Disabled"));

        nameItem->setData(Qt::UserRole, s.id);
        resultItem->setForeground(resultColor);
        statusItem->setForeground(s.enabled
            ? QColor(0x2E, 0x7D, 0x32)
            : QColor(0x9E, 0x9E, 0x9E));

        m_table->setItem(row, 0, nameItem);
        m_table->setItem(row, 1, pathItem);
        m_table->setItem(row, 2, schedItem);
        m_table->setItem(row, 3, resultItem);
        m_table->setItem(row, 4, statusItem);
    }

    m_infoLabel->setText(
        list.isEmpty()
            ? tr("No scheduled scans configured. Click Add to create one.")
            : tr("%n scheduled scan(s) configured. "
                 "Scans run automatically via systemd user timers.", nullptr, list.size()));

    onSelectionChanged();
    refreshThreats();
}

void ScheduledScansPage::refreshThreats()
{
    m_threatsTable->setRowCount(0);
    const auto threats = m_mgr->scheduledThreats();
    for (const ScheduledThreat &t : threats) {
        int row = m_threatsTable->rowCount();
        m_threatsTable->insertRow(row);

        QFileInfo fi(t.filePath);
        auto *fileItem   = new QTableWidgetItem(fi.fileName());
        fileItem->setToolTip(t.filePath);
        auto *threatItem = new QTableWidgetItem(t.threatName);
        auto *scanItem   = new QTableWidgetItem(t.scheduleName);
        auto *dateItem   = new QTableWidgetItem(
            t.detectedAt.isValid() ? t.detectedAt.toString("dd/MM/yyyy HH:mm") : tr("Unknown"));

        fileItem->setData(Qt::UserRole, t.id);  // threat record ID
        fileItem->setData(Qt::UserRole + 1, t.filePath);
        fileItem->setData(Qt::UserRole + 2, t.threatName);

        m_threatsTable->setItem(row, 0, fileItem);
        m_threatsTable->setItem(row, 1, threatItem);
        m_threatsTable->setItem(row, 2, scanItem);
        m_threatsTable->setItem(row, 3, dateItem);
    }

    // Update tab badge
    if (!threats.isEmpty())
        m_tabs->setTabText(1, tr("Detected Threats (%1)").arg(threats.size()));
    else
        m_tabs->setTabText(1, tr("Detected Threats"));

    onThreatSelectionChanged();
}

// ── Process new scan results ───────────────────────────────────────────────────

void ScheduledScansPage::processNewResults(const QString &scheduleId)
{
    if (m_mgr->isLogProcessed(scheduleId)) return;

    const auto sched = m_mgr->schedules();
    auto it = std::find_if(sched.begin(), sched.end(),
        [&scheduleId](const ScanSchedule &s) { return s.id == scheduleId; });
    if (it == sched.end()) return;

    const ScanSchedule &s = *it;
    const auto infected = m_mgr->readInfectedFiles(scheduleId);

    if (!infected.isEmpty()) {
        if (s.quarantine) {
            // Auto-quarantine: move each infected file through QuarantineManager
            for (const auto &pair : infected) {
                const QString &path   = pair.first;
                const QString &threat = pair.second;
                if (QFile::exists(path))
                    m_qmgr->quarantineFile(path, threat);
            }
        } else {
            // No auto-quarantine: record in the threats list
            QDateTime now = QDateTime::currentDateTime();
            for (const auto &pair : infected) {
                ScheduledThreat t;
                t.id           = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
                t.scheduleId   = scheduleId;
                t.scheduleName = s.name;
                t.filePath     = pair.first;
                t.threatName   = pair.second;
                t.detectedAt   = now;
                m_mgr->addScheduledThreat(t);
            }
        }

        if (s.notify)
            emit threatNotification(s.name, infected.size());
    }

    m_mgr->markLogProcessed(scheduleId);
}

void ScheduledScansPage::onScanLogUpdated(const QString &scheduleId)
{
    processNewResults(scheduleId);
    refresh();
}

// ── Schedules slots ────────────────────────────────────────────────────────────

void ScheduledScansPage::onAdd()
{
    ScanSchedule s;
    s.path      = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    s.frequency = "daily";
    s.time      = "02:00";
    openScheduleDialog(&s);
}

void ScheduledScansPage::onEdit()
{
    int row = m_table->currentRow();
    if (row < 0) return;

    QString id = m_table->item(row, 0)->data(Qt::UserRole).toString();
    const auto list = m_mgr->schedules();
    auto it = std::find_if(list.begin(), list.end(),
        [&id](const ScanSchedule &s) { return s.id == id; });
    if (it == list.end()) return;

    ScanSchedule s = *it;
    openScheduleDialog(&s);
}

void ScheduledScansPage::onRemove()
{
    int row = m_table->currentRow();
    if (row < 0) return;

    QString id   = m_table->item(row, 0)->data(Qt::UserRole).toString();
    QString name = m_table->item(row, 0)->text();

    if (QMessageBox::question(this, tr("Remove Schedule"),
            tr("Remove the schedule \"%1\"?\n\n"
               "This will also delete the associated systemd timer.").arg(name),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    m_mgr->removeSchedule(id);
    refresh();
}

void ScheduledScansPage::onToggleEnabled()
{
    int row = m_table->currentRow();
    if (row < 0) return;

    QString id = m_table->item(row, 0)->data(Qt::UserRole).toString();
    const auto list = m_mgr->schedules();
    auto it = std::find_if(list.begin(), list.end(),
        [&id](const ScanSchedule &s) { return s.id == id; });
    if (it == list.end()) return;

    m_mgr->setScheduleEnabled(id, !it->enabled);
    refresh();
}

void ScheduledScansPage::onRunNow()
{
    int row = m_table->currentRow();
    if (row < 0) return;

    QString id   = m_table->item(row, 0)->data(Qt::UserRole).toString();
    QString name = m_table->item(row, 0)->text();

    m_mgr->runNow(id);
    QMessageBox::information(this, tr("Scan started"),
        tr("Scan \"%1\" started in the background.\n\n"
           "Results will be shown here when the scan finishes.").arg(name));
}

void ScheduledScansPage::onSelectionChanged()
{
    bool sel = m_table->currentRow() >= 0;
    m_btnEdit->setEnabled(sel);
    m_btnRemove->setEnabled(sel);
    m_btnToggle->setEnabled(sel);
    m_btnRunNow->setEnabled(sel);
}

// ── Threats slots ──────────────────────────────────────────────────────────────

void ScheduledScansPage::onThreatSelectionChanged()
{
    bool sel = m_threatsTable->currentRow() >= 0;
    m_btnQuarantine->setEnabled(sel);
    m_btnExclude->setEnabled(sel);
    m_btnDeleteFile->setEnabled(sel);
}

void ScheduledScansPage::onThreatQuarantine()
{
    int row = m_threatsTable->currentRow();
    if (row < 0) return;

    auto *item = m_threatsTable->item(row, 0);
    QString threatId = item->data(Qt::UserRole).toString();
    QString filePath = item->data(Qt::UserRole + 1).toString();
    QString threat   = item->data(Qt::UserRole + 2).toString();

    if (!QFile::exists(filePath)) {
        QMessageBox::warning(this, tr("File not found"),
            tr("The file no longer exists:\n%1").arg(filePath));
        m_mgr->removeScheduledThreat(threatId);
        refreshThreats();
        return;
    }

    if (m_qmgr->quarantineFile(filePath, threat)) {
        m_mgr->removeScheduledThreat(threatId);
        refreshThreats();
    } else {
        QMessageBox::critical(this, tr("Error"),
            tr("Could not move the file to quarantine.\nCheck file permissions."));
    }
}

void ScheduledScansPage::onThreatExclude()
{
    int row = m_threatsTable->currentRow();
    if (row < 0) return;

    auto *item = m_threatsTable->item(row, 0);
    QString threatId = item->data(Qt::UserRole).toString();
    QString filePath = item->data(Qt::UserRole + 1).toString();
    QString folder   = QFileInfo(filePath).absolutePath();

    m_clam->addExclusion(folder);
    m_mgr->removeScheduledThreat(threatId);
    refreshThreats();

    QMessageBox::information(this, tr("Exclusion added"),
        tr("The folder has been added to scan exclusions:\n%1\n\n"
           "Future scans will skip this folder.").arg(folder));
}

void ScheduledScansPage::onThreatDelete()
{
    int row = m_threatsTable->currentRow();
    if (row < 0) return;

    auto *item = m_threatsTable->item(row, 0);
    QString threatId = item->data(Qt::UserRole).toString();
    QString filePath = item->data(Qt::UserRole + 1).toString();

    if (QMessageBox::question(this, tr("Delete file"),
            tr("Permanently delete this file?\n\n%1\n\n"
               "This action cannot be undone.").arg(filePath),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    if (!QFile::exists(filePath) || QFile::remove(filePath)) {
        m_mgr->removeScheduledThreat(threatId);
        refreshThreats();
    } else {
        QMessageBox::critical(this, tr("Error"),
            tr("Could not delete the file. Check permissions."));
    }
}

// ── Dialog helper ──────────────────────────────────────────────────────────────

void ScheduledScansPage::openScheduleDialog(ScanSchedule *s)
{
    ScheduleDialog dlg(this);
    dlg.setSchedule(*s);

    if (dlg.exec() != QDialog::Accepted)
        return;

    dlg.applyTo(*s);

    if (s->id.isEmpty())
        m_mgr->addSchedule(*s);
    else
        m_mgr->updateSchedule(*s);

    refresh();
}
