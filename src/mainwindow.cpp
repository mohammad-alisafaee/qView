#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qvapplication.h"
#include "qvcocoafunctions.h"
#include "qvwin32functions.h"
#include "qvrenamedialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QString>
#include <QGraphicsPixmapItem>
#include <QPixmap>
#include <QClipboard>
#include <QCoreApplication>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QDesktopServices>
#include <QContextMenuEvent>
#include <QMovie>
#include <QImageWriter>
#include <QSettings>
#include <QStyle>
#include <QIcon>
#include <QMimeDatabase>
#include <QScreen>
#include <QCursor>
#include <QInputDialog>
#include <QProgressDialog>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QMenu>
#include <QWindow>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTemporaryFile>

MainWindow::MainWindow(QWidget *parent, const QJsonObject &windowSessionState) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_OpaquePaintEvent);

#ifdef COCOA_LOADED
    // Allow the titlebar to overlap widgets with full size content view
    setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
    centralWidget()->setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
#endif

    sessionStateToLoad = windowSessionState;
    lastActivated.start();

    // Initialize graphicsviewkDefaultBufferAlignment
    graphicsView = new QVGraphicsView(this);
    centralWidget()->layout()->addWidget(graphicsView);

    // Hide fullscreen label by default
    ui->fullscreenLabel->hide();

    // Connect graphicsview signals
    connect(graphicsView, &QVGraphicsView::fileChanged, this, &MainWindow::fileChanged);
    connect(graphicsView, &QVGraphicsView::zoomLevelChanged, this, &MainWindow::zoomLevelChanged);
    connect(graphicsView, &QVGraphicsView::calculatedZoomModeChanged, this, &MainWindow::syncCalculatedZoomMode);
    connect(graphicsView, &QVGraphicsView::navigationResetsZoomChanged, this, &MainWindow::syncNavigationResetsZoom);
    connect(graphicsView, &QVGraphicsView::cancelSlideshow, this, &MainWindow::cancelSlideshow);

    // Initialize escape shortcut
    escShortcut = new QShortcut(Qt::Key_Escape, this);
    connect(escShortcut, &QShortcut::activated, this, [this](){
        if (windowState().testFlag(Qt::WindowFullScreen))
            toggleFullScreen();
    });

    // Enable drag&dropping
    setAcceptDrops(true);

    // Make info dialog object
    info = new QVInfoDialog(this);

    // Timer for slideshow
    slideshowTimer = new QTimer(this);
    connect(slideshowTimer, &QTimer::timeout, this, &MainWindow::slideshowAction);

    // Timer for updating titlebar after zoom change
    zoomTitlebarUpdateTimer = new QTimer(this);
    zoomTitlebarUpdateTimer->setSingleShot(true);
    zoomTitlebarUpdateTimer->setInterval(50);
    connect(zoomTitlebarUpdateTimer, &QTimer::timeout, this, &MainWindow::buildWindowTitle);

    // Context menu
    auto &actionManager = qvApp->getActionManager();

    contextMenu = new QMenu(this);

    actionManager.addCloneOfAction(contextMenu, "open");
    actionManager.addCloneOfAction(contextMenu, "openurl");
    contextMenu->addMenu(actionManager.buildRecentsMenu(true, contextMenu));
    contextMenu->addMenu(actionManager.buildOpenWithMenu(contextMenu));
    actionManager.addCloneOfAction(contextMenu, "opencontainingfolder");
    actionManager.addCloneOfAction(contextMenu, "showfileinfo");
    contextMenu->addSeparator();
    actionManager.addCloneOfAction(contextMenu, "rename");
    actionManager.addCloneOfAction(contextMenu, "delete");
    contextMenu->addSeparator();
    actionManager.addCloneOfAction(contextMenu, "nextfile");
    actionManager.addCloneOfAction(contextMenu, "previousfile");
    contextMenu->addSeparator();
    contextMenu->addMenu(actionManager.buildViewMenu(true, contextMenu));
    contextMenu->addMenu(actionManager.buildToolsMenu(true, contextMenu));
    contextMenu->addMenu(actionManager.buildHelpMenu(true, contextMenu));

    connect(contextMenu, &QMenu::triggered, this, [this](QAction *triggeredAction){
        ActionManager::actionTriggered(triggeredAction, this);
    });

    // Initialize menubar
    setMenuBar(actionManager.buildMenuBar(this));
    // Stop actions conflicting with the window's actions
    const auto menubarActions = ActionManager::getAllNestedActions(menuBar()->actions());
    for (auto action : menubarActions)
    {
        action->setShortcutContext(Qt::WidgetShortcut);
    }
    connect(menuBar(), &QMenuBar::triggered, this, [this](QAction *triggeredAction){
        ActionManager::actionTriggered(triggeredAction, this);
    });

    // Add all actions to this window so keyboard shortcuts are always triggered
    // using virtual menu to hold them so i can connect to the triggered signal
    virtualMenu = new QMenu(this);
    const auto &actionKeys = actionManager.getActionLibrary().keys();
    for (const QString &key : actionKeys)
    {
        actionManager.addCloneOfAction(virtualMenu, key);
    }
    addActions(virtualMenu->actions());
    connect(virtualMenu, &QMenu::triggered, this, [this](QAction *triggeredAction){
       ActionManager::actionTriggered(triggeredAction, this);
    });

    // Enable actions related to having a window
    disableActions();

    // Connect functions to application components
    connect(&qvApp->getShortcutManager(), &ShortcutManager::shortcutsUpdated, this, &MainWindow::shortcutsUpdated);
    connect(&qvApp->getSettingsManager(), &SettingsManager::settingsUpdated, this, &MainWindow::settingsUpdated);
    settingsUpdated();
    shortcutsUpdated();

    // Timer for delayed-load Open With menu
    populateOpenWithTimer = new QTimer(this);
    populateOpenWithTimer->setSingleShot(true);
    populateOpenWithTimer->setInterval(250);
    connect(populateOpenWithTimer, &QTimer::timeout, this, &MainWindow::requestPopulateOpenWithMenu);

    // Connection for open with menu population futurewatcher
    connect(&openWithFutureWatcher, &QFutureWatcher<QList<OpenWith::OpenWithItem>>::finished, this, [this](){
        populateOpenWithMenu(openWithFutureWatcher.result());
    });

    QSettings settings;

    if (!sessionStateToLoad.isEmpty())
    {
        loadSessionState(sessionStateToLoad, true);
    }
    else
    {
        // Load window geometry
        restoreGeometry(settings.value("geometry").toByteArray());
    }

    // Show welcome dialog on first launch
    if (!settings.value("firstlaunch", false).toBool())
    {
        settings.setValue("firstlaunch", true);
        settings.setValue("configversion", VERSION);
        qvApp->openWelcomeDialog(this);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::event(QEvent *event)
{
    if (event->type() == QEvent::WindowActivate && !qvApp->getIsApplicationQuitting())
    {
        lastActivated.start();
    }
    return QMainWindow::event(event);
}

void MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMainWindow::contextMenuEvent(event);

    // Show native menu on macOS with cocoa framework loaded
#ifdef COCOA_LOADED
    // On regular context menu, recents submenu updates right before it is shown.
    // The native cocoa menu does not update elements until the entire menu is reopened, so we update first
    qvApp->getActionManager().loadRecentsList();
    QVCocoaFunctions::showMenu(contextMenu, event->pos(), windowHandle());
#else
    contextMenu->popup(event->globalPos());
#endif
}

