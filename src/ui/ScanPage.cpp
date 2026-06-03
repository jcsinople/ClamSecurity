#include "ScanPage.h"
#include "ClamAvManager.h"
#include "QuarantineManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFont>

ScanPage::ScanPage(ClamAvManager *clam,
                   QuarantineManager *quar,
                   QWidget *parent)
    : QWidget(parent), m_clam(clam), m_quar(quar)
{
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 20, 20, 20);

    // Title
    auto *title = new QLabel(tr("Escanear"), this);
    QFont f = title->font(); f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    // Status label
    m_statusLabel = new QLabel(tr("Listo para escanear."), this);
    layout->addWidget(m_statusLabel);

    // Progress area
    auto *progGroup = new QGroupBox(tr("Progreso"), this);
    auto *progLayout = new QVBoxLayout(progGroup);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);
    progLayout->addWidget(m_progress);

    m_currentFileLabel = new QLabel(this);
    m_currentFileLabel->setVisible(false);
    QFont smallF = m_currentFileLabel->font(); smallF.setPointSize(8);
    m_currentFileLabel->setFont(smallF);
    progLayout->addWidget(m_currentFileLabel);

    m_statsLabel = new QLabel(tr("Archivos: 0  |  Amenazas: 0"), this);
    progLayout->addWidget(m_statsLabel);

    layout->addWidget(progGroup);

    // Threat list
    auto *threatGroup = new QGroupBox(tr("Resultados"), this);
    auto *threatLayout = new QVBoxLayout(threatGroup);

    m_threatList = new QListWidget(this);
    m_threatList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    threatLayout->addWidget(m_threatList);

    m_btnQuarantine = new QPushButton(tr("Poner en Cuarentena"), this);
    m_btnQuarantine->setEnabled(false);
    threatLayout->addWidget(m_btnQuarantine);

    layout->addWidget(threatGroup);

    // Buttons
    auto *btnLayout = new QHBoxLayout;
    m_btnHome   = new QPushButton(QIcon::fromTheme("go-home"),
                                  tr("Escanear HOME"), this);
    m_btnCustom = new QPushButton(QIcon::fromTheme("document-open"),
                                  tr("Escaneo Personalizado"), this);
    m_btnStop   = new QPushButton(QIcon::fromTheme("process-stop"),
                                  tr("Detener"), this);
    m_btnBack   = new QPushButton(QIcon::fromTheme("go-previous"),
                                  tr("Volver"), this);
    m_btnStop->setEnabled(false);

    btnLayout->addWidget(m_btnBack);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnHome);
    btnLayout->addWidget(m_btnCustom);
    btnLayout->addWidget(m_btnStop);
    layout->addLayout(btnLayout);

    connect(m_btnHome,       &QPushButton::clicked, this, &ScanPage::onScanHome);
    connect(m_btnCustom,     &QPushButton::clicked, this, &ScanPage::onScanCustom);
    connect(m_btnStop,       &QPushButton::clicked, this, &ScanPage::onStopScan);
    connect(m_btnBack,       &QPushButton::clicked, this, &ScanPage::backRequested);
    connect(m_btnQuarantine, &QPushButton::clicked, this, &ScanPage::onQuarantineSelected);
    connect(m_threatList, &QListWidget::itemSelectionChanged,
            this, [this]() {
        m_btnQuarantine->setEnabled(!m_threatList->selectedItems().isEmpty());
    });
}

void ScanPage::startScan(const QString &path)
{
    if (m_thread) return; // already scanning

    clearResults();
    setScanningState(true);
    m_statusLabel->setText(tr("Escaneando: %1").arg(path));

    m_thread = new QThread(this);
    m_worker = new ScanWorker(path, m_clam->exclusions());
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started,       m_worker, &ScanWorker::startScan);
    connect(m_worker, &ScanWorker::progressUpdate,
            this, &ScanPage::onProgress);
    connect(m_worker, &ScanWorker::threatFound,
            this, &ScanPage::onThreatFound);
    connect(m_worker, &ScanWorker::scanFinished,
            this, &ScanPage::onScanFinished);
    connect(m_worker, &ScanWorker::scanError,
            this, [this](const QString &msg) {
        QMessageBox::warning(this, tr("Error de escaneo"), msg);
    });
    connect(m_worker, &ScanWorker::scanFinished,
            m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished,
            m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished,
            m_thread, &QObject::deleteLater);

    m_thread->start();
}

void ScanPage::onScanHome()
{
    startScan(QDir::homePath());
}

void ScanPage::onScanCustom()
{
    QString path = QFileDialog::getExistingDirectory(
        this,
        tr("Seleccionar directorio o archivo"),
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    if (!path.isEmpty())
        startScan(path);
}

void ScanPage::onStopScan()
{
    if (m_worker) m_worker->stopScan();
}

void ScanPage::onProgress(int count, const QString &file)
{
    m_statsLabel->setText(tr("Archivos: %1  |  Amenazas: %2")
                          .arg(count)
                          .arg(m_threatList->count()));
    m_currentFileLabel->setText(file);
}

void ScanPage::onThreatFound(const QString &file, const QString &threat)
{
    auto *item = new QListWidgetItem(
        QIcon::fromTheme("security-low"),
        tr("%1  →  %2").arg(file, threat),
        m_threatList
    );
    item->setData(Qt::UserRole, file);
    item->setData(Qt::UserRole + 1, threat);
    m_threatList->scrollToBottom();
}

void ScanPage::onScanFinished(int scanned, int infected, bool cancelled)
{
    setScanningState(false);
    m_thread = nullptr;
    m_worker = nullptr;

    if (cancelled) {
        m_statusLabel->setText(tr("Escaneo detenido por el usuario."));
    } else if (infected == 0) {
        m_statusLabel->setText(tr("Escaneo completado. No se encontraron amenazas."));
        m_threatList->addItem(
            new QListWidgetItem(QIcon::fromTheme("security-high"),
                                tr("Sin amenazas detectadas")));
    } else {
        m_statusLabel->setText(
            tr("Escaneo completado. Se encontraron %1 amenaza(s).").arg(infected));
    }
    m_statsLabel->setText(
        tr("Archivos escaneados: %1  |  Amenazas: %2").arg(scanned).arg(infected));
}

void ScanPage::onQuarantineSelected()
{
    for (QListWidgetItem *item : m_threatList->selectedItems()) {
        QString file   = item->data(Qt::UserRole).toString();
        QString threat = item->data(Qt::UserRole + 1).toString();
        if (m_quar->quarantineFile(file, threat)) {
            item->setIcon(QIcon::fromTheme("edit-delete"));
            item->setText(item->text() + tr("  [EN CUARENTENA]"));
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        }
    }
}

void ScanPage::setScanningState(bool scanning)
{
    m_progress->setVisible(scanning);
    m_currentFileLabel->setVisible(scanning);
    m_btnHome->setEnabled(!scanning);
    m_btnCustom->setEnabled(!scanning);
    m_btnStop->setEnabled(scanning);
}

void ScanPage::clearResults()
{
    m_threatList->clear();
    m_statsLabel->setText(tr("Archivos: 0  |  Amenazas: 0"));
    m_currentFileLabel->clear();
}
