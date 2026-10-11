// SPDX-License-Identifier: GPL-3.0-or-later
#include "DesktopModel.h"

#include "AppRegistry.h"
#include "ThemeLoader.h"
#include "DesktopEntry.h"
#include "PlacesModel.h"
#include "services/Launcher.h"
#include "services/Settings.h"

#include <QCollator>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <numeric>

Q_LOGGING_CATEGORY(lcDesktop, "meadow.shell.desktop")

namespace {
const QString kComputer = QStringLiteral("meadow:computer");
const QString kDocuments = QStringLiteral("meadow:documents");
const QString kTrash = QStringLiteral("meadow:trash");
const QString kFilePrefix = QStringLiteral("file:");

int cellKey(const QPoint &p) { return p.x() * 10000 + p.y(); }

// Specials first, in XP's order.
int specialRank(const QString &id)
{
    if (id == kDocuments) return 0;
    if (id == kComputer) return 1;
    if (id == kTrash) return 2;
    return 3;
}
} // namespace

DesktopModel::DesktopModel(Settings *settings, PlacesModel *places, AppRegistry *apps, QObject *parent)
    : DesktopModel(settings, places, apps, QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
                   defaultTrashDir(), defaultLayoutFile(), parent)
{
}

DesktopModel::DesktopModel(Settings *settings, PlacesModel *places, AppRegistry *apps, const QString &desktopDir,
                           const QString &trashDir, const QString &layoutFile, QObject *parent)
    : QAbstractListModel(parent)
    , m_settings(settings)
    , m_places(places)
    , m_apps(apps)
    , m_desktopDir(desktopDir)
    , m_trashDir(trashDir)
    , m_layoutFile(layoutFile)
    , m_watcher(new QFileSystemWatcher(this))
    , m_saveTimer(new QTimer(this))
    , m_reloadTimer(new QTimer(this))
{
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(500);
    connect(m_saveTimer, &QTimer::timeout, this, &DesktopModel::saveLayout);
    // Bursts of file changes (a copy of many files) reload once.
    m_reloadTimer->setSingleShot(true);
    m_reloadTimer->setInterval(150);
    connect(m_reloadTimer, &QTimer::timeout, this, &DesktopModel::reload);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &dir) {
        if (dir == m_trashDir)
            updateTrash();
        else
            m_reloadTimer->start();
    });
    if (m_settings) {
        connect(m_settings, &Settings::changed, this, [this] {
            Q_EMIT optionsChanged();
            m_reloadTimer->start(); // show* options
        });
    }
    if (QFileInfo(m_desktopDir).isDir())
        m_watcher->addPath(m_desktopDir);
    if (QFileInfo(m_trashDir).isDir())
        m_watcher->addPath(m_trashDir);
    loadLayout();
    updateTrash();
    reload();
}

DesktopModel::~DesktopModel()
{
    if (m_saveTimer->isActive())
        saveLayout();
}

QString DesktopModel::defaultLayoutFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/meadow/desktop-layout.json");
}

QString DesktopModel::defaultTrashDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash/files");
}

int DesktopModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QVariant DesktopModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const Item &it = m_items.at(index.row());
    switch (role) {
    case IdRole: return it.id;
    case Qt::DisplayRole:
    case NameRole: return it.name;
    case IconRole:
        if (it.id == kTrash)
            return m_trashFull ? QStringLiteral("user-trash-full") : QStringLiteral("user-trash");
        return it.icon;
    case PathRole: return it.path;
    case KindRole: return it.kind;
    case CellXRole: return it.pos.x();
    case CellYRole: return it.pos.y();
    case TrustedRole: return it.trusted;
    default: return {};
    }
}

QHash<int, QByteArray> DesktopModel::roleNames() const
{
    return {{IdRole, "itemId"}, {NameRole, "name"}, {IconRole, "iconName"}, {PathRole, "path"},
            {KindRole, "kind"}, {CellXRole, "cellX"}, {CellYRole, "cellY"}, {TrustedRole, "trusted"}};
}