void MainWindow::showEvent(QShowEvent *event)
{
#ifdef COCOA_LOADED
    QTimer::singleShot(0, this, [this]() {
        QVCocoaFunctions::setFullSizeContentView(this, true);
    });
#endif

    if (!menuBar()->sizeHint().isEmpty())
    {
        ui->fullscreenLabel->setMargin(0);
        ui->fullscreenLabel->setMinimumHeight(menuBar()->sizeHint().height());
    }

    syncCalculatedZoomMode();
    syncNavigationResetsZoom();

    if (!sessionStateToLoad.isEmpty())
    {
        QTimer::singleShot(0, this, [this]() {
            loadSessionState(sessionStateToLoad, false);
            sessionStateToLoad = {};
        });
    }

    qvApp->addToActiveWindows(this);

    QMainWindow::showEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    isClosing = true;

#ifdef COCOA_LOADED
    QVCocoaFunctions::setFullSizeContentView(this, false);
#endif

    if (qvApp->isSessionStateSaveRequested())
        qvApp->addClosedWindowSessionState(getSessionState(), getLastActivatedTimestamp());

    QSettings settings;
    settings.setValue("geometry", saveGeometry());

    qvApp->deleteFromActiveWindows(this);
    qvApp->getActionManager().untrackClonedActions(contextMenu);
    qvApp->getActionManager().untrackClonedActions(menuBar());
    qvApp->getActionManager().untrackClonedActions(virtualMenu);

    QMainWindow::closeEvent(event);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange)
    {
        const auto *changeEvent = static_cast<QWindowStateChangeEvent*>(event);
        if (windowState().testFlag(Qt::WindowFullScreen) != changeEvent->oldState().testFlag(Qt::WindowFullScreen))
            fullscreenChanged();
    }

    QMainWindow::changeEvent(event);
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    const ViewportPosition viewportPos = getViewportPosition();
    const int adjustedViewportY = viewportPos.widgetY + viewportPos.obscuredHeight;
    const QRect headerRect = QRect(0, 0, width(), adjustedViewportY);
    const QRect viewportRect = rect().adjusted(0, adjustedViewportY, 0, 0);

    if (headerRect.isValid())
    {
        painter.eraseRect(headerRect);
    }

    if (viewportRect.isValid())
    {
        if (checkerboardBackground && getIsPixmapLoaded())
        {
            const int gridSize = 16;
            const QColor darkColor = QColorConstants::DarkGray;
            const QColor lightColor = QColorConstants::LightGray;
            const int numHorizontalSquares = (viewportRect.width() + (gridSize - 1)) / gridSize;
            const int numVerticalSquares = (viewportRect.height() + (gridSize - 1)) / gridSize;
            for (int iY = 0; iY < numVerticalSquares; iY++)
            {
                for (int iX = 0; iX < numHorizontalSquares; iX++)
                {
                    const bool isDarkSquare = (iX % 2) != (iY % 2);
                    painter.fillRect(
                        viewportRect.x() + (iX * gridSize),
                        viewportRect.y() + (iY * gridSize),
                        gridSize,
                        gridSize,
                        isDarkSquare ? darkColor : lightColor
                    );
                }
            }
        }
        else
        {
            const QColor &backgroundColor = customBackgroundColor.isValid() ? customBackgroundColor : painter.background().color();
            painter.fillRect(viewportRect, backgroundColor);

            if (getCurrentFileDetails().errorData.has_value())
            {
                const QVImageCore::ErrorData &errorData = getCurrentFileDetails().errorData.value();
                const QString errorMessage = tr("Error occurred opening\n%3\n%2 (Error %1)").arg(QString::number(errorData.errorNum), errorData.errorString, getCurrentFileDetails().fileInfo.fileName());
                painter.setFont(font());
                painter.setPen(Qv::getPerceivedBrightness(backgroundColor) > 0.5 ? QColorConstants::Black : QColorConstants::White);
                painter.drawText(viewportRect, errorMessage, QTextOption(Qt::AlignCenter));
            }
        }
    }
}

void MainWindow::fullscreenChanged()
{
    const bool isFullscreen = windowState().testFlag(Qt::WindowFullScreen);

    const auto fullscreenActions = qvApp->getActionManager().getAllClonesOfAction("fullscreen", this);
    for (const auto &fullscreenAction : fullscreenActions)
    {
        fullscreenAction->setText(isFullscreen ? tr("Exit F&ull Screen") : tr("Enter F&ull Screen"));
        fullscreenAction->setIcon(isFullscreen ? QIcon::fromTheme("view-restore") : QIcon::fromTheme("view-fullscreen"));
    }

    ui->fullscreenLabel->setVisible(isFullscreen && qvApp->getSettingsManager().getBoolean("fullscreendetails"));

    if (!isFullscreen && storedTitlebarHidden)
    {
        setTitlebarHidden(true);
        storedTitlebarHidden = false;
    }

    updateMenuBarVisible();

    graphicsView->setCursorVisible(true);
}

