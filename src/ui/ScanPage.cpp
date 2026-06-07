#include "ScanPage.h"
#include "ClamAvManager.h"
#include "QuarantineManager.h"
#include "ClamdConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFont>
#include <QProcess>
#include <QLocale>

ScanPage::ScanPage(ClamAvManager      *clam,
                   QuarantineManager  *quar,
                   ClamdConfigManager *cfgMgr,
                   QWidget *parent)
    : QWidget(parent), m_clam(clam), m_quar(quar), m_cfgMgr(cfgMgr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 20, 20, 20);

    // ── Title ─────────────────────────────────────────────────────────────
    auto *titleRow = new QHBoxLayout;
    auto *title = new QLabel(tr("Scan"), this);
    QFont f = title->font(); f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    titleRow->addWidget(title);
    titleRow->addStretch();
    layout->addLayout(titleRow);

    // ── Scanned path ──────────────────────────────────────────────────────
    m_pathLabel = new QLabel(tr("No scan running."), this);
    QFont pf = m_pathLabel->font(); pf.setPointSize(9); pf.setItalic(true);
    m_pathLabel->setFont(pf);
    m_pathLabel->setWordWrap(true);
    layout->addWidget(m_pathLabel);

    // ── Progress ──────────────────────────────────────────────────────────
    auto *progGroup  = new QGroupBox(tr("Progress"), this);
    auto *progLayout = new QVBoxLayout(progGroup);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);       // indeterminate until total is known
    m_progress->setVisible(false);
    progLayout->addWidget(m_progress);

    m_currentFileLabel = new QLabel(this);
    m_currentFileLabel->setVisible(false);
    QFont sf = m_currentFileLabel->font(); sf.setPointSize(8);
    m_currentFileLabel->setFont(sf);
    m_currentFileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    progLayout->addWidget(m_currentFileLabel);

    m_statsLabel = new QLabel(tr("Files: 0  |  Threats: 0"), this);
    progLayout->addWidget(m_statsLabel);

    m_statusLabel = new QLabel(tr("Ready to scan."), this);
    progLayout->addWidget(m_statusLabel);

    layout->addWidget(progGroup);

    // ── Threat list ───────────────────────────────────────────────────────
    auto *threatGroup  = new QGroupBox(tr("Detected Threats"), this);
    auto *threatLayout = new QVBoxLayout(threatGroup);

    m_threatList = new QListWidget(this);
    // Multi-selection: checkboxes + shift/click both work
    m_threatList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    threatLayout->addWidget(m_threatList);

    // Action buttons for checked items
    auto *actRow      = new QHBoxLayout;
    m_btnQuarantine   = new QPushButton(QIcon::fromTheme("security-low"),
                                        tr("Quarantine Checked"), this);
    m_btnIgnore       = new QPushButton(QIcon::fromTheme("list-remove"),
                                        tr("Ignore Checked"), this);
    m_btnAddExcl      = new QPushButton(QIcon::fromTheme("folder-open"),
                                        tr("Add to Exclusions"), this);
    m_btnQuarantine->setEnabled(false);
    m_btnIgnore->setEnabled(false);
    m_btnAddExcl->setEnabled(false);
    actRow->addWidget(m_btnQuarantine);
    actRow->addWidget(m_btnIgnore);
    actRow->addWidget(m_btnAddExcl);
    threatLayout->addLayout(actRow);

    layout->addWidget(threatGroup);

    // ── Navigation / scan control ─────────────────────────────────────────
    auto *btnLayout = new QHBoxLayout;
    m_btnBack   = new QPushButton(QIcon::fromTheme("go-previous"), tr("Back"),     this);
    m_btnHome   = new QPushButton(QIcon::fromTheme("go-home"),     tr("Scan HOME"), this);
    m_btnCustom = new QPushButton(QIcon::fromTheme("document-open"),
                                  tr("Custom Scan…"), this);
    m_btnStop   = new QPushButton(QIcon::fromTheme("process-stop"), tr("Stop"),    this);
    m_btnStop->setEnabled(false);
    btnLayout->addWidget(m_btnBack);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnHome);
    btnLayout->addWidget(m_btnCustom);
    btnLayout->addWidget(m_btnStop);
    layout->addLayout(btnLayout);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_btnHome,       &QPushButton::clicked, this, &ScanPage::onScanHome);
    connect(m_btnCustom,     &QPushButton::clicked, this, &ScanPage::onScanCustom);
    connect(m_btnStop,       &QPushButton::clicked, this, &ScanPage::onStopScan);
    connect(m_btnBack,       &QPushButton::clicked, this, &ScanPage::onBackClicked);
    connect(m_btnQuarantine, &QPushButton::clicked, this, &ScanPage::onQuarantineChecked);
    connect(m_btnIgnore,     &QPushButton::clicked, this, &ScanPage::onIgnoreChecked);
    connect(m_btnAddExcl,    &QPushButton::clicked, this, &ScanPage::onAddExclusionChecked);
    connect(m_threatList, &QListWidget::itemChanged,
            this, &ScanPage::onItemChanged);
}

