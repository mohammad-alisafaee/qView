#ifndef QVNAMESPACE_H
#define QVNAMESPACE_H

#include <QColor>
#include <QSet>
#include <QString>
#include <QWidget>
#include <QWindow>

namespace Qv
{
    // Data constants

    inline constexpr int SessionStateVersion = 1;

    // Settings value enums

    enum class AfterDelete
    {
        MoveBack = 0,
        DoNothing = 1,
        MoveForward = 2
    };

    enum class AfterMatchingSize
    {
        AvoidRepositioning = 0,
        CenterOnPrevious = 1,
        CenterOnScreen = 2
    };

    enum class CalculatedZoomMode
    {
        ZoomToFit = 0,
        FillWindow = 1,
        OriginalSize = 2
    };

    enum class ClickOrDrag
    {
        Click = 0,
        Drag = 1
    };

    enum class ColorSpaceConversion
    {
        Disabled = 0,
        AutoDetect = 1,
        SRgb = 2,
        DisplayP3 = 3
    };

    enum class PreloadMode
    {
        Disabled = 0,
        Adjacent = 1,
        Extended = 2
    };

    enum class SlideshowDirection
    {
        Forward = 0,
        Backward = 1,
        Random = 2
    };

    enum class SmoothScalingMode
    {
        Disabled = 0,
        Bilinear = 1,
        Expensive = 2
    };

    enum class SortMode
    {
        Name = 0,
        DateModified = 1,
        DateCreated = 2,
        Size = 3,
        Type = 4,
        Random = 5
    };

    enum class TitleBarText
    {
        Basic = 0,
        Minimal = 1,
        Practical = 2,
        Verbose = 3,
        Custom = 4
    };

    enum class ViewportClickAction
    {
        None = 0,
        ZoomToFit = 1,
        FillWindow = 2,
        OriginalSize = 3,
        ToggleFullScreen = 4,
        ToggleTitlebarHidden = 5,
        CenterImage = 6
    };

    enum class ViewportDragAction
    {
        None = 0,
        Pan = 1,
        MoveWindow = 2
    };

    enum class ViewportScrollAction
    {
        None = 0,
        Zoom = 1,
        Navigate = 2,
        Pan = 3
    };

    enum class WindowResizeMode
    {
        Never = 0,
        WhenLaunching = 1,
        WhenOpeningImages = 2
    };

    // Other enums

    enum class GoToFileMode
    {
        Constant,
        First,
        Previous,
        Next,
        Last,
        Random,
        PreviousRandom
    };

    // Helper functions

    inline QSet<QString> listToSet(const QStringList &list)
    {
        return QSet<QString>{list.begin(), list.end()};
    }

    inline QStringList setToList(const QSet<QString> &set)
    {
        return QStringList(set.begin(), set.end());
    }

    inline QStringList setToSortedList(const QSet<QString> &set, Qt::CaseSensitivity cs = Qt::CaseSensitive)
    {
        auto list = QStringList(set.begin(), set.end());
        list.sort(cs);
        return list;
    }

    inline bool calculatedZoomModeIsSticky(const CalculatedZoomMode mode)
    {
        return
            mode == CalculatedZoomMode::ZoomToFit ||
            mode == CalculatedZoomMode::FillWindow;
    }

    inline bool scrollActionIsSelfCompatible(const ViewportScrollAction action)
    {
        return
            action == ViewportScrollAction::None ||
            action == ViewportScrollAction::Pan;
    }

    inline qreal getPerceivedBrightness(const QColor &color)
    {
        return (color.red() * 0.299 + color.green() * 0.587 + color.blue() * 0.114) / 255.0;
    }

    inline void alterWindowFlags(QWidget *window, std::function<Qt::WindowFlags(Qt::WindowFlags)> alterFlags)
    {
        const Qt::WindowFlags newFlags = alterFlags(window->windowFlags());
        window->overrideWindowFlags(newFlags);
        window->windowHandle()->setFlags(newFlags);
    }
}

#endif // QVNAMESPACE_H