bool DesktopModel::autoArrange() const
{
    return m_settings && m_settings->boolSetting(QStringLiteral("Desktop/autoArrange"), false);
}

void DesktopModel::setAutoArrange(bool on)
{
    if (m_settings)
        m_settings->set(QStringLiteral("Desktop/autoArrange"), on);
    if (on)
        arrange(sortBy());
}

bool DesktopModel::alignToGrid() const
{
    return !m_settings || m_settings->boolSetting(QStringLiteral("Desktop/alignToGrid"), true);
}

void DesktopModel::setAlignToGrid(bool on)
{
    if (m_settings)
        m_settings->set(QStringLiteral("Desktop/alignToGrid"), on);
    if (on) {
        // Snap everything that is between cells.
        QStringList ids;
        for (const Item &it : std::as_const(m_items))
            ids << it.id;
        moveItems(ids, 0, 0);
    }
}

QString DesktopModel::sortBy() const
{
    return m_settings ? m_settings->stringValue(QStringLiteral("Desktop/sortBy"), QStringLiteral("name")) : QStringLiteral("name");
}

void DesktopModel::setGridSize(int columns, int rows)
{
    columns = std::max(1, columns);
    rows = std::max(1, rows);
    if (columns == m_columns && rows == m_rows)
        return;
    m_columns = columns;
    m_rows = rows;
    Q_EMIT gridChanged();
    reload();
}

int DesktopModel::indexOf(const QString &id) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id)
            return i;
    }
    return -1;
}

QVariantMap DesktopModel::get(const QString &id) const
{
    const int i = indexOf(id);
    if (i < 0)
        return {};
    QVariantMap m;
    const QHash<int, QByteArray> names = roleNames();
    for (auto it = names.cbegin(); it != names.cend(); ++it)
        m.insert(QString::fromLatin1(it.value()), data(index(i), it.key()));
    return m;
}

DesktopModel::Item DesktopModel::makeFileItem(const QString &fileName) const
{
    static const QMimeDatabase mimeDb;
    Item it;
    it.id = kFilePrefix + fileName;
    it.path = m_desktopDir + QLatin1Char('/') + fileName;
    const QFileInfo fi(it.path);
    it.name = fileName;
    it.size = fi.isDir() ? 0 : fi.size();
    it.modified = fi.lastModified();
    if (fi.isDir()) {
        it.kind = QStringLiteral("folder");
        it.icon = QStringLiteral("folder");
        it.mime = QStringLiteral("inode/directory");
        return it;
    }
    const QMimeType mime = mimeDb.mimeTypeForFile(fi);
    it.mime = mime.name();
    it.kind = QStringLiteral("file");
    it.icon = mime.iconName();
    if (fileName.endsWith(QLatin1String(".desktop"))) {
        const DesktopEntry entry = DesktopEntry::parse(it.path, fileName);
        it.trusted = fi.isExecutable();
        if (entry.isValid() && it.trusted) {
            it.kind = QStringLiteral("launcher");
            it.name = entry.name;
            it.icon = entry.icon.isEmpty() ? QStringLiteral("application-x-executable") : entry.icon;
        }
    }
    return it;
}