// ─── Scan control ─────────────────────────────────────────────────────────────

void ScanPage::startScan(const QString &path)
{
    if (m_thread) return;   // already scanning

    m_currentPath = path;
    clearResults();
    setScanningState(true);
    m_pathLabel->setText(tr("Target: %1").arg(path));
    m_statusLabel->setText(tr("Counting files…"));

    // ── Phase 1: count total files for determinate progress bar ───────────
    m_totalFiles = 0;
    m_progress->setRange(0, 0);   // indeterminate while counting

    m_countProcess = new QProcess(this);
    connect(m_countProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &ScanPage::onTotalCountReady);
    // Single-quote the path to handle spaces; escape any existing single quotes
    QString safePath = path;
    safePath.replace("'", "'\\''");
    QString cmd = "find '" + safePath + "' -type f -readable 2>/dev/null | wc -l";
    m_countProcess->start("sh", QStringList{"-c", cmd});
}

void ScanPage::onTotalCountReady()
{
    if (m_countProcess) {
        bool ok;
        int total = m_countProcess->readAllStandardOutput().trimmed().toInt(&ok);
        if (ok && total > 0) {
            m_totalFiles = total;
            m_progress->setRange(0, total);
        }
        m_countProcess->deleteLater();
        m_countProcess = nullptr;
    }

    // ── Phase 2: start clamscan ───────────────────────────────────────────
    m_statusLabel->setText(tr("Scanning…"));

    m_thread = new QThread(this);
    m_worker = new ScanWorker(m_currentPath, m_clam->exclusions(),
                              m_clam->excludedExtensions());
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started,         m_worker, &ScanWorker::startScan);
    connect(m_worker, &ScanWorker::progressUpdate,
            this, &ScanPage::onProgress);
    connect(m_worker, &ScanWorker::threatFound,
            this, &ScanPage::onThreatFound);
    connect(m_worker, &ScanWorker::scanFinished,
            this, &ScanPage::onScanFinished);
    connect(m_worker, &ScanWorker::scanError,
            this, [this](const QString &msg) {
        QMessageBox::warning(this, tr("Scan Error"), msg);
    });
    // QueuedConnection → stopScan() runs in worker thread, no cross-thread crash
    connect(this, &ScanPage::requestStopScan,
            m_worker, &ScanWorker::stopScan, Qt::QueuedConnection);
    connect(m_worker, &ScanWorker::scanFinished, m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

    m_thread->start();
}

bool ScanPage::checkRealtimeConflict(const QString &scanPath)
{
    if (!m_clam->isClamonaacRunning()) return false;

    ClamdOnAccessConfig cfg = m_cfgMgr->readConfig();
    bool overlap = false;
    for (const QString &watched : cfg.includePaths) {
        if (scanPath.startsWith(watched) || watched.startsWith(scanPath)) {
            overlap = true;
            break;
        }
    }
    if (!overlap) return false;

    QMessageBox dlg(this);
    dlg.setWindowTitle(tr("Real-Time Protection Active"));
    dlg.setIcon(QMessageBox::Warning);
    dlg.setText(
        tr("The folder you are about to scan is monitored by real-time protection.\n\n"
           "Scanning while real-time protection is active may cause conflicts "
           "or slow down the scan significantly.\n\n"
           "It is recommended to temporarily disable the real-time scanning service "
           "(clamav-clamonacc) from Settings \342\206\222 General before scanning, "
           "and re-enable it manually afterwards."));
    dlg.addButton(tr("Scan Anyway"), QMessageBox::AcceptRole);
    QPushButton *cancelBtn = dlg.addButton(tr("Cancel"), QMessageBox::RejectRole);
    dlg.exec();
    return dlg.clickedButton() == cancelBtn;
}