void MainWindow::openFile(const QString &fileName)
{
    graphicsView->loadFile(fileName);
    cancelSlideshow();
}

void MainWindow::settingsUpdated()
{
    auto &settingsManager = qvApp->getSettingsManager();

    buildWindowTitle();

    //bgcolor
    customBackgroundColor = settingsManager.getBoolean("bgcolorenabled") ? QColor(settingsManager.getString("bgcolor")) : QColor();

    //checkerboardbackground
    checkerboardBackground = settingsManager.getBoolean("checkerboardbackground");

    // menubarenabled
    menuBarEnabled = settingsManager.getBoolean("menubarenabled");

#ifdef COCOA_LOADED
    // titlebaralwaysdark
    QVCocoaFunctions::setVibrancy(settingsManager.getBoolean("titlebaralwaysdark"), windowHandle());
#endif

    //slideshow timer
    slideshowTimer->setInterval(static_cast<int>(settingsManager.getDouble("slideshowtimer")*1000));


    ui->fullscreenLabel->setVisible(qvApp->getSettingsManager().getBoolean("fullscreendetails") && windowState().testFlag(Qt::WindowFullScreen));

    updateMenuBarVisible();

    // repaint in case background color changed
    update();
}

void MainWindow::shortcutsUpdated()
{
    // If esc is not used in a shortcut, let it exit fullscreen
    escShortcut->setKey(Qt::Key_Escape);

    const auto &actionLibrary = qvApp->getActionManager().getActionLibrary();
    for (const auto &action : actionLibrary)
    {
        if (action->shortcuts().contains(QKeySequence(Qt::Key_Escape)))
        {
            escShortcut->setKey({});
            break;
        }
    }
}

void MainWindow::openRecent(int i)
{
    auto recentsList = qvApp->getActionManager().getRecentsList();
    graphicsView->loadFile(recentsList.value(i).filePath);
    cancelSlideshow();
}

void MainWindow::fileChanged(const bool isRestoringState)
{
    populateOpenWithTimer->start();
    disableActions();

    if (info->isVisible())
        refreshProperties();
    buildWindowTitle();
    updateWindowFilePath();
    if (!isRestoringState)
        setWindowSize();

    // full repaint to handle error message
    update();
}

void MainWindow::zoomLevelChanged()
{
    if (!zoomTitlebarUpdateTimer->isActive())
        zoomTitlebarUpdateTimer->start();
}

void MainWindow::syncCalculatedZoomMode()
{
    const bool isZoomToFit = graphicsView->getCalculatedZoomMode() == Qv::CalculatedZoomMode::ZoomToFit;
    const bool isFillWindow = graphicsView->getCalculatedZoomMode() == Qv::CalculatedZoomMode::FillWindow;
    for (const auto &action : qvApp->getActionManager().getAllClonesOfAction("zoomtofit", this))
        action->setChecked(isZoomToFit);
    for (const auto &action : qvApp->getActionManager().getAllClonesOfAction("fillwindow", this))
        action->setChecked(isFillWindow);
}

void MainWindow::syncNavigationResetsZoom()
{
    const bool value = graphicsView->getNavigationResetsZoom();
    for (const auto &action : qvApp->getActionManager().getAllClonesOfAction("navresetszoom", this))
        action->setChecked(value);
}

void MainWindow::disableActions()
{
    const auto &actionLibrary = qvApp->getActionManager().getActionLibrary();
    for (const auto &action : actionLibrary)
    {
        const auto &data = action->data().toStringList();
        const auto &clonesOfAction = qvApp->getActionManager().getAllClonesOfAction(data.first(), this);

        // Enable this window's actions when a file is loaded
        if (data.last().contains("disable"))
        {
            for (const auto &clone : clonesOfAction)
            {
                const auto &cloneData = clone->data().toStringList();
                if (cloneData.last() == "disable")
                {
                    clone->setEnabled(getCurrentFileDetails().isPixmapLoaded);
                }
                else if (cloneData.last() == "gifdisable")
                {
                    clone->setEnabled(getCurrentFileDetails().isMovieLoaded);
                }
                else if (cloneData.last() == "undodisable")
                {
                    clone->setEnabled(!lastDeletedFiles.isEmpty() && !lastDeletedFiles.top().pathInTrash.isEmpty());
                }
                else if (cloneData.last() == "folderdisable")
                {
                    clone->setEnabled(!getCurrentFileDetails().folderFileInfoList.isEmpty());
                }
                else if (cloneData.last() == "windowdisable")
                {
                    clone->setEnabled(true);
                }
            }
        }
    }

    const auto &openWithMenus = qvApp->getActionManager().getAllClonesOfMenu("openwith");
    for (const auto &menu : openWithMenus)
    {
        menu->setEnabled(getCurrentFileDetails().isPixmapLoaded);
    }
}

void MainWindow::requestPopulateOpenWithMenu()
{
    openWithFutureWatcher.setFuture(QtConcurrent::run([&]{
        const auto &curFilePath = getCurrentFileDetails().fileInfo.absoluteFilePath();
        return OpenWith::getOpenWithItems(curFilePath);
    }));
}

void MainWindow::populateOpenWithMenu(const QList<OpenWith::OpenWithItem> openWithItems)
{
    for (int i = 0; i < qvApp->getActionManager().getOpenWithMaxLength(); i++)
    {
        const auto clonedActions = qvApp->getActionManager().getAllClonesOfAction("openwith" + QString::number(i), this);
        for (const auto &action : clonedActions)
        {
            // If we are within the bounds of the open with list
            if (i < openWithItems.length())
            {
                auto openWithItem = openWithItems.value(i);

                action->setVisible(true);
                action->setIconVisibleInMenu(false); // Hide icon temporarily to speed up updates in certain cases
                action->setText(openWithItem.name);
                if (qvApp->getShowSubmenuIcons())
                {
                    if (!openWithItem.iconName.isEmpty())
                        action->setIcon(QIcon::fromTheme(openWithItem.iconName));
                    else
                        action->setIcon(openWithItem.icon);
                }
                auto data = action->data().toList();
                data.replace(1, QVariant::fromValue(openWithItem));
                action->setData(data);
                if (qvApp->getShowSubmenuIcons())
                    action->setIconVisibleInMenu(true);
            }
            else
            {
                action->setVisible(false);
            }
        }
    }
}