void DesktopModel::reload()
{
    QList<Item> items;
    auto special = [&](const QString &id, const QString &key, const QString &name, const QString &icon) {
        if (m_settings && !m_settings->boolSetting(QStringLiteral("Desktop/") + key, true))
            return;
        Item it;
        it.id = id;
        it.name = name;
        it.icon = icon;
        it.kind = id.mid(7); // after "meadow:"
        items << it;
    };
    special(kDocuments, QStringLiteral("showMyDocuments"), tr("My Documents"), QStringLiteral("folder-documents"));
    special(kComputer, QStringLiteral("showMyComputer"), tr("My Computer"), QStringLiteral("computer"));
    special(kTrash, QStringLiteral("showRecycleBin"), tr("Recycle Bin"), QStringLiteral("user-trash"));

    if (QFileInfo(m_desktopDir).isDir()) {
        const QStringList names = QDir(m_desktopDir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &name : names)
            items << makeFileItem(name);
        if (!m_watcher->directories().contains(m_desktopDir))
            m_watcher->addPath(m_desktopDir);
    }

    // Keep known positions: from the current items, else the saved layout.
    QHash<QString, QPointF> current;
    for (const Item &it : std::as_const(m_items))
        current.insert(it.id, it.pos);
    for (Item &it : items) {
        if (current.contains(it.id) && current.value(it.id).x() >= 0)
            it.pos = current.value(it.id);
        else if (m_saved.contains(it.id))
            it.pos = m_saved.value(it.id);
    }

    beginResetModel();
    m_items = items;
    if (m_columns > 0) {
        if (autoArrange())
            arrange(sortBy());
        else
            placeNewItems();
    }
    endResetModel();
}

void DesktopModel::refresh()
{
    updateTrash();
    reload();
}

QPointF DesktopModel::clamp(const QPointF &p) const
{
    return {std::clamp(p.x(), 0.0, qreal(m_columns - 1)), std::clamp(p.y(), 0.0, qreal(m_rows - 1))};
}

bool DesktopModel::occupied(const QPoint &cell, const QSet<int> &ignore) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (ignore.contains(i) || m_items.at(i).pos.x() < 0)
            continue;
        const QPointF p = m_items.at(i).pos;
        if (qRound(p.x()) == cell.x() && qRound(p.y()) == cell.y())
            return true;
    }
    return false;
}

QPoint DesktopModel::firstFreeCell() const
{
    for (int x = 0; x < m_columns; ++x) {
        for (int y = 0; y < m_rows; ++y) {
            if (!occupied({x, y}, {}))
                return {x, y};
        }
    }
    return {0, 0}; // full desktop: stack on the first cell
}

QPoint DesktopModel::freeCellNear(const QPointF &target, const QSet<int> &ignore) const
{
    const QPointF t = clamp(target);
    QPoint best(qRound(t.x()), qRound(t.y()));
    if (!occupied(best, ignore))
        return best;
    qreal bestDist = -1;
    for (int x = 0; x < m_columns; ++x) {
        for (int y = 0; y < m_rows; ++y) {
            if (occupied({x, y}, ignore))
                continue;
            const qreal d = std::hypot(x - t.x(), y - t.y());
            if (bestDist < 0 || d < bestDist) {
                bestDist = d;
                best = {x, y};
            }
        }
    }
    return best;
}

void DesktopModel::placeNewItems()
{
    // Items whose cell is now off the grid (smaller screen) are placed again.
    for (Item &it : m_items) {
        if (it.pos.x() >= m_columns || it.pos.y() >= m_rows)
            it.pos = {-1, -1};
    }
    for (int i = 0; i < m_items.size(); ++i) {
        Item &it = m_items[i];
        if (it.pos.x() >= 0)
            continue;
        if (it.id == kTrash && !occupied({m_columns - 1, m_rows - 1}, {}))
            it.pos = QPointF(m_columns - 1, m_rows - 1);
        else
            it.pos = firstFreeCell();
        m_saved.insert(it.id, it.pos);
    }
    scheduleSave();
}

