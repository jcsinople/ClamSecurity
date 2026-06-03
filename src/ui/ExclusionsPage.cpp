#include "ExclusionsPage.h"
#include "ClamAvManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QFileDialog>
#include <QDir>

ExclusionsPage::ExclusionsPage(ClamAvManager *clam, QWidget *parent)
    : QWidget(parent), m_clam(clam)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *title = new QLabel(tr("Scan Exclusions"), this);
    QFont f = title->font(); f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    layout->addWidget(new QLabel(
        tr("Paths listed here will be skipped during scans."), this));

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    layout->addWidget(m_list);

    auto *actRow   = new QHBoxLayout;
    m_btnAddFile   = new QPushButton(QIcon::fromTheme("list-add"),
                                     tr("+ File"), this);
    m_btnAddFolder = new QPushButton(QIcon::fromTheme("folder-new"),
                                     tr("+ Folder"), this);
    m_btnRemove    = new QPushButton(QIcon::fromTheme("list-remove"),
                                     tr("Remove"), this);
    m_btnRemove->setEnabled(false);
    actRow->addWidget(m_btnAddFile);
    actRow->addWidget(m_btnAddFolder);
    actRow->addStretch();
    actRow->addWidget(m_btnRemove);
    layout->addLayout(actRow);
    layout->addStretch();

    auto *navRow = new QHBoxLayout;
    m_btnBack = new QPushButton(QIcon::fromTheme("go-previous"), tr("Back"), this);
    navRow->addWidget(m_btnBack);
    navRow->addStretch();
    layout->addLayout(navRow);

    connect(m_btnAddFile,   &QPushButton::clicked, this, &ExclusionsPage::onAddFile);
    connect(m_btnAddFolder, &QPushButton::clicked, this, &ExclusionsPage::onAddFolder);
    connect(m_btnRemove,    &QPushButton::clicked, this, &ExclusionsPage::onRemoveSelected);
    connect(m_btnBack,      &QPushButton::clicked, this, &ExclusionsPage::backRequested);
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this]() {
        m_btnRemove->setEnabled(!m_list->selectedItems().isEmpty());
    });

    refresh();
}

void ExclusionsPage::refresh()
{
    m_list->clear();
    for (const QString &path : m_clam->exclusions())
        m_list->addItem(path);
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