void ScanPage::onScanHome()
{
    if (checkRealtimeConflict(QDir::homePath())) return;
    startScan(QDir::homePath());
}

void ScanPage::onScanCustom()
{
    QString path = QFileDialog::getExistingDirectory(
        this, tr("Select directory to scan"), QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (path.isEmpty()) return;
    if (checkRealtimeConflict(path)) return;
    startScan(path);
}

void ScanPage::onStopScan()
{
    // Emit signal — worker receives via QueuedConnection in its own thread
    emit requestStopScan();

    // Also abort the pre-count if still running
    if (m_countProcess) {
        m_countProcess->kill();
        m_countProcess->deleteLater();
        m_countProcess = nullptr;
    }
}

// ─── Progress ──────────────────────────────────────────────────────────────────

void ScanPage::onProgress(int count, int infected,
                           const QString &file, qint64 elapsedMs)
{
    if (m_totalFiles > 0)
        m_progress->setValue(count);

    m_currentFileLabel->setText(file);
    formatStats(count, infected, elapsedMs);
}

void ScanPage::formatStats(int files, int threats, qint64 elapsedMs)
{
    double secs = elapsedMs / 1000.0;
    double speed = (secs > 0.5) ? (files / secs) : 0;

    QString elapsed;
    int es = int(secs);
    elapsed = QString("%1:%2")
        .arg(es / 60, 2, 10, QChar('0'))
        .arg(es % 60, 2, 10, QChar('0'));

    QString eta;
    if (m_totalFiles > 0 && speed > 1 && files < m_totalFiles) {
        int remaining = int((m_totalFiles - files) / speed);
        eta = tr("  |  ~%1:%2 left")
              .arg(remaining / 60, 2, 10, QChar('0'))
              .arg(remaining % 60, 2, 10, QChar('0'));
    }

    QString speedStr = speed > 0.5
        ? tr("  |  %1 files/s").arg(int(speed)) : QString();

    m_statsLabel->setText(
        tr("Files: %1  |  Threats: %2  |  %3%4%5")
        .arg(QLocale().toString(files))
        .arg(threats)
        .arg(elapsed)
        .arg(speedStr)
        .arg(eta));
}

void ScanPage::onThreatFound(const QString &file, const QString &threat)
{
    auto *item = new QListWidgetItem(
        QIcon::fromTheme("security-low"),
        tr("%1  →  %2").arg(file, threat),
        m_threatList);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Unchecked);

    ThreatItem data{file, threat, false};
    item->setData(Qt::UserRole, QVariant::fromValue(data));

    m_threatList->scrollToBottom();
    onItemChanged(item);   // update button states
}

void ScanPage::onScanFinished(int scanned, int infected, bool cancelled)
{
    setScanningState(false);
    m_thread = nullptr;
    m_worker = nullptr;

    if (m_totalFiles > 0) m_progress->setValue(scanned);

    if (cancelled) {
        m_statusLabel->setText(tr("Scan stopped by user."));
    } else if (infected == 0) {
        m_statusLabel->setText(tr("Scan complete — no threats found."));
    } else {
        m_statusLabel->setText(
            tr("Scan complete — %1 threat(s) found.").arg(infected));
    }
    formatStats(scanned, infected, 0);
}

// ─── Actions on threats ────────────────────────────────────────────────────────

void ScanPage::onItemChanged(QListWidgetItem *)
{
    bool anyChecked = !checkedItems().isEmpty();
    m_btnQuarantine->setEnabled(anyChecked);
    m_btnIgnore->setEnabled(anyChecked);
    m_btnAddExcl->setEnabled(anyChecked);
}

void ScanPage::onQuarantineChecked()
{
    bool any = false;
    for (QListWidgetItem *item : checkedItems()) {
        ThreatItem data = item->data(Qt::UserRole).value<ThreatItem>();
        if (m_quar->quarantineFile(data.filePath, data.threatName)) {
            data.actionTaken = true;
            item->setData(Qt::UserRole, QVariant::fromValue(data));
            item->setIcon(QIcon::fromTheme("edit-delete"));
            item->setText(item->text() + tr("  [QUARANTINED]"));
            item->setFlags(item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable));
            any = true;
        }
    }
    onItemChanged(nullptr);
    if (any) {
        QMessageBox::information(this, tr("Quarantine"),
            tr("File quarantined successfully.\n\n"
               "To also exclude it from real-time protection, go to Exclusions "
               "and click \"Apply to Real-Time Protection\"."));
    }
}