void DesktopModel::moveItems(const QStringList &ids, qreal dx, qreal dy)
{
    if (m_columns <= 0 || ids.isEmpty())
        return;
    if (autoArrange()) {
        // Auto Arrange keeps the sorted layout; a drag cannot change it.
        arrange(sortBy());
        return;
    }
    QSet<int> moving;
    for (const QString &id : ids) {
        const int i = indexOf(id);
        if (i >= 0)
            moving.insert(i);
    }
    // Place in reading order so a moved group keeps its shape where it can.
    QList<int> order(moving.cbegin(), moving.cend());
    std::sort(order.begin(), order.end(), [this](int a, int b) {
        const QPointF pa = m_items.at(a).pos, pb = m_items.at(b).pos;
        return pa.x() != pb.x() ? pa.x() < pb.x() : pa.y() < pb.y();
    });
    QSet<int> pending = moving;
    for (int i : order) {
        const QPointF target = clamp(m_items.at(i).pos + QPointF(dx, dy));
        pending.remove(i);
        QPointF pos;
        if (alignToGrid()) {
            pos = freeCellNear(target, pending + QSet<int>{i});
        } else {
            pos = target;
        }
        m_items[i].pos = pos;
        m_saved.insert(m_items.at(i).id, pos);
        const QModelIndex idx = index(i);
        Q_EMIT dataChanged(idx, idx, {CellXRole, CellYRole});
    }
    scheduleSave();
}

void DesktopModel::arrange(const QString &by)
{
    if (m_columns <= 0)
        return;
    if (m_settings && by != sortBy())
        m_settings->set(QStringLiteral("Desktop/sortBy"), by);
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    QList<int> order(m_items.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        const Item &ia = m_items.at(a), &ib = m_items.at(b);
        const int ra = specialRank(ia.id), rb = specialRank(ib.id);
        if (ra != rb)
            return ra < rb;
        if (ra < 3)
            return false;
        // Folders before files, as in Explorer.
        const bool da = ia.kind == QLatin1String("folder"), db = ib.kind == QLatin1String("folder");
        if (da != db)
            return da;
        if (by == QLatin1String("size") && ia.size != ib.size)
            return ia.size < ib.size;
        if (by == QLatin1String("type") && ia.mime != ib.mime)
            return ia.mime < ib.mime;
        if (by == QLatin1String("modified") && ia.modified != ib.modified)
            return ia.modified > ib.modified; // newest first
        return collator.compare(ia.name, ib.name) < 0;
    });
    int n = 0;
    for (int i : order) {
        const QPointF pos(std::min(n / m_rows, m_columns - 1), n % m_rows);
        m_items[i].pos = pos;
        m_saved.insert(m_items.at(i).id, pos);
        ++n;
    }
    if (!m_items.isEmpty())
        Q_EMIT dataChanged(index(0), index(int(m_items.size()) - 1), {CellXRole, CellYRole});
    scheduleSave();
}

bool DesktopModel::open(const QString &id)
{
    const int i = indexOf(id);
    if (i < 0)
        return false;
    const Item &it = m_items.at(i);
    qCInfo(lcDesktop) << "open" << it.kind;
    if (id == kComputer)
        return m_places && m_places->activate(QStringLiteral("computer"));
    if (id == kDocuments)
        return m_places && m_places->activate(QStringLiteral("documents"));
    if (id == kTrash)
        return Launcher::instance()->openFolder(QStringLiteral("trash:///"));
    if (it.kind == QLatin1String("folder"))
        return Launcher::instance()->openFolder(it.path);
    if (it.path.endsWith(QLatin1String(".desktop"))) {
        if (!it.trusted) {
            Q_EMIT untrustedLauncher(it.name);
            return false;
        }
        const DesktopEntry entry = DesktopEntry::parse(it.path, it.name);
        if (entry.type == QLatin1String("Link") || entry.exec.isEmpty())
            return false;
        QStringList argv = entry.execArguments();
        if (argv.isEmpty())
            return false;
        const QString program = argv.takeFirst();
        return Launcher::start(program, argv);
    }
    return Launcher::instance()->open(it.path);
}

QString DesktopModel::uniqueName(const QString &base, const QString &suffix) const
{
    QString name = base + suffix;
    for (int n = 2; QFileInfo::exists(m_desktopDir + QLatin1Char('/') + name); ++n)
        name = QStringLiteral("%1 (%2)%3").arg(base).arg(n).arg(suffix);
    return name;
}

QString DesktopModel::newFolder()
{
    if (!QDir().mkpath(m_desktopDir))
        return {};
    const QString name = uniqueName(tr("New Folder"), QString());
    if (!QDir(m_desktopDir).mkdir(name))
        return {};
    reload();
    return kFilePrefix + name;
}

