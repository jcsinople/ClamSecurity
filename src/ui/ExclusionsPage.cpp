#include "ExclusionsPage.h"
#include "ClamAvManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>

ExclusionsPage::ExclusionsPage(ClamAvManager *clam, QWidget *parent)
    : QWidget(parent), m_clam(clam)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *title = new QLabel(tr("Exclusiones de Escaneo"), this);
    QFont f = title->font(); f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    auto *hint = new QLabel(
        tr("Las rutas aquí listadas serán ignoradas durante el escaneo."), this);
    layout->addWidget(hint);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    layout->addWidget(m_list);

    // Action buttons (add/remove)
    auto *actRow = new QHBoxLayout;
    m_btnAddFile   = new QPushButton(QIcon::fromTheme("list-add"),
                                     tr("+ Archivo"), this);
    m_btnAddFolder = new QPushButton(QIcon::fromTheme("folder-new"),
                                     tr("+ Carpeta"), this);
    m_btnRemove    = new QPushButton(QIcon::fromTheme("list-remove"),
                                     tr("Eliminar"), this);
    m_btnRemove->setEnabled(false);

    actRow->addWidget(m_btnAddFile);
    actRow->addWidget(m_btnAddFolder);
    actRow->addStretch();
    actRow->addWidget(m_btnRemove);
    layout->addLayout(actRow);

    layout->addStretch();

    // Navigation
    auto *navRow = new QHBoxLayout;
    m_btnBack = new QPushButton(QIcon::fromTheme("go-previous"), tr("Volver"), this);
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
        this, tr("Seleccionar archivo a excluir"), QDir::homePath());
    if (!file.isEmpty()) {
        m_clam->addExclusion(file);
        refresh();
    }
}

void ExclusionsPage::onAddFolder()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Seleccionar carpeta a excluir"), QDir::homePath(),
        QFileDialog::ShowDirsOnly);
    if (!dir.isEmpty()) {
        m_clam->addExclusion(dir);
        refresh();
    }
}

void ExclusionsPage::onRemoveSelected()
{
    for (QListWidgetItem *item : m_list->selectedItems())
        m_clam->removeExclusion(item->text());
    refresh();
}
