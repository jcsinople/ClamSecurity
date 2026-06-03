#pragma once
#include <QObject>
#include <QDateTime>
#include <QList>

struct QuarantineEntry {
    QString id;
    QString originalPath;
    QString quarantineName;
    QDateTime dateAdded;
    QString threat;
    qint64 fileSize = 0;
};

class QuarantineManager : public QObject
{
    Q_OBJECT
public:
    explicit QuarantineManager(QObject *parent = nullptr);

    QString quarantineDir() const;
    bool ensureQuarantineDir();

    bool quarantineFile(const QString &filePath, const QString &threat);
    bool restoreFile(const QString &id);
    bool deleteFile(const QString &id);
    void deleteAll();

    QList<QuarantineEntry> entries() const;
    int entryCount() const;

signals:
    void quarantineChanged();

private:
    void loadIndex();
    void saveIndex();
    void encodeFile(const QString &src, const QString &dst);
    bool decodeFile(const QString &src, const QString &dst);

    QList<QuarantineEntry> m_entries;
    QString m_dir;
    QString m_indexPath;
};
