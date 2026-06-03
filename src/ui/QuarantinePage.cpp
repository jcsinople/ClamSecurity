#include "QuarantinePage.h"
#include "QuarantineManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QMessageBox>
#include <QHeaderView>
#include <QLocale>
#include <QFileInfo>

QuarantinePage::QuarantinePage(QuarantineManager *quar, QWidget *parent)
    : QWidget(parent), m_quar(quar)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *title = new QLabel(tr("Cuarentena"), this);
    QFont f = title->font(); f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    m_countLabel = new QLabel(this);
    layout->addWidget(m_countLabel);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({
        tr("Nombre"), tr("Ruta original"), tr("Fecha"), tr("Amenaza"), tr("Tamaño")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    layout->addWidget(m_table);

    auto *actRow = new QHBoxLayout;
    m_btnBack      = new QPushButton(QIcon::fromTheme("go-previous"),  tr("Volver"), this);
    m_btnRestore   = new QPushButton(QIcon::fromTheme("edit-undo"),    tr("Restaurar"), this);
    m_btnDelete    = new QPushButton(QIcon::fromTheme("edit-delete"),  tr("Eliminar"), this);
    m_btnDeleteAll = new QPushButton(QIcon::fromTheme("edit-clear"),   tr("Eliminar Todo"), this);

    m_btnRestore->setEnabled(false);
    m_btnDelete->setEnabled(false);
    m_btnDeleteAll->setEnabled(false);

    actRow->addWidget(m_btnBack);
    actRow->addStretch();
    actRow->addWidget(m_btnRestore);
    actRow->addWidget(m_btnDelete);
    actRow->addWidget(m_btnDeleteAll);
    layout->addLayout(actRow);

    connect(m_btnBack,      &QPushButton::clicked, this, &QuarantinePage::backRequested);
    connect(m_btnRestore,   &QPushButton::clicked, this, &QuarantinePage::onRestore);
    connect(m_btnDelete,    &QPushButton::clicked, this, &QuarantinePage::onDelete);
    connect(m_btnDeleteAll, &QPushButton::clicked, this, &QuarantinePage::onDeleteAll);
    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &QuarantinePage::onSelectionChanged);
    connect(m_quar, &QuarantineManager::quarantineChanged,
            this, &QuarantinePage::refresh);

    refresh();
}

void QuarantinePage::refresh()
{
    m_table->setRowCount(0);
    const auto entries = m_quar->entries();
    m_countLabel->setText(tr("%1 archivo(s) en cuarentena.").arg(entries.size()));
    m_btnDeleteAll->setEnabled(!entries.isEmpty());

    QLocale locale;
    for (const QuarantineEntry &e : entries) {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        QFileInfo fi(e.originalPath);
        auto *nameItem = new QTableWidgetItem(fi.fileName());
        nameItem->setData(Qt::UserRole, e.id);
        m_table->setItem(row, 0, nameItem);
        m_table->setItem(row, 1, new QTableWidgetItem(fi.dir().path()));
        m_table->setItem(row, 2, new QTableWidgetItem(
            e.dateAdded.toString("dd/MM/yyyy HH:mm")));
        m_table->setItem(row, 3, new QTableWidgetItem(e.threat));
        m_table->setItem(row, 4, new QTableWidgetItem(
            locale.formattedDataSize(e.fileSize)));
    }
    onSelectionChanged();
}

void QuarantinePage::onRestore()
{
    int row = m_table->currentRow();
    if (row < 0) return;
    QString id = m_table->item(row, 0)->data(Qt::UserRole).toString();
    if (!m_quar->restoreFile(id))
        QMessageBox::warning(this, tr("Error"),
            tr("No se pudo restaurar. El destino original puede no existir."));
}

void QuarantinePage::onDelete()
{
    int row = m_table->currentRow();
    if (row < 0) return;
    if (QMessageBox::question(this, tr("Confirmar"),
            tr("¿Eliminar permanentemente este archivo?")) != QMessageBox::Yes) return;
    m_quar->deleteFile(m_table->item(row, 0)->data(Qt::UserRole).toString());
}

void QuarantinePage::onDeleteAll()
{
    if (QMessageBox::question(this, tr("Confirmar"),
            tr("¿Eliminar TODOS los archivos de cuarentena?")) != QMessageBox::Yes) return;
    m_quar->deleteAll();
}

void QuarantinePage::onSelectionChanged()
{
    bool sel = m_table->currentRow() >= 0;
    m_btnRestore->setEnabled(sel);
    m_btnDelete->setEnabled(sel);
}