void MainWindow::refreshProperties()
{
    int value4;
    if (getCurrentFileDetails().isMovieLoaded)
        value4 = graphicsView->getLoadedMovie().frameCount();
    else
        value4 = 0;
    info->setInfo(getCurrentFileDetails().fileInfo, getCurrentFileDetails().baseImageSize.width(), getCurrentFileDetails().baseImageSize.height(), value4);
}

void MainWindow::buildWindowTitle()
{
    QString newString = "qView";
    if (getCurrentFileDetails().fileInfo.isFile())
    {
        const QVImageCore::FileDetails &fileDetails = getCurrentFileDetails();
        const bool hasError = fileDetails.errorData.has_value();
        auto getFileName = [&]() { return fileDetails.fileInfo.fileName(); };
        auto getZoomLevel = [&]() { return QString::number((hasError ? 1.0 : graphicsView->getZoomLevel()) * 100.0, 'f', 1) + "%"; };
        auto getImageIndex = [&]() { return QString::number(fileDetails.loadedIndexInFolder+1); };
        auto getImageCount = [&]() { return QString::number(fileDetails.folderFileInfoList.count()); };
        auto getImageWidth = [&]() { return QString::number(hasError ? 0 : fileDetails.baseImageSize.width()); };
        auto getImageHeight = [&]() { return QString::number(hasError ? 0 : fileDetails.baseImageSize.height()); };
        auto getFileSize = [&]() { return QVInfoDialog::formatBytes(hasError ? 0 : fileDetails.fileInfo.size()); };
        switch (qvApp->getSettingsManager().getEnum<Qv::TitleBarText>("titlebarmode")) {
        case Qv::TitleBarText::Minimal:
        {
            newString = getFileName();
            break;
        }
        case Qv::TitleBarText::Practical:
        {
            newString = getZoomLevel() + " - " + getImageIndex() + "/" + getImageCount() + " - " + getFileName();
            break;
        }
        case Qv::TitleBarText::Verbose:
        {
            newString = getZoomLevel() + " - " + getImageIndex() + "/" + getImageCount() + " - " + getFileName() + " - " +
                        getImageWidth() + "x" + getImageHeight() + " - " + getFileSize() + " - qView";
            break;
        }
        case Qv::TitleBarText::Custom:
        {
            newString = "";
            const QString customText = qvApp->getSettingsManager().getString("customtitlebartext");
            for (int i = 0; i < customText.length(); i++)
            {
                const QChar c = customText.at(i);
                if (c == '%')
                {
                    i++;
                    if (i >= customText.length()) break;
                    const QChar n = customText.at(i);
                    if (n == 'n') newString += getFileName();
                    else if (n == 'z') newString += getZoomLevel();
                    else if (n == 'i') newString += getImageIndex();
                    else if (n == 'c') newString += getImageCount();
                    else if (n == 'w') newString += getImageWidth();
                    else if (n == 'h') newString += getImageHeight();
                    else if (n == 's') newString += getFileSize();
                    else newString += n;
                }
                else newString += c;
            }
            break;
        }
        default:
            break;
        }
    }

    setWindowTitle(newString);

    // Update fullscreen label to titlebar text as well
    ui->fullscreenLabel->setText(newString);
}

void MainWindow::updateWindowFilePath()
{
    if (!windowHandle())
        return;

    const bool shouldPopulate = getCurrentFileDetails().isPixmapLoaded && !getTitlebarHidden();
    windowHandle()->setFilePath(shouldPopulate ? getCurrentFileDetails().fileInfo.absoluteFilePath() : "");
}

void MainWindow::updateMenuBarVisible()
{
    bool alwaysVisible = false;
    bool hideWhenImmersive = false;
#ifdef Q_OS_MACOS
    alwaysVisible = true;
#else
    hideWhenImmersive = true;
#endif
    const auto isImmersive = [&]() { return getTitlebarHidden() || windowState().testFlag(Qt::WindowFullScreen); };
    menuBar()->setVisible(alwaysVisible || (menuBarEnabled && !(hideWhenImmersive && isImmersive())));
}

bool MainWindow::getWindowOnTop() const
{
    const QWindow *winHandle = windowHandle();
    return winHandle && winHandle->flags().testFlag(Qt::WindowStaysOnTopHint);
}

bool MainWindow::getTitlebarHidden() const
{
    if (!windowHandle())
        return false;

#if defined COCOA_LOADED && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QVCocoaFunctions::getTitlebarHidden(this);
#else
    return !windowFlags().testFlag(Qt::WindowTitleHint);
#endif
}

void MainWindow::setTitlebarHidden(const bool shouldHide)
{
    if (!windowHandle())
        return;

    const auto customizeWindowFlags = [this](const Qt::WindowFlags flagsToChange, const bool on) {
        Qv::alterWindowFlags(this, [&](Qt::WindowFlags f) { return (on ? (f | flagsToChange) : (f & ~flagsToChange)) | Qt::CustomizeWindowHint; });
    };

#if defined COCOA_LOADED && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QVCocoaFunctions::setTitlebarHidden(this, shouldHide);
    customizeWindowFlags(Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint | Qt::WindowFullscreenButtonHint, !shouldHide);
#elif defined WIN32_LOADED
    customizeWindowFlags(Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint, !shouldHide);
#else
    customizeWindowFlags(Qt::WindowTitleHint, !shouldHide);
#endif

    const auto toggleTitlebarActions = qvApp->getActionManager().getAllClonesOfAction("toggletitlebar", this);
    for (const auto &toggleTitlebarAction : toggleTitlebarActions)
    {
        toggleTitlebarAction->setText(shouldHide ? tr("Show Title&bar") : tr("Hide Title&bar"));
    }

    updateWindowFilePath();
    updateMenuBarVisible();
    update();
    graphicsView->fitOrConstrainImage();
}

