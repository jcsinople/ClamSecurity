#include "QuarantineManager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QStandardPaths>
#include <QDateTime>

QuarantineManager::QuarantineManager(QObject *parent) : QObject(parent)
{
    m_dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
            + "/quarantine";
    m_indexPath = m_dir + "/index.json";
    ensureQuarantineDir();
    loadIndex();
}

QString QuarantineManager::quarantineDir() const { return m_dir; }

bool QuarantineManager::ensureQuarantineDir()
{
    return QDir().mkpath(m_dir);
}

bool QuarantineManager::quarantineFile(const QString &filePath, const QString &threat)
{
    QFileInfo fi(filePath);
    if (!fi.exists()) return false;

    QString id   = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString dest = m_dir + "/" + id + ".quar";

    encodeFile(filePath, dest);

    if (!QFile::exists(dest)) return false;
    QFile::remove(filePath);

    QuarantineEntry entry;
    entry.id            = id;
    entry.originalPath  = filePath;
    entry.quarantineName = id + ".quar";
    entry.dateAdded     = QDateTime::currentDateTime();
    entry.threat        = threat;
    entry.fileSize      = fi.size();

    m_entries.prepend(entry);
    saveIndex();
    emit quarantineChanged();
    return true;
}

bool QuarantineManager::restoreFile(const QString &id)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id == id) {
            QString src  = m_dir + "/" + m_entries[i].quarantineName;
            QString dst  = m_entries[i].originalPath;
            QDir().mkpath(QFileInfo(dst).dir().path());
            if (decodeFile(src, dst)) {
                QFile::remove(src);
                m_entries.removeAt(i);
                saveIndex();
                emit quarantineChanged();
                return true;
            }
            return false;
        }
    }
    return false;
}

bool QuarantineManager::deleteFile(const QString &id)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id == id) {
            QFile::remove(m_dir + "/" + m_entries[i].quarantineName);
            m_entries.removeAt(i);
            saveIndex();
            emit quarantineChanged();
            return true;
        }
    }
    return false;
}

void QuarantineManager::deleteAll()
{
    for (const QuarantineEntry &e : m_entries)
        QFile::remove(m_dir + "/" + e.quarantineName);
    m_entries.clear();
    saveIndex();
    emit quarantineChanged();
}

QList<QuarantineEntry> QuarantineManager::entries() const { return m_entries; }
int QuarantineManager::entryCount() const { return m_entries.size(); }

void QuarantineManager::loadIndex()
{
    m_entries.clear();
    QFile f(m_indexPath);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;
    QJsonArray arr = doc.object().value("entries").toArray();
    for (const QJsonValue &v : arr) {
        QJsonObject o = v.toObject();
        QuarantineEntry e;
        e.id             = o["id"].toString();
        e.originalPath   = o["originalPath"].toString();
        e.quarantineName = o["quarantineName"].toString();
        e.dateAdded      = QDateTime::fromString(o["dateAdded"].toString(), Qt::ISODate);
        e.threat         = o["threat"].toString();
        e.fileSize       = o["fileSize"].toVariant().toLongLong();
        m_entries << e;
    }
}

void QuarantineManager::saveIndex()
{
    QJsonArray arr;
    for (const QuarantineEntry &e : m_entries) {
        QJsonObject o;
        o["id"]            = e.id;
        o["originalPath"]  = e.originalPath;
        o["quarantineName"] = e.quarantineName;
        o["dateAdded"]     = e.dateAdded.toString(Qt::ISODate);
        o["threat"]        = e.threat;
        o["fileSize"]      = e.fileSize;
        arr.append(o);
    }
    QJsonObject root;
    root["entries"] = arr;
    QFile f(m_indexPath);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson());
    }
}

void QuarantineManager::encodeFile(const QString &src, const QString &dst)
{
    QFile in(src), out(dst);
    if (!in.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly))
        return;
    QByteArray data = in.readAll();
    for (char &b : data) b ^= 0xFF;
    out.write(data);
}

bool QuarantineManager::decodeFile(const QString &src, const QString &dst)
{
    QFile in(src), out(dst);
    if (!in.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly))
        return false;
    QByteArray data = in.readAll();
    for (char &b : data) b ^= 0xFF;
    out.write(data);
    return true;
}
