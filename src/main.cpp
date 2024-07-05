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
    QString styleName = QSettings().value("options/nonnativetheme").toBool() ? "fusion" : QString();
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    if (styleName.isEmpty())
        styleName = "windowsvista"; // Avoid windows11 style until it's less buggy
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