void MainWindow::setWindowSize(const bool isFromTransform)
{
    if (!getCurrentFileDetails().isPixmapLoaded)
        return;

    //check if the program is configured to resize the window
    const auto windowResizeMode = qvApp->getSettingsManager().getEnum<Qv::WindowResizeMode>("windowresizemode");
    if (!(windowResizeMode == Qv::WindowResizeMode::WhenOpeningImages || (windowResizeMode == Qv::WindowResizeMode::WhenLaunching && justLaunchedWithImage)))
        return;

    justLaunchedWithImage = false;

    //check if window is maximized or fullscreened
    if (windowState().testFlag(Qt::WindowMaximized) || windowState().testFlag(Qt::WindowFullScreen))
        return;

    const qreal minWindowResizedPercentage = qvApp->getSettingsManager().getInteger("minwindowresizedpercentage")/100.0;
    const qreal maxWindowResizedPercentage = qvApp->getSettingsManager().getInteger("maxwindowresizedpercentage")/100.0;

    // Try to grab the current screen
    QScreen *currentScreen = screenContaining(frameGeometry());
    // If completely offscreen, use first screen as fallback
    if (!currentScreen)
        currentScreen = QGuiApplication::screens().at(0);

    QSize extraWidgetsSize { 0, 0 };

    if (menuBar()->isVisible())
        extraWidgetsSize.rheight() += menuBar()->height();

    const int titlebarOverlap = getTitlebarOverlap();
    if (titlebarOverlap != 0)
        extraWidgetsSize.rheight() += titlebarOverlap;

    const QSize windowFrameSize = frameGeometry().size() - geometry().size();
    const QSize hardLimitSize = currentScreen->availableSize() - windowFrameSize - extraWidgetsSize;
    const QSize screenSize = currentScreen->size();
    const QSize minWindowSize = (screenSize * minWindowResizedPercentage).boundedTo(hardLimitSize);
    const QSize maxWindowSize = (screenSize * maxWindowResizedPercentage).boundedTo(hardLimitSize);
    const bool isZoomFixed = (!graphicsView->getNavigationResetsZoom() || isFromTransform) && !graphicsView->getCalculatedZoomMode().has_value();
    const QSizeF imageSize = graphicsView->getEffectiveOriginalSize() * (isZoomFixed ? graphicsView->getZoomLevel() : 1.0);
    const int fitOverscan = graphicsView->getFitOverscan();
    const QSize fitOverscanSize = QSize(fitOverscan * 2, fitOverscan * 2);
    const LogicalPixelFitter fitter = graphicsView->getPixelFitter();

    QSize targetSize = fitter.snapSize(imageSize) - fitOverscanSize;

    if (targetSize.width() > maxWindowSize.width() || targetSize.height() > maxWindowSize.height())
    {
        const QSizeF enforcedSize = fitter.unsnapSize(maxWindowSize) + fitOverscanSize;
        const qreal fitRatio = qMin(enforcedSize.width() / imageSize.width(), enforcedSize.height() / imageSize.height());
        targetSize = fitter.snapSize(imageSize * fitRatio) - fitOverscanSize;
    }

    targetSize = targetSize.expandedTo(minWindowSize).boundedTo(maxWindowSize);

    const bool recenterImage = isZoomFixed && geometry().size() != targetSize + extraWidgetsSize;

    const auto afterMatchingSizeMode = qvApp->getSettingsManager().getEnum<Qv::AfterMatchingSize>("aftermatchingsizemode");
    const QPoint referenceCenter =
        afterMatchingSizeMode == Qv::AfterMatchingSize::CenterOnPrevious ? geometry().center() :
        afterMatchingSizeMode == Qv::AfterMatchingSize::CenterOnScreen ? currentScreen->availableGeometry().center() :
        QPoint();

    // Resize window first, reposition later
    // This is smoother than a single geometry set for some reason
    resize(targetSize + extraWidgetsSize);
    QRect newRect = geometry();

    if (afterMatchingSizeMode != Qv::AfterMatchingSize::AvoidRepositioning)
        newRect.moveCenter(referenceCenter);

    // Ensure titlebar is not above or below the available screen area
    const QRect availableScreenRect = currentScreen->availableGeometry();
    const int topFrameHeight = geometry().top() - frameGeometry().top();
    const int windowMinY = availableScreenRect.top() + topFrameHeight;
    const int windowMaxY = availableScreenRect.top() + availableScreenRect.height() - titlebarOverlap;
    if (newRect.top() < windowMinY)
        newRect.moveTop(windowMinY);
    if (newRect.top() > windowMaxY)
        newRect.moveTop(windowMaxY);

    // Reposition window
    setGeometry(newRect);

    if (recenterImage)
        graphicsView->centerImage();
}

// Initially copied from Qt source code (QGuiApplication::screenAt) and then customized
QScreen *MainWindow::screenContaining(const QRect &rect)
{
    QScreen *bestScreen = nullptr;
    int bestScreenArea = 0;
    QVarLengthArray<const QScreen *, 8> visitedScreens;
    const auto screens = QGuiApplication::screens();
    for (const QScreen *screen : screens) {
        if (visitedScreens.contains(screen))
            continue;
        // The virtual siblings include the screen itself, so iterate directly
        const auto siblings = screen->virtualSiblings();
        for (QScreen *sibling : siblings) {
            const QRect intersect = sibling->geometry().intersected(rect);
            const int area = intersect.width() * intersect.height();
            if (area > bestScreenArea) {
                bestScreen = sibling;
                bestScreenArea = area;
            }
            visitedScreens.append(sibling);
        }
    }
    return bestScreen;
}

const QJsonObject MainWindow::getSessionState() const
{
    QJsonObject state;

    state["geometry"] = QString(saveGeometry().toBase64());

    state["windowOnTop"] = getWindowOnTop();

    state["titlebarHidden"] = getTitlebarHidden();

    if (getCurrentFileDetails().isPixmapLoaded)
        state["path"] = getCurrentFileDetails().fileInfo.absoluteFilePath();

    state["graphicsView"] = graphicsView->getSessionState();

    return state;
}

void MainWindow::loadSessionState(const QJsonObject &state, const bool isInitialPhase)
{
    if (isInitialPhase)
    {
        restoreGeometry(QByteArray::fromBase64(state["geometry"].toString().toUtf8()));

        graphicsView->loadSessionState(state["graphicsView"].toObject());

        return;
    }

    if (state["windowOnTop"].toBool() != getWindowOnTop())
        toggleWindowOnTop();

    if (state["titlebarHidden"].toBool() != getTitlebarHidden())
        toggleTitlebarHidden();

    const QString path = state["path"].toString();
    if (!path.isEmpty())
    {
        graphicsView->setLoadIsFromSessionRestore(true);
        openFile(path);
    }
}

