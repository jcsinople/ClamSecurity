#include "ExclusionsPage.h"
#include "ClamAvManager.h"
#include "ClamdConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QFileDialog>
#include <QDir>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QTabWidget>
#include <QRegularExpression>

ExclusionsPage::ExclusionsPage(ClamAvManager     *clam,
                               ClamdConfigManager *cfgMgr,
                               QWidget *parent)
    : QWidget(parent), m_clam(clam), m_cfgMgr(cfgMgr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *title = new QLabel(tr("Scan Exclusions"), this);
    QFont f = title->font(); f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    // ── Tab widget ────────────────────────────────────────────────────────
    auto *tabs = new QTabWidget(this);

    // ── Tab 0: Paths ──────────────────────────────────────────────────────
    auto *pathsTab = new QWidget(tabs);
    auto *pathsLay = new QVBoxLayout(pathsTab);
    pathsLay->setContentsMargins(8, 8, 8, 8);
    pathsLay->setSpacing(10);

    pathsLay->addWidget(new QLabel(
        tr("Files and folders listed here will be skipped during scans."), pathsTab));

    m_list = new QListWidget(pathsTab);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    pathsLay->addWidget(m_list);

    auto *pathActRow = new QHBoxLayout;
    m_btnAddFile     = new QPushButton(QIcon::fromTheme("list-add"),
                                      tr("File"), pathsTab);
    m_btnAddFolder   = new QPushButton(QIcon::fromTheme("folder-new"),
                                      tr("Folder"), pathsTab);
    m_btnRemove      = new QPushButton(QIcon::fromTheme("list-remove"),
                                      tr("Remove"), pathsTab);
    m_btnRemove->setEnabled(false);
    pathActRow->addWidget(m_btnAddFile);
    pathActRow->addWidget(m_btnAddFolder);
    pathActRow->addStretch();
    pathActRow->addWidget(m_btnRemove);
    pathsLay->addLayout(pathActRow);

    tabs->addTab(pathsTab, tr("Paths"));

    // ── Tab 1: Extensions ─────────────────────────────────────────────────
    auto *extTab = new QWidget(tabs);
    auto *extLay = new QVBoxLayout(extTab);
    extLay->setContentsMargins(8, 8, 8, 8);
    extLay->setSpacing(10);

    extLay->addWidget(new QLabel(
        tr("File extensions listed here will be skipped during scans\n"
           "and excluded from on-access (real-time) protection."), extTab));

    m_extList = new QListWidget(extTab);
    m_extList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    extLay->addWidget(m_extList);

    auto *extActRow  = new QHBoxLayout;
    m_btnAddExt      = new QPushButton(QIcon::fromTheme("list-add"),
                                       tr("Add Extension"), extTab);
    m_btnRemoveExt   = new QPushButton(QIcon::fromTheme("list-remove"),
                                       tr("Remove"), extTab);
    m_btnRemoveExt->setEnabled(false);
    extActRow->addWidget(m_btnAddExt);
    extActRow->addStretch();
    extActRow->addWidget(m_btnRemoveExt);
    extLay->addLayout(extActRow);

    tabs->addTab(extTab, tr("Extensions"));

    layout->addWidget(tabs);

    // ── Bottom row ────────────────────────────────────────────────────────
    auto *bottomRow  = new QHBoxLayout;
    m_btnBack        = new QPushButton(QIcon::fromTheme("go-previous"), tr("Back"), this);
    m_btnApplyToRT   = new QPushButton(QIcon::fromTheme("system-run"),
                                       tr("Apply to Real-Time Protection"), this);
    m_btnApplyToRT->setToolTip(tr("Writes the current exclusions to /etc/clamav/clamd.conf "
                                   "and restarts real-time protection services. "
                                   "You will be prompted for your password."));
    m_statusLabel = new QLabel(this);
    bottomRow->addWidget(m_btnBack);
    bottomRow->addWidget(m_statusLabel);
    bottomRow->addStretch();
    bottomRow->addWidget(m_btnApplyToRT);
    layout->addLayout(bottomRow);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_btnAddFile,    &QPushButton::clicked, this, &ExclusionsPage::onAddFile);
    connect(m_btnAddFolder,  &QPushButton::clicked, this, &ExclusionsPage::onAddFolder);
    connect(m_btnRemove,     &QPushButton::clicked, this, &ExclusionsPage::onRemoveSelected);
    connect(m_btnAddExt,     &QPushButton::clicked, this, &ExclusionsPage::onAddExtension);
    connect(m_btnRemoveExt,  &QPushButton::clicked, this, &ExclusionsPage::onRemoveExtension);
    connect(m_btnApplyToRT,  &QPushButton::clicked, this, &ExclusionsPage::onApplyToRT);
    connect(m_btnBack,       &QPushButton::clicked, this, &ExclusionsPage::backRequested);

    connect(m_cfgMgr, &ClamdConfigManager::configSaved,
            this, [this](bool success, const QString &msg) {
        m_btnApplyToRT->setEnabled(true);
        m_statusLabel->setText(msg);
        QPalette p = m_statusLabel->palette();
        p.setColor(QPalette::WindowText,
                   success ? QColor(0x2E, 0x7D, 0x32) : QColor(0xC6, 0x28, 0x28));
        m_statusLabel->setPalette(p);
    });

    connect(m_list, &QListWidget::itemSelectionChanged, this, [this]() {
        m_btnRemove->setEnabled(!m_list->selectedItems().isEmpty());
    });
    connect(m_extList, &QListWidget::itemSelectionChanged, this, [this]() {
        m_btnRemoveExt->setEnabled(!m_extList->selectedItems().isEmpty());
    });

    refresh();
}

