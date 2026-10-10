// SPDX-License-Identifier: GPL-3.0-or-later
#include "TrayModel.h"
#include "DBusProperties.h"
#include "StatusNotifierWatcher.h"

#include <QCoreApplication>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusObjectPath>
#include <QIcon>
#include <QUrl>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcSniWatcher)

namespace {
const QString kItemInterface = QStringLiteral("org.kde.StatusNotifierItem");
const QString kMenuInterface = QStringLiteral("com.canonical.dbusmenu");
constexpr int kIconSize = 32;
}

TrayModel::TrayModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_bus(QDBusConnection::sessionBus())
{
    registerTrayTypes();
    if (!m_bus.isConnected())
        return;

    auto *watcher = new StatusNotifierWatcher(this);
    if (watcher->registerOnSessionBus()) {
        m_watcher = watcher;
        connect(watcher, &StatusNotifierWatcher::StatusNotifierItemRegistered, this, &TrayModel::addItem);
        connect(watcher, &StatusNotifierWatcher::StatusNotifierItemUnregistered, this, &TrayModel::removeItem);
        watcher->RegisterStatusNotifierHost(QStringLiteral("org.kde.StatusNotifierHost-%1").arg(QCoreApplication::applicationPid()));
        return;
    }
    // Someone else runs the watcher: be a host of theirs.
    delete watcher;
    const QString service = QString::fromLatin1(StatusNotifierWatcher::kService);
    const QString path = QString::fromLatin1(StatusNotifierWatcher::kPath);
    m_bus.connect(service, path, service, QStringLiteral("StatusNotifierItemRegistered"), this, SLOT(addItem(QString)));
    m_bus.connect(service, path, service, QStringLiteral("StatusNotifierItemUnregistered"), this, SLOT(removeItem(QString)));
    const QString host = QStringLiteral("org.kde.StatusNotifierHost-%1").arg(QCoreApplication::applicationPid());
    m_bus.registerService(host);
    QDBusMessage reg = QDBusMessage::createMethodCall(service, path, service, QStringLiteral("RegisterStatusNotifierHost"));
    reg << host;
    m_bus.asyncCall(reg);
    QDBusMessage get = QDBusMessage::createMethodCall(service, path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    get << service << QStringLiteral("RegisteredStatusNotifierItems");
    auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(get), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QDBusVariant> reply = *w;
        if (!reply.isError())
            for (const QString &key : reply.value().variant().toStringList())
                addItem(key);
    });
}

TrayModel::~TrayModel() = default;

int TrayModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

int TrayModel::rowForKey(const QString &key) const
{
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items.at(i).key == key)
            return i;
    return -1;
}

void TrayModel::addItem(const QString &key)
{
    if (rowForKey(key) >= 0)
        return;
    const qsizetype slash = key.indexOf(QLatin1Char('/'));
    Item item;
    item.key = key;
    item.service = slash > 0 ? key.left(slash) : key;
    item.path = slash > 0 ? key.mid(slash) : QStringLiteral("/StatusNotifierItem");
    item.props = new DBusProperties(m_bus, item.service, item.path, kItemInterface, this);
    connect(item.props, &DBusProperties::fetched, this, [this, key] { itemChanged(key); });
    connect(item.props, &DBusProperties::changed, this, [this, key] { itemChanged(key); });
    // Items announce changes with signals instead of PropertiesChanged.
    for (const char *sig : {"NewTitle", "NewIcon", "NewAttentionIcon", "NewOverlayIcon", "NewToolTip", "NewStatus"})
        m_bus.connect(item.service, item.path, kItemInterface, QString::fromLatin1(sig), item.props, SLOT(fetch()));
    beginInsertRows({}, int(m_items.size()), int(m_items.size()));
    m_items.append(item);
    endInsertRows();
    Q_EMIT countChanged();
}

void TrayModel::removeItem(const QString &key)
{
    const int row = rowForKey(key);
    if (row < 0)
        return;
    beginRemoveRows({}, row, row);
    m_items.at(row).props->deleteLater();
    m_items.removeAt(row);
    endRemoveRows();
    Q_EMIT countChanged();
}

void TrayModel::itemChanged(const QString &key)
{
    const int row = rowForKey(key);
    if (row < 0)
        return;
    Item &item = m_items[row];
    const QVariantMap v = item.props->values();
    const bool attention = v.value(QStringLiteral("Status")).toString() == QLatin1String("NeedsAttention");
    // Prefer the pixmap the item sends; else its theme icon (optionally
    // from the item's own IconThemePath).
    const QString nameKey = attention && !v.value(QStringLiteral("AttentionIconName")).toString().isEmpty()
        ? QStringLiteral("AttentionIconName") : QStringLiteral("IconName");
    const QString pixKey = attention ? QStringLiteral("AttentionIconPixmap") : QStringLiteral("IconPixmap");
    QImage image = bestIcon(qdbus_cast<SniIconPixmapList>(v.value(pixKey)), kIconSize);
    const QString iconName = v.value(nameKey).toString();
    if (!iconName.isEmpty()) {
        const QString themePath = v.value(QStringLiteral("IconThemePath")).toString();
        if (!themePath.isEmpty() && !QIcon::themeSearchPaths().contains(themePath))
            QIcon::setThemeSearchPaths(QIcon::themeSearchPaths() << themePath);
        const QIcon icon = QIcon::fromTheme(iconName);
        if (!icon.isNull())
            image = icon.pixmap(kIconSize, kIconSize).toImage();
    }
    item.icon = image;
    ++item.revision;
    const QModelIndex idx = index(row);
    Q_EMIT dataChanged(idx, idx);
}

