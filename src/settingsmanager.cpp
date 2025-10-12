#include "settingsmanager.h"
#include "qvnamespace.h"

#include <QSettings>
#include <QTranslator>
#include <QLocale>
#include <QCoreApplication>
#include <QDir>

#include <QDebug>

SettingsManager::SettingsManager(QObject *parent) : QObject(parent)
{
    initializeSettingsLibrary();
    loadSettings();
    loadTranslation();
}

QString SettingsManager::getSystemLanguage() const
{
    auto entries = QDir(":/i18n/").entryList();
    entries.prepend("qview_en.ts");
    const auto centries = entries;

    const auto languages = QLocale::system().uiLanguages();
    for (auto language : languages)
    {
        language.replace('-', '_');
        const auto countryless = language.left(2);

        for (auto entry : centries)
        {
            entry.remove(0, 6);
            entry.remove(entry.length()-3, 3);

            if (entry == language)
                return language;

            if (entry == countryless)
                return countryless;
        }
    }
    return "en";
}

bool SettingsManager::loadTranslation() const
{
    QString lang = getString("language");
    if (lang == "system")
        lang = getSystemLanguage();

    if (lang == "en")
        return true;

    QTranslator *translator = new QTranslator();
    bool success = translator->load("qview_" + lang + ".qm", QLatin1String(":/i18n"));
    if (success)
    {
        qInfo() << "Loaded translation" << lang;
        QCoreApplication::installTranslator(translator);
    }
    return success;
}

void SettingsManager::loadSettings()
{
    QSettings settings;
    settings.beginGroup("options");
    bool changed = false;

    const auto keys = settingsLibrary.keys();
    for (const auto &key : keys)
    {
        auto &setting = settingsLibrary[key];
        if (setting.value != settings.value(key, setting.defaultValue))
            changed = true;

        setting.value = settings.value(key, setting.defaultValue);
    }

    if (changed)
        emit settingsUpdated();
}

const QVariant SettingsManager::getSetting(const QString &key, bool defaults) const
{
    auto value = settingsLibrary.value(key);

    if (!defaults && !value.value.isNull())
        return value.value;

    if (!value.defaultValue.isNull())
        return value.defaultValue;

    qWarning() << "Error: Invalid settings key: " + key;
    return QVariant();
}

bool SettingsManager::getBoolean(const QString &key, bool defaults) const
{
    auto value = getSetting(key, defaults);

    if (value.canConvert<bool>())
        return value.value<bool>();

    qWarning() << "Error: Can't convert setting key " + key + " to bool";
    return false;
}

int SettingsManager::getInteger(const QString &key, bool defaults) const
{
    auto value = getSetting(key, defaults);

    if (value.canConvert<int>())
        return value.value<int>();

    qWarning() << "Error: Can't convert setting key " + key + " to int";
    return 0;
}

double SettingsManager::getDouble(const QString &key, bool defaults) const
{
    auto value = getSetting(key, defaults);

    if (value.canConvert<double>())
        return value.value<double>();

    qWarning() << "Error: Can't convert setting key " + key + " to double";
    return 0;
}

const QString SettingsManager::getString(const QString &key, bool defaults) const
{
    auto value = getSetting(key, defaults);

    if (value.canConvert<QString>())
        return value.value<QString>();

    qWarning() << "Error: Can't convert setting key " + key + " to string";
    return "";
}

bool SettingsManager::isDefault(const QString &key) const
{
    return getSetting(key) == getSetting(key, true);
}

void SettingsManager::migrateOldSettings()
{
    if (!QSettings().contains("firstlaunch"))
        copyFromOfficial();

    QSettings settings;
    settings.beginGroup("options");

    if (!settings.contains("smoothscalingmode") && settings.contains("filteringenabled"))
    {
        const auto value =
            settings.value("scalingenabled").toBool() ? Qv::SmoothScalingMode::Expensive :
            settings.value("filteringenabled").toBool() ? Qv::SmoothScalingMode::Bilinear :
            Qv::SmoothScalingMode::Disabled;
        settings.setValue("smoothscalingmode", static_cast<int>(value));
    }
}