bool MainWindow::getIsPixmapLoaded() const
{
    return getCurrentFileDetails().isPixmapLoaded;
}

void MainWindow::setJustLaunchedWithImage(bool value)
{
    justLaunchedWithImage = value;
}

void MainWindow::openUrl(const QUrl &url)
{
    if (!url.isValid()) {
        QMessageBox::critical(this, tr("Error"), tr("Error: URL is invalid"));
        return;
    }

    auto request = QNetworkRequest(url);
    auto *reply = networkAccessManager.get(request);
    auto *progressDialog = new QProgressDialog(tr("Downloading image..."), tr("Cancel"), 0, 100);
    progressDialog->setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    progressDialog->setAutoClose(false);
    progressDialog->setAutoReset(false);
    progressDialog->setWindowTitle(tr("Open URL..."));
    progressDialog->open();

    connect(progressDialog, &QProgressDialog::canceled, reply, [reply]{
        reply->abort();
    });

    connect(reply, &QNetworkReply::downloadProgress, progressDialog, [progressDialog](qreal bytesReceived, qreal bytesTotal){
        auto percent = (bytesReceived/bytesTotal)*100;
        progressDialog->setValue(qRound(percent));
    });


    connect(reply, &QNetworkReply::finished, progressDialog, [progressDialog, reply, this]{
        if (reply->error())
        {
            progressDialog->close();
            QMessageBox::critical(this, tr("Error"), tr("Error ") + QString::number(reply->error()) + ": " + reply->errorString());

            progressDialog->deleteLater();
            return;
        }

        progressDialog->setMaximum(0);

        auto *tempFile = new QTemporaryFile(this);
        tempFile->setFileTemplate(QDir::tempPath() + "/" + qvApp->applicationName() + ".XXXXXX.png");

        auto *saveFutureWatcher = new QFutureWatcher<bool>();
        connect(saveFutureWatcher, &QFutureWatcher<bool>::finished, this, [progressDialog, tempFile, saveFutureWatcher, this](){
            progressDialog->close();
            if (saveFutureWatcher->result())
            {
                if (tempFile->open())
                {
                    openFile(tempFile->fileName());
                }
            }
            else
            {
                 QMessageBox::critical(this, tr("Error"), tr("Error: Invalid image"));
                 tempFile->deleteLater();
            }
            progressDialog->deleteLater();
            saveFutureWatcher->deleteLater();
        });

        saveFutureWatcher->setFuture(QtConcurrent::run([reply, tempFile]{
            return QImage::fromData(reply->readAll()).save(tempFile, "png");
        }));
    });
}

void MainWindow::pickUrl()
{
    auto inputDialog = new QInputDialog(this);
    inputDialog->setWindowTitle(tr("Open URL..."));
    inputDialog->setLabelText(tr("URL of a supported image file:"));
    inputDialog->resize(350, inputDialog->height());
    inputDialog->setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    connect(inputDialog, &QInputDialog::finished, this, [inputDialog, this](int result) {
        if (result)
        {
            const auto url = QUrl(inputDialog->textValue());
            openUrl(url);
        }
        inputDialog->deleteLater();
    });
    inputDialog->open();
}

void MainWindow::reloadFile()
{
    graphicsView->reloadFile();
}

void MainWindow::openWith(const OpenWith::OpenWithItem &openWithItem)
{
    OpenWith::openWith(getCurrentFileDetails().fileInfo.absoluteFilePath(), openWithItem);
}

void MainWindow::openContainingFolder()
{
    if (!getCurrentFileDetails().isPixmapLoaded)
        return;

    const QFileInfo selectedFileInfo = getCurrentFileDetails().fileInfo;

#ifdef WIN32_LOADED
    QString pathToSelect = QDir::toNativeSeparators(selectedFileInfo.absoluteFilePath());
    if (pathToSelect.length() > 259 && pathToSelect.startsWith(R"(\\)"))
    {
        // The Shell API seems to handle long paths, unless they are UNC :(
        pathToSelect = QVWin32Functions::getShortPath(pathToSelect);
        if (pathToSelect.isEmpty())
            return;
    }
    QVWin32Functions::showInExplorer(pathToSelect);
#elif defined Q_OS_MACOS
    QProcess::execute("open", QStringList() << "-R" << selectedFileInfo.absoluteFilePath());
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(selectedFileInfo.absolutePath()));
#endif
}

void MainWindow::showFileInfo()
{
    refreshProperties();
    info->show();
    info->raise();
}

void MainWindow::askDeleteFile(bool permanent)
{
    if (!permanent && !qvApp->getSettingsManager().getBoolean("askdelete"))
    {
        deleteFile(permanent);
        return;
    }

    const QFileInfo &fileInfo = getCurrentFileDetails().fileInfo;
    const QString fileName = getCurrentFileDetails().fileInfo.fileName();

    if (!fileInfo.isWritable())
    {
        QMessageBox::critical(this, tr("Error"), tr("Can't delete %1:\nNo write permission or file is read-only.").arg(fileName));
        return;
    }

    QString messageText;
    if (permanent)
    {
        messageText = tr("Are you sure you want to permanently delete %1? This can't be undone.").arg(fileName);
    }
    else
    {
#ifdef Q_OS_WIN
        messageText = tr("Are you sure you want to move %1 to the Recycle Bin?").arg(fileName);
#else
        messageText = tr("Are you sure you want to move %1 to the Trash?").arg(fileName);
#endif
    }

    auto *msgBox = new QMessageBox(QMessageBox::Question, tr("Delete"), messageText,
                       QMessageBox::Yes | QMessageBox::No, this);
    if (!permanent)
        msgBox->setCheckBox(new QCheckBox(tr("Do not ask again")));

    connect(msgBox, &QMessageBox::finished, this, [this, msgBox, permanent](int result){
        if (result != QMessageBox::Yes)
            return;

        if (!permanent)
        {
            QSettings settings;
            settings.beginGroup("options");
            settings.setValue("askdelete", !msgBox->checkBox()->isChecked());
            qvApp->getSettingsManager().loadSettings();
        }
        this->deleteFile(permanent);
    });

    msgBox->open();
}

