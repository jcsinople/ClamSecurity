#include "ScheduledScansPage.h"
#include "SchedulerManager.h"
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

// ── Schedule edit dialog ───────────────────────────────────────────────────────

class ScheduleDialog : public QDialog
{
    Q_DECLARE_TR_FUNCTIONS(ScheduleDialog)
public:
    explicit ScheduleDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(tr("Schedule Scan"));
        setMinimumWidth(400);

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

        m_quarChk = new QCheckBox(
            tr("Auto-quarantine detected threats"), this);
        form->addRow(QString(), m_quarChk);

        auto *btns = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        form->addRow(btns);

        // Show/hide day selector based on frequency
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

    // Populate dialog from existing schedule
    void setSchedule(const ScanSchedule &s)
    {
        m_nameEdit->setText(s.name);
        m_pathEdit->setText(s.path);
        for (int i = 0; i < m_freqCombo->count(); ++i) {
            if (m_freqCombo->itemData(i).toString() == s.frequency) {
                m_freqCombo->setCurrentIndex(i);
                break;
            }
        }
        m_dayCombo->setCurrentIndex(qBound(0, s.weekday, 6));
        QTime t = QTime::fromString(s.time, "HH:mm");
        if (t.isValid()) m_timeEdit->setTime(t);
        m_quarChk->setChecked(s.quarantine);
    }

    // Extract schedule from dialog
    void applyTo(ScanSchedule &s) const
    {
        s.name      = m_nameEdit->text().trimmed();
        s.path      = m_pathEdit->text().trimmed();
        s.frequency = m_freqCombo->currentData().toString();
        s.weekday   = m_dayCombo->currentIndex();
        s.time      = m_timeEdit->time().toString("HH:mm");
        s.quarantine = m_quarChk->isChecked();
    }

private:
    QLineEdit  *m_nameEdit;
    QLineEdit  *m_pathEdit;
    QComboBox  *m_freqCombo;
    QComboBox  *m_dayCombo;
    QTimeEdit  *m_timeEdit;
    QCheckBox  *m_quarChk;
};

// ── ScheduledScansPage ─────────────────────────────────────────────────────────

ScheduledScansPage::ScheduledScansPage(SchedulerManager *mgr, QWidget *parent)
    : QWidget(parent), m_mgr(mgr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *title = new QLabel(tr("Scheduled Scans"), this);
    QFont f = title->font(); f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    m_infoLabel = new QLabel(
        tr("Scheduled scans run automatically using systemd user timers.\n"
           "No root permissions are required."), this);
    m_infoLabel->setWordWrap(true);
    layout->addWidget(m_infoLabel);

    // Table
    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({
        tr("Name"), tr("Target"), tr("Schedule"), tr("Status")
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    layout->addWidget(m_table);

    // Action buttons
    auto *actRow = new QHBoxLayout;
    m_btnAdd    = new QPushButton(QIcon::fromTheme("list-add"),       tr("Add"),       this);
    m_btnEdit   = new QPushButton(QIcon::fromTheme("document-edit"),  tr("Edit"),      this);
    m_btnRemove = new QPushButton(QIcon::fromTheme("list-remove"),    tr("Remove"),    this);
    m_btnToggle = new QPushButton(QIcon::fromTheme("media-playback-pause"),
                                  tr("Enable/Disable"), this);
    m_btnRunNow = new QPushButton(QIcon::fromTheme("system-run"),     tr("Run Now"),   this);

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

    // Nav row
    auto *navRow = new QHBoxLayout;
    m_btnBack = new QPushButton(QIcon::fromTheme("go-previous"), tr("Back"), this);
    navRow->addWidget(m_btnBack);
    navRow->addStretch();
    layout->addLayout(navRow);

    connect(m_btnAdd,    &QPushButton::clicked,
            this, &ScheduledScansPage::onAdd);
    connect(m_btnEdit,   &QPushButton::clicked,
            this, &ScheduledScansPage::onEdit);
    connect(m_btnRemove, &QPushButton::clicked,
            this, &ScheduledScansPage::onRemove);
    connect(m_btnToggle, &QPushButton::clicked,
            this, &ScheduledScansPage::onToggleEnabled);
    connect(m_btnRunNow, &QPushButton::clicked,
            this, &ScheduledScansPage::onRunNow);
    connect(m_btnBack,   &QPushButton::clicked,
            this, &ScheduledScansPage::backRequested);
    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &ScheduledScansPage::onSelectionChanged);

    refresh();
}

// ── Refresh ────────────────────────────────────────────────────────────────────

void ScheduledScansPage::refresh()
{
    m_table->setRowCount(0);

    const auto list = m_mgr->schedules();
    for (const ScanSchedule &s : list) {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        auto *nameItem   = new QTableWidgetItem(s.name);
        auto *pathItem   = new QTableWidgetItem(s.path);
        auto *schedItem  = new QTableWidgetItem(m_mgr->scheduleDescription(s));
        auto *statusItem = new QTableWidgetItem(s.enabled ? tr("Enabled") : tr("Disabled"));

        // Store ID in the first item for later retrieval
        nameItem->setData(Qt::UserRole, s.id);
        statusItem->setForeground(s.enabled
            ? QColor(0x2E, 0x7D, 0x32)
            : QColor(0x9E, 0x9E, 0x9E));

        m_table->setItem(row, 0, nameItem);
        m_table->setItem(row, 1, pathItem);
        m_table->setItem(row, 2, schedItem);
        m_table->setItem(row, 3, statusItem);
    }

    m_infoLabel->setText(
        list.isEmpty()
            ? tr("No scheduled scans configured. Click Add to create one.")
            : tr("%n scheduled scan(s) configured. "
                 "Scans run automatically via systemd user timers.", nullptr, list.size()));

    onSelectionChanged();
}

// ── Slots ──────────────────────────────────────────────────────────────────────

void ScheduledScansPage::onAdd()
{
    ScanSchedule s;
    // Sensible default: Downloads, daily at 02:00
    s.path = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    s.frequency = "daily";
    s.time = "02:00";
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
           "Results will be saved to the scan log.").arg(name));
}

void ScheduledScansPage::onSelectionChanged()
{
    bool sel = m_table->currentRow() >= 0;
    m_btnEdit->setEnabled(sel);
    m_btnRemove->setEnabled(sel);
    m_btnToggle->setEnabled(sel);
    m_btnRunNow->setEnabled(sel);
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
