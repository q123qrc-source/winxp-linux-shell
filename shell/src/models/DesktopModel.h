// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QPointF>
#include <QStringList>

class AppRegistry;
class PlacesModel;
class Settings;
class QFileSystemWatcher;
class QTimer;

// The desktop's icons: My Documents, My Computer and the Recycle Bin (each
// optional, shell.conf [Desktop] show*), then every entry of the XDG desktop
// directory, kept up to date with a file watcher.
//
// Positions are in grid cells (fractional when Align to Grid is off) and are
// remembered per item in ~/.local/share/meadow/desktop-layout.json. New
// items take the first free cell, filling columns top to bottom from the
// left (from the right in RTL, which the view mirrors); the Recycle Bin
// starts in the bottom-right corner, as on XP. Auto Arrange keeps
// everything sorted by [Desktop] sortBy.
//
// .desktop launchers show their Name and Icon and run only when the file is
// executable ("trusted"), like file managers do; otherwise they are plain
// files, so a downloaded launcher cannot pose as something else.
class DesktopModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int columns READ columns NOTIFY gridChanged)
    Q_PROPERTY(int rows READ rows NOTIFY gridChanged)
    Q_PROPERTY(bool autoArrange READ autoArrange WRITE setAutoArrange NOTIFY optionsChanged)
    Q_PROPERTY(bool alignToGrid READ alignToGrid WRITE setAlignToGrid NOTIFY optionsChanged)
    Q_PROPERTY(QString sortBy READ sortBy NOTIFY optionsChanged)
    Q_PROPERTY(bool trashFull READ trashFull NOTIFY trashChanged)
    Q_PROPERTY(QString desktopDir READ desktopDir CONSTANT)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        IconRole,
        PathRole,
        KindRole,      // computer, documents, trash, folder, file, launcher
        CellXRole,
        CellYRole,
        TrustedRole,
    };

    DesktopModel(Settings *settings, PlacesModel *places, AppRegistry *apps, QObject *parent = nullptr);
    // For tests: explicit directories and layout file.
    DesktopModel(Settings *settings, PlacesModel *places, AppRegistry *apps, const QString &desktopDir,
                 const QString &trashDir, const QString &layoutFile, QObject *parent = nullptr);
    ~DesktopModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int columns() const { return m_columns; }
    int rows() const { return m_rows; }
    bool autoArrange() const;
    void setAutoArrange(bool on);
    bool alignToGrid() const;
    void setAlignToGrid(bool on);
    QString sortBy() const;
    bool trashFull() const { return m_trashFull; }
    QString desktopDir() const { return m_desktopDir; }

    // The view's grid size in cells (from the screen size and cell tokens).
    Q_INVOKABLE void setGridSize(int columns, int rows);
    Q_INVOKABLE int indexOf(const QString &id) const;
    Q_INVOKABLE QVariantMap get(const QString &id) const;

    // Moves the items by (dx, dy) cells, keeping them inside the grid and
    // off other icons (each goes to the nearest free cell).
    Q_INVOKABLE void moveItems(const QStringList &ids, qreal dx, qreal dy);
    // Sorts by name, size, type or modified and lays the icons out again.
    Q_INVOKABLE void arrange(const QString &by);
    Q_INVOKABLE void refresh();

    Q_INVOKABLE bool open(const QString &id);
    // New items in the desktop folder, uniquely named; returns their id.
    Q_INVOKABLE QString newFolder();
    Q_INVOKABLE QString newTextDocument();
    // A launcher for an installed application (Send To > Desktop).
    Q_INVOKABLE QString createShortcut(const QString &desktopId);
    // Renames a file; false (and nothing changes) for invalid or taken names.
    Q_INVOKABLE bool rename(const QString &id, const QString &newName);
    Q_INVOKABLE bool canRename(const QString &id) const;
    // Moves files to the Recycle Bin (XDG trash); specials are ignored.
    Q_INVOKABLE int moveToTrash(const QStringList &ids);
    Q_INVOKABLE void emptyTrash();
    // Name, type, location, size, modified (the Properties sheet).
    Q_INVOKABLE QVariantMap details(const QString &id) const;

    // Display Properties > Desktop: (None), the shipped "Meadow Hills", then
    // pictures from the asset override folders and the Pictures folder.
    // Entries: {label, value} (value as stored in [Desktop] wallpaper).
    Q_INVOKABLE QVariantList wallpaperChoices() const;
    // A readable picture Qt can load (Browse... results are checked).
    Q_INVOKABLE static bool isImage(const QString &path);

    static QString defaultLayoutFile();
    static QString defaultTrashDir();

Q_SIGNALS:
    void gridChanged();
    void optionsChanged();
    void trashChanged();
    // A non-executable .desktop file was opened.
    void untrustedLauncher(const QString &name);

private:
    struct Item {
        QString id;
        QString name;
        QString icon;
        QString path;
        QString kind;
        QString mime;
        qint64 size = 0;
        QDateTime modified;
        bool trusted = true;
        QPointF pos{-1, -1};
    };

    void reload();
    void placeNewItems();
    QPoint freeCellNear(const QPointF &target, const QSet<int> &ignore) const;
    QPoint firstFreeCell() const;
    bool occupied(const QPoint &cell, const QSet<int> &ignore) const;
    QPointF clamp(const QPointF &p) const;
    void loadLayout();
    void scheduleSave();
    void saveLayout();
    void updateTrash();
    Item makeFileItem(const QString &fileName) const;
    QString uniqueName(const QString &base, const QString &suffix) const;

    Settings *m_settings;
    PlacesModel *m_places;
    AppRegistry *m_apps;
    QString m_desktopDir;
    QString m_trashDir;
    QString m_layoutFile;
    QList<Item> m_items;
    QHash<QString, QPointF> m_saved;
    int m_columns = 0;
    int m_rows = 0;
    bool m_trashFull = false;
    QFileSystemWatcher *m_watcher;
    QTimer *m_saveTimer;
    QTimer *m_reloadTimer;
};