void ScanPage::onIgnoreChecked()
{
    for (QListWidgetItem *item : checkedItems()) {
        ThreatItem data = item->data(Qt::UserRole).value<ThreatItem>();
        data.actionTaken = true;
        item->setData(Qt::UserRole, QVariant::fromValue(data));
        item->setIcon(QIcon::fromTheme("dialog-ok"));
        item->setText(item->text() + tr("  [IGNORED]"));
        item->setFlags(item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable));
    }
    onItemChanged(nullptr);
}

void ScanPage::onAddExclusionChecked()
{
    for (QListWidgetItem *item : checkedItems()) {
        ThreatItem data = item->data(Qt::UserRole).value<ThreatItem>();
        m_clam->addExclusion(data.filePath);   // add the file itself, not the folder
        data.actionTaken = true;
        item->setData(Qt::UserRole, QVariant::fromValue(data));
        item->setText(item->text() + tr("  [EXCLUDED]"));
        item->setFlags(item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable));
    }
    onItemChanged(nullptr);
}

// ─── Back navigation ───────────────────────────────────────────────────────────

void ScanPage::onBackClicked()
{
    if (m_thread) {
        QMessageBox::information(this, tr("Scan in progress"),
            tr("Please stop the scan before leaving."));
        return;
    }

    if (hasUnhandledThreats()) {
        QMessageBox dlg(this);
        dlg.setWindowTitle(tr("Unhandled threats"));
        dlg.setText(tr("There are threats that have not been quarantined or ignored.\n"
                       "What would you like to do?"));
        dlg.setIcon(QMessageBox::Warning);
        auto *btnQuar   = dlg.addButton(tr("Quarantine All and Leave"),
                                         QMessageBox::AcceptRole);
        auto *btnIgnore = dlg.addButton(tr("Ignore All and Leave"),
                                         QMessageBox::DestructiveRole);
        dlg.addButton(tr("Stay Here"), QMessageBox::RejectRole);
        dlg.exec();

        if (dlg.clickedButton() == btnQuar) {
            // Check all, then quarantine
            for (int i = 0; i < m_threatList->count(); ++i) {
                auto *item = m_threatList->item(i);
                if (item->flags() & Qt::ItemIsUserCheckable)
                    item->setCheckState(Qt::Checked);
            }
            onQuarantineChecked();
        } else if (dlg.clickedButton() == btnIgnore) {
            for (int i = 0; i < m_threatList->count(); ++i) {
                auto *item = m_threatList->item(i);
                if (item->flags() & Qt::ItemIsUserCheckable)
                    item->setCheckState(Qt::Checked);
            }
            onIgnoreChecked();
        } else {
            return;  // Stay
        }
    }

    clearResults();
    emit backRequested();
}

// ─── Helpers ───────────────────────────────────────────────────────────────────

bool ScanPage::hasUnhandledThreats() const
{
    for (int i = 0; i < m_threatList->count(); ++i) {
        auto *item = m_threatList->item(i);
        ThreatItem data = item->data(Qt::UserRole).value<ThreatItem>();
        if (!data.actionTaken && !data.filePath.isEmpty())
            return true;
    }
    return false;
}

QList<QListWidgetItem*> ScanPage::checkedItems() const
{
    QList<QListWidgetItem*> result;
    for (int i = 0; i < m_threatList->count(); ++i) {
        QListWidgetItem *item = m_threatList->item(i);
        if ((item->flags() & Qt::ItemIsUserCheckable) &&
            item->checkState() == Qt::Checked)
            result << item;
    }
    return result;
}

void ScanPage::setScanningState(bool scanning)
{
    m_progress->setVisible(scanning);
    m_currentFileLabel->setVisible(scanning);
    m_btnHome->setEnabled(!scanning);
    m_btnCustom->setEnabled(!scanning);
    m_btnStop->setEnabled(scanning);
    m_btnBack->setEnabled(!scanning);
}

void ScanPage::clearResults()
{
    m_threatList->clear();
    m_pathLabel->setText(tr("No scan running."));
    m_statsLabel->setText(tr("Files: 0  |  Threats: 0"));
    m_currentFileLabel->clear();
    m_statusLabel->setText(tr("Ready to scan."));
    m_progress->setRange(0, 0);
    m_progress->setValue(0);
    m_totalFiles = 0;
    onItemChanged(nullptr);
}