void ExclusionsPage::refresh()
{
    // Paths tab
    m_list->clear();
    for (const QString &path : m_clam->exclusions())
        m_list->addItem(path);

    // Extensions tab
    m_extList->clear();
    for (const QString &ext : m_clam->excludedExtensions())
        m_extList->addItem(ext);
}

void ExclusionsPage::onAddFile()
{
    QString file = QFileDialog::getOpenFileName(
        this, tr("Select file to exclude"), QDir::homePath());
    if (!file.isEmpty()) { m_clam->addExclusion(file); refresh(); }
}

void ExclusionsPage::onAddFolder()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select folder to exclude"), QDir::homePath(),
        QFileDialog::ShowDirsOnly);
    if (!dir.isEmpty()) { m_clam->addExclusion(dir); refresh(); }
}

void ExclusionsPage::onRemoveSelected()
{
    for (QListWidgetItem *item : m_list->selectedItems())
        m_clam->removeExclusion(item->text());
    refresh();
}

void ExclusionsPage::onAddExtension()
{
    bool ok;
    QString ext = QInputDialog::getText(
        this, tr("Add Extension"),
        tr("Enter file extension to exclude (e.g. .crdownload):"),
        QLineEdit::Normal, ".", &ok);

    if (!ok || ext.trimmed().isEmpty()) return;

    // Normalize: ensure leading dot
    QString e = ext.trimmed();
    if (!e.startsWith('.'))
        e.prepend('.');

    m_clam->addExcludedExtension(e);
    refresh();
}

void ExclusionsPage::onRemoveExtension()
{
    for (QListWidgetItem *item : m_extList->selectedItems())
        m_clam->removeExcludedExtension(item->text());
    refresh();
}

void ExclusionsPage::onApplyToRT()
{
    m_btnApplyToRT->setEnabled(false);
    m_statusLabel->setText(tr("Saving…"));
    buildClamdConfig();
}

void ExclusionsPage::buildClamdConfig()
{
    ClamdOnAccessConfig cfg = m_cfgMgr->readConfig();

    // Directories → OnAccessExcludePath, files → ExcludePath (PCRE)
    QStringList excludeDirs;
    QStringList filePatterns;
    for (const QString &path : m_clam->exclusions()) {
        if (QFileInfo(path).isDir()) {
            excludeDirs << path;
        } else {
            filePatterns << ("^" + QRegularExpression::escape(path) + "$");
        }
    }
    cfg.onAccessExcludePaths = excludeDirs;

    // Extension patterns + individual file patterns
    for (const QString &ext : m_clam->excludedExtensions()) {
        QString escaped = ext;
        escaped.replace('.', "\\.");
        filePatterns << (escaped + "$");
    }
    cfg.excludeFilePatterns = filePatterns;

    m_cfgMgr->saveConfig(cfg);
}
