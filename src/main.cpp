#include "mainwindow.h"
#include "qvapplication.h"
#include "qvwin32functions.h"

#include <QCommandLineParser>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    QCoreApplication::setOrganizationName("qView");
    QCoreApplication::setApplicationName("qView-JDP");
    QCoreApplication::setApplicationVersion(QString::number(VERSION));

    SettingsManager::migrateOldSettings();

    QString defaultStyleName;
#if defined Q_OS_WIN && QT_VERSION >= QT_VERSION_CHECK(6, 8, 1)
    // windows11 style works on Windows 10 too if the right font is available
    if (QOperatingSystemVersion::current() < QOperatingSystemVersion::Windows11)
        defaultStyleName = "windows11";
#endif
    // Convenient way to set a default style but still allow the user to customize it
    if (!defaultStyleName.isEmpty() && qEnvironmentVariableIsEmpty("QT_STYLE_OVERRIDE"))
        qputenv("QT_STYLE_OVERRIDE", defaultStyleName.toLocal8Bit());

    QVApplication app(argc, argv);

#if defined Q_OS_WIN && QT_VERSION >= QT_VERSION_CHECK(6, 8, 1)
    // For windows11 style on Windows 10, make sure we have the font it needs, otherwise change style
    if (QOperatingSystemVersion::current() < QOperatingSystemVersion::Windows11 &&
        QApplication::style()->name() == "windows11" &&
        !QFontDatabase::families().contains("Segoe Fluent Icons"))
    {
        const QString fontPath = QDir(QApplication::applicationDirPath()).filePath("fonts/Segoe Fluent Icons.ttf");
        if (QFile::exists(fontPath))
            QFontDatabase::addApplicationFont(fontPath);
        else
            QApplication::setStyle("windowsvista");
    }
#endif

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