void MainWindow::deleteFile(bool permanent)
{
    const QFileInfo &fileInfo = getCurrentFileDetails().fileInfo;
    const QString filePath = fileInfo.absoluteFilePath();
    const QString fileName = fileInfo.fileName();

    graphicsView->closeImage();

    bool success;
    QString trashFilePath;
    if (permanent)
    {
        success = QFile::remove(filePath);
    }
    else
    {
        QFile file(filePath);
        success = file.moveToTrash();
        if (success)
            trashFilePath = file.fileName();
    }

    if (!success || QFile::exists(filePath))
    {
        openFile(filePath);
        QMessageBox::critical(this, tr("Error"), tr("Can't delete %1.").arg(fileName));
        return;
    }

    auto afterDelete = qvApp->getSettingsManager().getEnum<Qv::AfterDelete>("afterdelete");
    if (afterDelete == Qv::AfterDelete::MoveForward)
        nextFile();
    else if (afterDelete == Qv::AfterDelete::MoveBack)
        previousFile();

    if (!trashFilePath.isEmpty())
        lastDeletedFiles.push({trashFilePath, filePath});

    disableActions();
}

void MainWindow::undoDelete()
{
    if (lastDeletedFiles.isEmpty())
        return;

    const DeletedPaths lastDeletedFile = lastDeletedFiles.pop();
    if (lastDeletedFile.pathInTrash.isEmpty() || lastDeletedFile.previousPath.isEmpty())
        return;

    const QFileInfo fileInfo(lastDeletedFile.pathInTrash);
    if (!fileInfo.isWritable())
    {
        QMessageBox::critical(this, tr("Error"), tr("Can't undo deletion of %1:\n"
                                                    "No write permission or file is read-only.").arg(fileInfo.fileName()));
        return;
    }

    bool success = QFile::rename(lastDeletedFile.pathInTrash, lastDeletedFile.previousPath);
    if (!success)
    {
        QMessageBox::critical(this, tr("Error"), tr("Failed undoing deletion of %1.").arg(fileInfo.fileName()));
    }

    openFile(lastDeletedFile.previousPath);
    disableActions();
}

void MainWindow::copy()
{
    auto *mimeData = graphicsView->getMimeData();
    if (!mimeData->hasImage() || !mimeData->hasUrls())
    {
        mimeData->deleteLater();
        return;
    }

    QApplication::clipboard()->setMimeData(mimeData);
}

void MainWindow::paste()
{
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    if (mimeData == nullptr)
        return;

    if (mimeData->hasText())
    {
        auto url = QUrl(mimeData->text());

        if (url.isValid() && (url.scheme() == "http" || url.scheme() == "https"))
        {
            openUrl(url);
            return;
        }
    }

    graphicsView->loadMimeData(mimeData);
}

void MainWindow::rename()
{
    if (!getCurrentFileDetails().isPixmapLoaded)
        return;

    auto *renameDialog = new QVRenameDialog(this, getCurrentFileDetails().fileInfo);
    connect(renameDialog, &QVRenameDialog::newFileToOpen, this, &MainWindow::openFile);
    connect(renameDialog, &QVRenameDialog::readyToRenameFile, this, [this] () {
        if (auto device = graphicsView->getLoadedMovie().device()) {
            device->close();
        }
    });

    renameDialog->open();
}

void MainWindow::zoomIn()
{
    graphicsView->zoomIn();
}

void MainWindow::zoomOut()
{
    graphicsView->zoomOut();
}

void MainWindow::originalSize()
{
    graphicsView->setCalculatedZoomMode(Qv::CalculatedZoomMode::OriginalSize);
}

void MainWindow::setZoomToFit(const bool value)
{
    graphicsView->setCalculatedZoomMode(value ? std::make_optional(Qv::CalculatedZoomMode::ZoomToFit) : std::nullopt);
}

void MainWindow::setFillWindow(const bool value)
{
    graphicsView->setCalculatedZoomMode(value ? std::make_optional(Qv::CalculatedZoomMode::FillWindow) : std::nullopt);
}

void MainWindow::setNavigationResetsZoom(const bool value)
{
    graphicsView->setNavigationResetsZoom(value);
}

void MainWindow::rotateRight()
{
    graphicsView->rotateImage(90);
    graphicsView->fitOrConstrainImage();
    setWindowSize(true);
}

void MainWindow::rotateLeft()
{
    graphicsView->rotateImage(-90);
    graphicsView->fitOrConstrainImage();
    setWindowSize(true);
}

void MainWindow::mirror()
{
    graphicsView->mirrorImage();
    graphicsView->fitOrConstrainImage();
}

void MainWindow::flip()
{
    graphicsView->flipImage();
    graphicsView->fitOrConstrainImage();
}

void MainWindow::resetTransformation()
{
    graphicsView->resetTransformation();
    graphicsView->fitOrConstrainImage();
    setWindowSize(true);
}

void MainWindow::firstFile()
{
    graphicsView->goToFile(Qv::GoToFileMode::First);
}

void MainWindow::previousFile()
{
    graphicsView->goToFile(Qv::GoToFileMode::Previous);
}

void MainWindow::nextFile()
{
    graphicsView->goToFile(Qv::GoToFileMode::Next);
}

void MainWindow::lastFile()
{
    graphicsView->goToFile(Qv::GoToFileMode::Last);
}

void MainWindow::previousRandomFile()
{
    graphicsView->goToFile(Qv::GoToFileMode::PreviousRandom);
}

void MainWindow::randomFile()
{
    graphicsView->goToFile(Qv::GoToFileMode::Random);
}

void MainWindow::saveFrameAs()
{
    QSettings settings;
    settings.beginGroup("recents");
    if (!getCurrentFileDetails().isMovieLoaded)
        return;

    if (graphicsView->getLoadedMovie().state() == QMovie::Running)
    {
        pause();
    }
    QFileDialog *saveDialog = new QFileDialog(this, tr("Save Frame As..."));
    saveDialog->setDirectory(settings.value("lastFileDialogDir", QDir::homePath()).toString());
    saveDialog->setNameFilters(qvApp->getNameFilterList());
    saveDialog->selectFile(getCurrentFileDetails().fileInfo.baseName() + "-" + QString::number(graphicsView->getLoadedMovie().currentFrameNumber()) + ".png");
    saveDialog->setDefaultSuffix("png");
    saveDialog->setAcceptMode(QFileDialog::AcceptSave);
    saveDialog->open();
    connect(saveDialog, &QFileDialog::fileSelected, this, [=](const QString &fileName){
        graphicsView->getLoadedMovie().currentPixmap().save(fileName, nullptr, 100);
    });
}