QString DesktopModel::newTextDocument()
{
    if (!QDir().mkpath(m_desktopDir))
        return {};
    const QString name = uniqueName(tr("New Text Document"), QStringLiteral(".txt"));
    QFile f(m_desktopDir + QLatin1Char('/') + name);
    if (!f.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        return {};
    f.close();
    reload();
    return kFilePrefix + name;
}

QString DesktopModel::createShortcut(const QString &desktopId)
{
    if (!m_apps || !m_apps->hasDesktopId(desktopId) || !QDir().mkpath(m_desktopDir))
        return {};
    QString source;
    for (const QString &dir : AppRegistry::applicationDirs()) {
        const QString candidate = dir + QLatin1Char('/') + QString(desktopId).replace(QLatin1Char('-'), QLatin1Char('/'));
        const QString direct = dir + QLatin1Char('/') + desktopId;
        if (QFileInfo::exists(direct)) { source = direct; break; }
        if (QFileInfo::exists(candidate)) { source = candidate; break; }
    }
    if (source.isEmpty())
        return {};
    const QString base = desktopId.endsWith(QLatin1String(".desktop")) ? desktopId.chopped(8) : desktopId;
    const QString name = uniqueName(base, QStringLiteral(".desktop"));
    const QString target = m_desktopDir + QLatin1Char('/') + name;
    if (!QFile::copy(source, target))
        return {};
    // Created by the user from an installed application: trusted.
    QFile::setPermissions(target, QFile::permissions(target) | QFileDevice::ExeOwner | QFileDevice::WriteOwner);
    reload();
    return kFilePrefix + name;
}

bool DesktopModel::canRename(const QString &id) const
{
    return id.startsWith(kFilePrefix) && indexOf(id) >= 0;
}

bool DesktopModel::rename(const QString &id, const QString &newName)
{
    const int i = indexOf(id);
    const QString name = newName.trimmed();
    if (i < 0 || !canRename(id) || name.isEmpty() || name.contains(QLatin1Char('/'))
        || name == QLatin1String(".") || name == QLatin1String(".."))
        return false;
    const QString oldName = id.mid(kFilePrefix.size());
    if (name == oldName)
        return true;
    const QString target = m_desktopDir + QLatin1Char('/') + name;
    if (QFileInfo::exists(target))
        return false;
    if (!QFile::rename(m_desktopDir + QLatin1Char('/') + oldName, target))
        return false;
    // The icon stays where it was.
    const QPointF pos = m_items.at(i).pos;
    m_saved.remove(id);
    m_saved.insert(kFilePrefix + name, pos);
    m_items[i].id = kFilePrefix + name;
    m_items[i].path = target;
    reload();
    scheduleSave();
    return true;
}

int DesktopModel::moveToTrash(const QStringList &ids)
{
    int moved = 0;
    for (const QString &id : ids) {
        const int i = indexOf(id);
        if (i < 0 || !id.startsWith(kFilePrefix))
            continue;
        if (QFile::moveToTrash(m_items.at(i).path)) {
            ++moved;
            m_saved.remove(id);
        } else {
            qCWarning(lcDesktop) << "could not move a desktop item to the Recycle Bin";
        }
    }
    if (moved) {
        if (QFileInfo(m_trashDir).isDir() && !m_watcher->directories().contains(m_trashDir))
            m_watcher->addPath(m_trashDir);
        updateTrash();
        reload();
        scheduleSave();
    }
    return moved;
}

void DesktopModel::emptyTrash()
{
    // XDG trash: files/ holds the items, info/ their .trashinfo records.
    const QDir trashRoot = QFileInfo(m_trashDir).dir();
    for (const QString &sub : {QStringLiteral("files"), QStringLiteral("info")}) {
        QDir dir(trashRoot.filePath(sub));
        if (!dir.exists())
            continue;
        const QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QFileInfo &fi : entries) {
            if (fi.isDir() && !fi.isSymLink())
                QDir(fi.absoluteFilePath()).removeRecursively();
            else
                QFile::remove(fi.absoluteFilePath());
        }
    }
    qCInfo(lcDesktop) << "emptied the Recycle Bin";
    updateTrash();
}

