#include "mainwindow.h"
#include "qvapplication.h"
#include "qvwin32functions.h"

#include <QCommandLineParser>

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

#ifdef Q_OS_WIN
    bool useNonNativeTheme = QSettings().value("options/nonnativetheme").toBool();
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    useNonNativeTheme = false;
#elif QT_VERSION >= QT_VERSION_CHECK(6, 7, 3)
    if (useNonNativeTheme && QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows11)
        useNonNativeTheme = false; // Disable if we can use windows11 style instead
#endif
    QString styleName = useNonNativeTheme ? "fusion" : QString();
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0) && QT_VERSION <= QT_VERSION_CHECK(6, 7, 2)
    if (styleName.isEmpty())
        styleName = "windowsvista"; // windows11 style was buggy for a while after it was introduced
#endif
    // The docs recommend calling QApplication's static setStyle before its constructor, one reason
    // being that this allows styles to be set via command line arguments. Unfortunately it seems
    // that after running windeployqt, some styles such as windowsvista and windows11 aren't yet
    // loaded/available until the constructor runs. So we'll use this environment variable instead,
    // which Qt uses as a fallback override mechanism if a style wasn't specified via command line.
    if (!styleName.isEmpty())
        qputenv("QT_STYLE_OVERRIDE", styleName.toLocal8Bit());
#endif

    QVApplication app(argc, argv);

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