QVariant TrayModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const Item &item = m_items.at(index.row());
    const QVariantMap v = item.props->values();
    switch (role) {
    case KeyRole: return item.key;
    case ItemIdRole: return v.value(QStringLiteral("Id"));
    case TitleRole: return v.value(QStringLiteral("Title"));
    case StatusRole: return v.value(QStringLiteral("Status"), QStringLiteral("Active"));
    case IconSourceRole:
        return item.icon.isNull() ? QString()
                                  : QStringLiteral("image://tray/%1?%2").arg(QString::fromLatin1(QUrl::toPercentEncoding(item.key)), QString::number(item.revision));
    case ToolTipRole: {
        const SniToolTip tip = qdbus_cast<SniToolTip>(v.value(QStringLiteral("ToolTip")));
        const QString title = tip.title.isEmpty() ? v.value(QStringLiteral("Title")).toString() : tip.title;
        return tip.text.isEmpty() ? title : title + QLatin1Char('\n') + tip.text;
    }
    case HasMenuRole: {
        const QString menu = qdbus_cast<QDBusObjectPath>(v.value(QStringLiteral("Menu"))).path();
        return !menu.isEmpty() && menu != QLatin1String("/");
    }
    default: return {};
    }
}

QHash<int, QByteArray> TrayModel::roleNames() const
{
    return {{KeyRole, "key"}, {ItemIdRole, "itemId"}, {TitleRole, "title"}, {StatusRole, "status"},
            {IconSourceRole, "iconSource"}, {ToolTipRole, "toolTip"}, {HasMenuRole, "hasMenu"}};
}

QImage TrayModel::image(const QString &key) const
{
    const int row = rowForKey(key);
    return row < 0 ? QImage() : m_items.at(row).icon;
}

void TrayModel::callItem(const QString &key, const QString &method, const QVariantList &args)
{
    const int row = rowForKey(key);
    if (row < 0)
        return;
    QDBusMessage msg = QDBusMessage::createMethodCall(m_items.at(row).service, m_items.at(row).path, kItemInterface, method);
    msg.setArguments(args);
    m_bus.asyncCall(msg);
}

void TrayModel::activate(const QString &key, int x, int y)
{
    const int row = rowForKey(key);
    // ItemIsMenu items want their menu on a primary click.
    if (row >= 0 && m_items.at(row).props->value(QStringLiteral("ItemIsMenu")).toBool() && data(index(row), HasMenuRole).toBool()) {
        requestMenu(key, x, y);
        return;
    }
    callItem(key, QStringLiteral("Activate"), {x, y});
}

void TrayModel::secondaryActivate(const QString &key, int x, int y)
{
    callItem(key, QStringLiteral("SecondaryActivate"), {x, y});
}

void TrayModel::scroll(const QString &key, int delta, bool horizontal)
{
    callItem(key, QStringLiteral("Scroll"), {delta, horizontal ? QStringLiteral("horizontal") : QStringLiteral("vertical")});
}

void TrayModel::requestMenu(const QString &key, int x, int y)
{
    const int row = rowForKey(key);
    if (row < 0)
        return;
    const QString menuPath = qdbus_cast<QDBusObjectPath>(m_items.at(row).props->value(QStringLiteral("Menu"))).path();
    if (menuPath.isEmpty() || menuPath == QLatin1String("/")) {
        callItem(key, QStringLiteral("ContextMenu"), {x, y});
        return;
    }
    const QString service = m_items.at(row).service;
    QDBusMessage about = QDBusMessage::createMethodCall(service, menuPath, kMenuInterface, QStringLiteral("AboutToShow"));
    about << 0;
    m_bus.asyncCall(about);
    QDBusMessage layout = QDBusMessage::createMethodCall(service, menuPath, kMenuInterface, QStringLiteral("GetLayout"));
    layout << 0 << -1 << QStringList();
    auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(layout, 3000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, key](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusMessage reply = w->reply();
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() < 2) {
            qCWarning(lcSniWatcher) << "menu of" << key << "unavailable:" << reply.errorMessage();
            return;
        }
        DBusMenuItem root;
        reply.arguments().at(1).value<QDBusArgument>() >> root;
        Q_EMIT menuReady(key, menuToVariant(root));
    });
}

void TrayModel::triggerMenuItem(const QString &key, int id)
{
    const int row = rowForKey(key);
    if (row < 0)
        return;
    const QString menuPath = qdbus_cast<QDBusObjectPath>(m_items.at(row).props->value(QStringLiteral("Menu"))).path();
    QDBusMessage msg = QDBusMessage::createMethodCall(m_items.at(row).service, menuPath, kMenuInterface, QStringLiteral("Event"));
    msg << id << QStringLiteral("clicked") << QVariant::fromValue(QDBusVariant(QString())) << uint(0);
    m_bus.asyncCall(msg);
}

void TrayModel::menuClosed(const QString &key)
{
    const int row = rowForKey(key);
    if (row < 0)
        return;
    const QString menuPath = qdbus_cast<QDBusObjectPath>(m_items.at(row).props->value(QStringLiteral("Menu"))).path();
    if (menuPath.isEmpty())
        return;
    QDBusMessage msg = QDBusMessage::createMethodCall(m_items.at(row).service, menuPath, kMenuInterface, QStringLiteral("Event"));
    msg << 0 << QStringLiteral("closed") << QVariant::fromValue(QDBusVariant(QString())) << uint(0);
    m_bus.asyncCall(msg);
}

TrayImageProvider::TrayImageProvider(TrayModel *model)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_model(model)
{
}

QImage TrayImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString key = QUrl::fromPercentEncoding(id.section(QLatin1Char('?'), 0, 0).toUtf8());
    QImage img = m_model->image(key);
    if (!img.isNull() && requestedSize.isValid() && !requestedSize.isEmpty())
        img = img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (size)
        *size = img.size();
    return img;
}
