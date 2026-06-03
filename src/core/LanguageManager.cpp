#include "LanguageManager.h"
#include <tuple>  // std::ignore
#include <QApplication>
#include <QLocale>
#include <QSettings>
#include <QLibraryInfo>

LanguageManager::LanguageManager(QApplication *app, QObject *parent)
    : QObject(parent), m_app(app)
{
    m_appTranslator = new QTranslator(this);
    m_qtTranslator  = new QTranslator(this);
}

QList<LanguageManager::Language> LanguageManager::available()
{
    return {
        {"en", "English"},
        {"es", "Español"},
    };
}

bool LanguageManager::apply(const QString &langCode)
{
    if (langCode == m_current) return false;

    // Remove old translators
    m_app->removeTranslator(m_appTranslator);
    m_app->removeTranslator(m_qtTranslator);

    if (langCode != "en") {
        // Load Qt's own translations (buttons like Ok/Cancel in dialogs)
        // Qt's own translator (Ok/Cancel buttons in dialogs); ignore if not found
        std::ignore = m_qtTranslator->load("qt_" + langCode,
            QLibraryInfo::path(QLibraryInfo::TranslationsPath));
        m_app->installTranslator(m_qtTranslator);

        // Load app translations (embedded in resources as /translations/ClamSecurity_xx.qm)
        if (m_appTranslator->load(":/translations/ClamSecurity_" + langCode)) {
            m_app->installTranslator(m_appTranslator);
        }
    }

    m_current = langCode;

    QSettings s;
    s.setValue("ui/language", langCode);

    emit languageChanged(langCode);
    return true;
}

void LanguageManager::applyStartup()
{
    QSettings s;
    QString saved = s.value("ui/language", QString()).toString();

    if (!saved.isEmpty()) {
        apply(saved);
        return;
    }

    // Detect system language (e.g. "es_MX" → try "es_MX", then "es")
    QString sysLocale = QLocale::system().name();          // "es_MX"
    QString sysLang   = sysLocale.section('_', 0, 0);     // "es"

    const auto langs = available();
    for (const Language &l : langs) {
        if (l.code == sysLocale || l.code == sysLang) {
            apply(l.code);
            return;
        }
    }
    // Fallback: English (no translation file needed)
    m_current = "en";
}
