#ifndef QVFILEENUMERATOR_H
#define QVFILEENUMERATOR_H

#include "qvnamespace.h"
#include <QCollator>
#include <QFileInfo>
#include <QObject>

class QVFileEnumerator : public QObject
{
    Q_OBJECT
public:
    struct CompatibleFile
    {
        QString absoluteFilePath;
        qint64 numericSortKey;
        QString stringSortKey;
    };

    explicit QVFileEnumerator(QObject *parent = nullptr);

    QList<CompatibleFile> getCompatibleFiles(const QString &dirPath) const;
    bool getIsLoopFoldersEnabled() const { return isLoopFoldersEnabled; }
    Qv::SortMode getSortMode() const { return sortMode; }
    void setSortMode(const Qv::SortMode mode);
    bool getSortDescending() const { return sortDescending; }
    void setSortDescending(const bool descending);
    void loadSettings(const bool isInitialLoad);

signals:
    void sortParametersChanged();

protected:
    qint64 getFileTimeSortKey(const QFileInfo &fileInfo, const QFileDevice::FileTime type) const;
    qint64 getRandomSortKey(const QString &filePath) const;

private:
    const quint32 baseRandomSortSeed {static_cast<quint32>(std::chrono::system_clock::now().time_since_epoch().count())};

    QCollator collator;

    bool isLoopFoldersEnabled {true};
    Qv::SortMode globalSortMode {Qv::SortMode::Name};
    Qv::SortMode sortMode {Qv::SortMode::Name};
    bool globalSortDescending {false};
    bool sortDescending {false};
    bool allowMimeContentDetection {false};
    bool skipHiddenFiles {false};
};

#endif // QVFILEENUMERATOR_H
