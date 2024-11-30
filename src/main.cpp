#include "mainwindow.h"
#include "qvapplication.h"
#include "qvwin32functions.h"

#include <QCommandLineParser>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    QCoreApplication::setOrganizationName("qView");
    QCoreApplication::setApplicationName("qView-JDP");
    QCoreApplication::setApplicationVersion(QString::number(VERSION));

    SettingsManager::migrateOldSettings();

    QString defaultStyleName;
    QStringList fontsToInstall;

#ifdef Q_OS_WIN
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0) && QT_VERSION <= QT_VERSION_CHECK(6, 7, 2)
    defaultStyleName = "windowsvista"; // windows11 style was buggy for a while after it was introduced
#elif QT_VERSION >= QT_VERSION_CHECK(6, 8, 1)
    if (QOperatingSystemVersion::current() < QOperatingSystemVersion::Windows11)
    {
        // Qt's windows11 style can work on Windows 10, but it isn't enabled by default because the required
        // font isn't included with Windows 10. If we can locate and install the font, use windows11 style.
        const QString windows11StyleFontPath = QDir(QApplication::applicationDirPath()).filePath("fonts/Segoe Fluent Icons.ttf");
        if (QFile::exists(windows11StyleFontPath))
        {
            defaultStyleName = "windows11";
            fontsToInstall.append(windows11StyleFontPath);
        }
    }
#endif
#endif

    if (!defaultStyleName.isEmpty() && qEnvironmentVariableIsEmpty("QT_STYLE_OVERRIDE"))
    {
        // The docs recommend calling QApplication's static setStyle before its constructor, one reason
        // being that this allows styles to be set via command line arguments. Unfortunately it seems
        // that after running windeployqt, some styles such as windowsvista and windows11 aren't yet
        // loaded/available until the constructor runs. So we'll use this environment variable instead,
        // which Qt uses as a fallback override mechanism if a style wasn't specified via command line.
        qputenv("QT_STYLE_OVERRIDE", defaultStyleName.toLocal8Bit());
    }

    QVApplication app(argc, argv);

    for (const QString &font : fontsToInstall)
    {
        QFontDatabase::addApplicationFont(font);
    }

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QObject::tr("file"), QObject::tr("The file to open."));
#if defined Q_OS_WIN && WIN32_LOADED && QT_VERSION < QT_VERSION_CHECK(6, 7, 2)
    // Workaround for QTBUG-125380
    parser.process(QVWin32Functions::getCommandLineArgs());
#else
    parser.process(app);
#endif

    if (!parser.positionalArguments().isEmpty())
    {
        QVApplication::openFile(QVApplication::newWindow(), parser.positionalArguments().constFirst(), true);
    }
    else if (!QVApplication::tryRestoreLastSession())
    {
        QVApplication::newWindow();
    }

    return QApplication::exec();
}