void MainWindow::pause()
{
    if (!getCurrentFileDetails().isMovieLoaded)
        return;

    const auto pauseActions = qvApp->getActionManager().getAllClonesOfAction("pause", this);

    if (graphicsView->getLoadedMovie().state() == QMovie::Running)
    {
        graphicsView->setPaused(true);
        for (const auto &pauseAction : pauseActions)
        {
            pauseAction->setText(tr("Res&ume"));
            pauseAction->setIcon(QIcon::fromTheme("media-playback-start"));
        }
    }
    else
    {
        graphicsView->setPaused(false);
        for (const auto &pauseAction : pauseActions)
        {
            pauseAction->setText(tr("Pause"));
            pauseAction->setIcon(QIcon::fromTheme("media-playback-pause"));
        }
    }
}

void MainWindow::nextFrame()
{
    if (!getCurrentFileDetails().isMovieLoaded)
        return;

    graphicsView->jumpToNextFrame();
}

void MainWindow::toggleSlideshow()
{
    const auto slideshowActions = qvApp->getActionManager().getAllClonesOfAction("slideshow", this);
    const bool isStarting = !slideshowTimer->isActive();
    if (isStarting)
        slideshowTimer->start();
    else
        slideshowTimer->stop();
    for (const auto &slideshowAction : slideshowActions)
    {
        slideshowAction->setText(isStarting ? tr("Stop S&lideshow") : tr("Start S&lideshow"));
        slideshowAction->setIcon(QIcon::fromTheme(isStarting ? "media-playback-stop" : "media-playback-start"));
    }
    if (isStarting)
    {
        slideshowSetOnTopFlag = qvApp->getSettingsManager().getBoolean("slideshowkeepswindowontop") && !getWindowOnTop();
        if (slideshowSetOnTopFlag)
            toggleWindowOnTop();
    }
    else
    {
        if (slideshowSetOnTopFlag && getWindowOnTop())
            toggleWindowOnTop();
    }
}

void MainWindow::cancelSlideshow()
{
    if (slideshowTimer->isActive())
        toggleSlideshow();
}

void MainWindow::slideshowAction()
{
    switch (qvApp->getSettingsManager().getEnum<Qv::SlideshowDirection>("slideshowdirection"))
    {
    case Qv::SlideshowDirection::Forward:
        nextFile();
        break;
    case Qv::SlideshowDirection::Backward:
        previousFile();
        break;
    case Qv::SlideshowDirection::Random:
        randomFile();
        break;
    case Qv::SlideshowDirection::PreviousRandom:
        previousRandomFile();
        break;
    }
}

void MainWindow::decreaseSpeed()
{
    if (!getCurrentFileDetails().isMovieLoaded)
        return;

    graphicsView->setSpeed(graphicsView->getLoadedMovie().speed()-25);
}

void MainWindow::resetSpeed()
{
    if (!getCurrentFileDetails().isMovieLoaded)
        return;

    graphicsView->setSpeed(100);
}

void MainWindow::increaseSpeed()
{
    if (!getCurrentFileDetails().isMovieLoaded)
        return;

    graphicsView->setSpeed(graphicsView->getLoadedMovie().speed()+25);
}

void MainWindow::toggleFullScreen()
{
    // Note: This is only triggered by the menu action, so the logic here should be kept to a minimum. Anything that
    // needs to run even if the window manager initiated the change should be triggered by QEvent::WindowStateChange.

    // Disable updates during window state change to resolve visual glitches on macOS if the titlebar is hidden
    setUpdatesEnabled(false);

    if (windowState().testFlag(Qt::WindowFullScreen))
    {
        setWindowState(storedWindowState);
    }
    else
    {
        storedWindowState = windowState();

        // Restore the titlebar if it was hidden because the window manager might do something special with the
        // titlebar (e.g. macOS) in fullscreen mode or get confused by the titlebar being hidden (e.g. Windows).
        storedTitlebarHidden = getTitlebarHidden();
        if (storedTitlebarHidden)
            setTitlebarHidden(false);

        showFullScreen();
    }

    setUpdatesEnabled(true);
}

void MainWindow::toggleWindowOnTop()
{
    if (!windowHandle())
        return;

    const bool targetValue = !getWindowOnTop();

    Qv::alterWindowFlags(this, [&](Qt::WindowFlags f) { return f.setFlag(Qt::WindowStaysOnTopHint, targetValue); });

    if (info->windowHandle())
        Qv::alterWindowFlags(info, [&](Qt::WindowFlags f) { return f.setFlag(Qt::WindowStaysOnTopHint, targetValue); });

    for (const auto &action : qvApp->getActionManager().getAllClonesOfAction("windowontop", this))
       action->setChecked(targetValue);

#ifdef COCOA_LOADED
    // Make sure window still participates in Mission Control
    QVCocoaFunctions::setWindowCollectionBehaviorManaged(this);
#endif

    emit qvApp->windowOnTopChanged();
}

void MainWindow::toggleTitlebarHidden()
{
    if (windowState().testFlag(Qt::WindowFullScreen))
        return;

    setTitlebarHidden(!getTitlebarHidden());
}

int MainWindow::getTitlebarOverlap() const
{
#ifdef COCOA_LOADED
    // To account for fullsizecontentview on mac
    return QVCocoaFunctions::getObscuredHeight(window()->windowHandle());
#endif

    return 0;
}

MainWindow::ViewportPosition MainWindow::getViewportPosition() const
{
    ViewportPosition result;
    // This accounts for anything that may be above the viewport such as the menu bar (if it's inside
    // the window) and/or the label that displays titlebar text in full screen mode.
    result.widgetY = windowHandle() ? graphicsView->mapTo(this, QPoint()).y() : 0;
    // On macOS, part of the viewport may be additionally covered with the window's translucent
    // titlebar due to full size content view.
    result.obscuredHeight = qMax(getTitlebarOverlap() - result.widgetY, 0);
    return result;
}