QVariantMap DesktopModel::details(const QString &id) const
{
    const int i = indexOf(id);
    if (i < 0)
        return {};
    static const QMimeDatabase mimeDb;
    const Item &it = m_items.at(i);
    QVariantMap m{{QStringLiteral("name"), it.name}, {QStringLiteral("iconName"), data(index(i), IconRole)}};
    if (!it.path.isEmpty()) {
        const QFileInfo fi(it.path);
        m.insert(QStringLiteral("type"), it.kind == QLatin1String("folder") ? tr("File Folder") : mimeDb.mimeTypeForName(it.mime).comment());
        m.insert(QStringLiteral("location"), fi.absolutePath());
        m.insert(QStringLiteral("size"), it.size);
        m.insert(QStringLiteral("modified"), it.modified);
        m.insert(QStringLiteral("fileName"), fi.fileName());
    }
    return m;
}

void DesktopModel::updateTrash()
{
    const bool full = QFileInfo(m_trashDir).isDir()
        && !QDir(m_trashDir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System).isEmpty();
    if (full == m_trashFull)
        return;
    m_trashFull = full;
    Q_EMIT trashChanged();
    const int i = indexOf(kTrash);
    if (i >= 0)
        Q_EMIT dataChanged(index(i), index(i), {IconRole});
}

void DesktopModel::loadLayout()
{
    QFile f(m_layoutFile);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject positions = QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("positions")).toObject();
    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
        const QJsonArray a = it.value().toArray();
        if (a.size() == 2)
            m_saved.insert(it.key(), QPointF(a.at(0).toDouble(), a.at(1).toDouble()));
    }
}

void DesktopModel::scheduleSave()
{
    m_saveTimer->start();
}

void DesktopModel::saveLayout()
{
    // Only items that exist, so the file does not grow forever.
    QJsonObject positions;
    for (const Item &it : std::as_const(m_items)) {
        if (it.pos.x() >= 0)
            positions.insert(it.id, QJsonArray{it.pos.x(), it.pos.y()});
    }
    QDir().mkpath(QFileInfo(m_layoutFile).absolutePath());
    QSaveFile f(m_layoutFile);
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(QJsonDocument(QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("positions"), positions}}).toJson());
    f.commit();
}

QVariantList DesktopModel::wallpaperChoices() const
{
    QVariantList out;
    out << QVariantMap{{QStringLiteral("label"), tr("(None)")}, {QStringLiteral("value"), QString()}};
    out << QVariantMap{{QStringLiteral("label"), tr("Meadow Hills")}, {QStringLiteral("value"), QStringLiteral("meadow-hills.svg")}};
    QStringList dirs = ThemeLoader::overrideDirs();
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (!pictures.isEmpty())
        dirs << pictures;
    QSet<QString> seen;
    static const QStringList filters{QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
                                     QStringLiteral("*.svg"), QStringLiteral("*.bmp"), QStringLiteral("*.webp")};
    constexpr int kMaxPictures = 200;
    for (const QString &dir : std::as_const(dirs)) {
        const QFileInfoList files = QDir(dir).entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo &fi : files) {
            if (seen.size() >= kMaxPictures)
                return out;
            if (fi.fileName() == QLatin1String("meadow-hills.svg") || seen.contains(fi.canonicalFilePath()))
                continue;
            seen.insert(fi.canonicalFilePath());
            out << QVariantMap{{QStringLiteral("label"), fi.completeBaseName()}, {QStringLiteral("value"), fi.absoluteFilePath()}};
        }
    }
    return out;
}

bool DesktopModel::isImage(const QString &path)
{
    QImageReader reader(path);
    return QFileInfo(path).isReadable() && reader.canRead();
}