void SettingsManager::copyFromOfficial()
{
    const QSet<QString> keysToSkip = []()
    {
#ifdef Q_OS_MACOS
        QList<QString> systemDefaultKeys = QSettings{"qView", "NonExistent"}.allKeys();
        return QSet<QString>{systemDefaultKeys.begin(), systemDefaultKeys.end()};
#else
        return QSet<QString>();
#endif
    }();
    QSettings src{"qView", "qView"};
    QSettings dst{};

    for (const QString &key : src.allKeys())
    {
        if (keysToSkip.contains(key)) continue;
        dst.setValue(key, src.value(key));
    }
}

void SettingsManager::initializeSettingsLibrary()
{
    // Window
    settingsLibrary.insert("bgcolorenabled", {true, {}});
    settingsLibrary.insert("bgcolor", {"#212121", {}});
    settingsLibrary.insert("checkerboardbackground", {false, {}});
    settingsLibrary.insert("titlebarmode", {static_cast<int>(Qv::TitleBarText::Minimal), {}});
    settingsLibrary.insert("customtitlebartext", {"%z - %n", {}});
    settingsLibrary.insert("windowresizemode", {static_cast<int>(Qv::WindowResizeMode::WhenLaunching), {}});
    settingsLibrary.insert("aftermatchingsizemode", {static_cast<int>(Qv::AfterMatchingSize::CenterOnPrevious), {}});
    settingsLibrary.insert("minwindowresizedpercentage", {20, {}});
    settingsLibrary.insert("maxwindowresizedpercentage", {70, {}});
    settingsLibrary.insert("titlebaralwaysdark", {false, {}});
    settingsLibrary.insert("quitonlastwindow", {false, {}});
    settingsLibrary.insert("menubarenabled", {false, {}});
    settingsLibrary.insert("fullscreendetails", {false, {}});
#if defined Q_OS_MACOS && QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
    settingsLibrary.insert("mainmenuicons", {false, {}});
    settingsLibrary.insert("contextmenuicons", {false, {}});
#else
    settingsLibrary.insert("mainmenuicons", {true, {}});
    settingsLibrary.insert("contextmenuicons", {true, {}});
#endif
    settingsLibrary.insert("submenuicons", {true, {}});
    settingsLibrary.insert("slideshowkeepswindowontop", {false, {}});
    settingsLibrary.insert("reusewindow", {false, {}});
    settingsLibrary.insert("persistsession", {false, {}});
    // Image
    settingsLibrary.insert("smoothscalingmode", {static_cast<int>(Qv::SmoothScalingMode::Expensive), {}});
    settingsLibrary.insert("scalingtwoenabled", {true, {}});
    settingsLibrary.insert("smoothscalinglimitenabled", {false, {}});
    settingsLibrary.insert("smoothscalinglimitpercent", {400, {}});
    settingsLibrary.insert("scalefactor", {25, {}});
    settingsLibrary.insert("cursorzoom", {true, {}});
    settingsLibrary.insert("navresetszoom", {true, {}});
#ifdef Q_OS_MACOS
    // Usually not desired due to the way macOS does DPI scaling
    settingsLibrary.insert("onetoonepixelsizing", {false, {}});
#else
    settingsLibrary.insert("onetoonepixelsizing", {true, {}});
#endif
    settingsLibrary.insert("calculatedzoommode", {static_cast<int>(Qv::CalculatedZoomMode::ZoomToFit), {}});
    settingsLibrary.insert("fitzoomlimitenabled", {false, {}});
    settingsLibrary.insert("fitzoomlimitpercent", {100, {}});
    settingsLibrary.insert("fitoverscan", {0, {}});
    settingsLibrary.insert("constrainimageposition", {true, {}});
    settingsLibrary.insert("constraincentersmallimage", {true, {}});
    settingsLibrary.insert("disabledelayedconstraint", {false, {}});
    settingsLibrary.insert("originalsizeastoggle", {false, {}});
    settingsLibrary.insert("colorspaceconversion", {static_cast<int>(Qv::ColorSpaceConversion::AutoDetect), {}});
    // Miscellaneous
    settingsLibrary.insert("language", {"system", {}});
    settingsLibrary.insert("sortmode", {static_cast<int>(Qv::SortMode::Name), {}});
    settingsLibrary.insert("sortdescending", {false, {}});
    settingsLibrary.insert("preloadingmode", {static_cast<int>(Qv::PreloadMode::Adjacent), {}});
    settingsLibrary.insert("navspeed", {50, {}});
    settingsLibrary.insert("loopfoldersenabled", {true, {}});
    settingsLibrary.insert("slideshowdirection", {static_cast<int>(Qv::SlideshowDirection::Forward), {}});
    settingsLibrary.insert("slideshowtimer", {5, {}});
    settingsLibrary.insert("afterdelete", {static_cast<int>(Qv::AfterDelete::MoveForward), {}});
    settingsLibrary.insert("askdelete", {true, {}});
    settingsLibrary.insert("allowmimecontentdetection", {false, {}});
    settingsLibrary.insert("skiphidden", {false, {}});
    settingsLibrary.insert("saverecents", {true, {}});
    settingsLibrary.insert("updatenotifications", {false, {}});
    // Formats
    settingsLibrary.insert("disabledfileextensions", {"", {}});
    // Mouse
    settingsLibrary.insert("navigationregionsenabled", {false, {}});
    settingsLibrary.insert("viewportdoubleclickaction", {static_cast<int>(Qv::ViewportClickAction::ToggleFullScreen), {}});
    settingsLibrary.insert("viewportaltdoubleclickaction", {static_cast<int>(Qv::ViewportClickAction::ToggleTitlebarHidden), {}});
    settingsLibrary.insert("viewportdragaction", {static_cast<int>(Qv::ViewportDragAction::Pan), {}});
    settingsLibrary.insert("viewportaltdragaction", {static_cast<int>(Qv::ViewportDragAction::MoveWindow), {}});
    settingsLibrary.insert("viewportmiddlebuttonmode", {static_cast<int>(Qv::ClickOrDrag::Click), {}});
    settingsLibrary.insert("viewportmiddleclickaction", {static_cast<int>(Qv::ViewportClickAction::ZoomToFit), {}});
    settingsLibrary.insert("viewportaltmiddleclickaction", {static_cast<int>(Qv::ViewportClickAction::OriginalSize), {}});
    settingsLibrary.insert("viewportmiddledragaction", {static_cast<int>(Qv::ViewportDragAction::Pan), {}});
    settingsLibrary.insert("viewportaltmiddledragaction", {static_cast<int>(Qv::ViewportDragAction::MoveWindow), {}});
    settingsLibrary.insert("viewportverticalscrollaction", {static_cast<int>(Qv::ViewportScrollAction::Zoom), {}});
    settingsLibrary.insert("viewporthorizontalscrollaction", {static_cast<int>(Qv::ViewportScrollAction::Navigate), {}});
    settingsLibrary.insert("viewportaltverticalscrollaction", {static_cast<int>(Qv::ViewportScrollAction::Pan), {}});
    settingsLibrary.insert("viewportalthorizontalscrollaction", {static_cast<int>(Qv::ViewportScrollAction::Pan), {}});
#ifdef Q_OS_MACOS
    // Works best with touchpads that accurately report ScrollPhase (macOS only currently)
    settingsLibrary.insert("scrollactioncooldown", {true, {}});
#else
    settingsLibrary.insert("scrollactioncooldown", {false, {}});
#endif
    settingsLibrary.insert("cursorautohidefullscreenenabled", {true, {}});
    settingsLibrary.insert("cursorautohidefullscreendelay", {2, {}});
}
