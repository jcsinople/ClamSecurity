#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <QTranslator>

class QApplication;

class LanguageManager : public QObject
{
    Q_OBJECT
public:
    struct Language {
        QString code;   // "en", "es"
        QString name;   // "English", "Español"
    };

    explicit LanguageManager(QApplication *app, QObject *parent = nullptr);

    static QList<Language> available();
    QString current() const { return m_current; }

    // Load a language. "en" removes the translator (uses source strings).
    // Returns true if the language was changed (requires UI refresh).
    bool apply(const QString &langCode);

    // Load the language saved in QSettings, falling back to system locale,
    // then English. Call this once at startup before any UI is created.
    void applyStartup();

signals:
    void languageChanged(const QString &newCode);

private:
    QApplication *m_app;
    QTranslator  *m_appTranslator  = nullptr;
    QTranslator  *m_qtTranslator   = nullptr;
    QString       m_current        = "en";
};
