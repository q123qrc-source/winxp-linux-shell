// SPDX-License-Identifier: GPL-3.0-or-later
#include "NotificationServer.h"

#include <QDBusAbstractAdaptor>
#include <QDBusArgument>
#include <QDBusMetaType>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

Q_LOGGING_CATEGORY(lcNotify, "meadow.shell.notifications")

namespace {

// Spec "image-data": (iiibiiay) width, height, rowstride, has_alpha,
// bits_per_sample, channels, data.
QImage imageFromHint(const QVariant &value)
{
    if (!value.canConvert<QDBusArgument>())
        return {};
    const QDBusArgument arg = value.value<QDBusArgument>();
    int width = 0, height = 0, stride = 0, bits = 0, channels = 0;
    bool alpha = false;
    QByteArray data;
    arg.beginStructure();
    arg >> width >> height >> stride >> alpha >> bits >> channels >> data;
    arg.endStructure();
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024 || bits != 8 || (channels != 3 && channels != 4)
        || stride < width * channels || data.size() < qsizetype(stride) * (height - 1) + width * channels)
        return {};
    QImage image(width, height, QImage::Format_ARGB32);
    for (int y = 0; y < height; ++y) {
        const auto *src = reinterpret_cast<const uchar *>(data.constData()) + qsizetype(y) * stride;
        auto *dst = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < width; ++x, src += channels)
            dst[x] = qRgba(src[0], src[1], src[2], channels == 4 && alpha ? src[3] : 255);
    }
    return image;
}

// app_icon / image-path: an icon name, an absolute path or a file:// URI.
QString iconSource(const QString &icon)
{
    if (icon.isEmpty())
        return {};
    if (icon.startsWith(QLatin1String("file://")))
        return icon;
    if (icon.startsWith(QLatin1Char('/')))
        return QFileInfo::exists(icon) ? QUrl::fromLocalFile(icon).toString() : QString();
    return QStringLiteral("image://icon/") + QString::fromLatin1(QUrl::toPercentEncoding(icon));
}

} // namespace

// The D-Bus face of NotificationServer, kept separate so the model's own
// slots are not exported.
class NotificationsAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

public:
    explicit NotificationsAdaptor(NotificationServer *server)
        : QDBusAbstractAdaptor(server)
        , m_server(server)
    {
        setAutoRelaySignals(true);
    }

public Q_SLOTS:
    QStringList GetCapabilities()
    {
        return {QStringLiteral("actions"), QStringLiteral("body"), QStringLiteral("body-markup"),
                QStringLiteral("icon-static")};
    }
    uint Notify(const QString &appName, uint replacesId, const QString &appIcon, const QString &summary,
                const QString &body, const QStringList &actions, const QVariantMap &hints, int expireTimeout)
    {
        return m_server->notify(appName, replacesId, appIcon, summary, body, actions, hints, expireTimeout);
    }
    void CloseNotification(uint id) { m_server->close(id, NotificationServer::ClosedByCall); }
    QString GetServerInformation(QString &vendor, QString &version, QString &specVersion)
    {
        vendor = QStringLiteral("Meadow");
        version = QStringLiteral(MEADOW_VERSION);
        specVersion = QStringLiteral("1.2");
        return QStringLiteral("meadow-shell");
    }

Q_SIGNALS:
    void NotificationClosed(uint id, uint reason);
    void ActionInvoked(uint id, const QString &actionKey);

private:
    NotificationServer *m_server;
};

NotificationServer::NotificationServer(QObject *parent)
    : QAbstractListModel(parent)
    , m_bus(QString())
{
    new NotificationsAdaptor(this);
}

NotificationServer::~NotificationServer()
{
    if (m_registered) {
        m_bus.unregisterService(QString::fromLatin1(kService));
        m_bus.unregisterObject(QString::fromLatin1(kPath));
    }
}

bool NotificationServer::registerOn(QDBusConnection bus)
{
    if (!bus.isConnected() || !bus.registerObject(QString::fromLatin1(kPath), this))
        return false;
    if (!bus.registerService(QString::fromLatin1(kService))) {
        qCInfo(lcNotify) << "another notification daemon owns" << kService << "- balloons disabled";
        bus.unregisterObject(QString::fromLatin1(kPath));
        return false;
    }
    m_bus = bus;
    m_registered = true;
    Q_EMIT registeredChanged();
    return true;
}

int NotificationServer::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QVariant NotificationServer::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const Notification &n = m_items.at(index.row());
    switch (role) {
    case IdRole: return n.id;
    case AppNameRole: return n.appName;
    case IconSourceRole:
        return n.image.isNull() ? n.iconSource : QStringLiteral("image://notification/%1").arg(n.id);
    case SummaryRole: return n.summary;
    case BodyRole: return n.body;
    case ActionsRole: return n.actions;
    case UrgencyRole: return n.urgency;
    case DesktopEntryRole: return n.desktopEntry;
    }
    return {};
}

QHash<int, QByteArray> NotificationServer::roleNames() const
{
    return {{IdRole, "notificationId"}, {AppNameRole, "appName"}, {IconSourceRole, "iconSource"},
            {SummaryRole, "summary"}, {BodyRole, "body"}, {ActionsRole, "actions"}, {UrgencyRole, "urgency"},
            {DesktopEntryRole, "desktopEntry"}};
}

void NotificationServer::setDefaultTimeout(int ms)
{
    if (ms == m_defaultTimeout || ms <= 0)
        return;
    m_defaultTimeout = ms;
    Q_EMIT defaultTimeoutChanged();
}

uint NotificationServer::notify(const QString &appName, uint replacesId, const QString &appIcon,
                                const QString &summary, const QString &body, const QStringList &actions,
                                const QVariantMap &hints, int expireTimeout)
{
    Notification n;
    n.appName = appName;
    n.summary = summary;
    n.body = sanitizeBody(body);
    n.urgency = qBound(0, hints.value(QStringLiteral("urgency"), 1).toInt(), 2);
    for (int i = 0; i + 1 < actions.size(); i += 2) {
        // "default" is invoked by clicking the balloon itself, not a button.
        n.actions.append(QVariantMap{{QStringLiteral("key"), actions.at(i)},
                                     {QStringLiteral("label"), actions.at(i + 1)}});
    }
    for (const char *key : {"image-data", "image_data", "icon_data"}) {
        if (hints.contains(QLatin1String(key))) {
            n.image = imageFromHint(hints.value(QLatin1String(key)));
            break;
        }
    }
    const QString imagePath = hints.value(QStringLiteral("image-path"), hints.value(QStringLiteral("image_path"))).toString();
    n.iconSource = iconSource(imagePath.isEmpty() ? appIcon : imagePath);
    n.desktopEntry = hints.value(QStringLiteral("desktop-entry")).toString();

    const int existing = replacesId ? rowForId(replacesId) : -1;
    if (existing >= 0) {
        Notification &old = m_items[existing];
        delete old.timer;
        n.id = old.id;
        old = n;
        armTimer(old, expireTimeout);
        Q_EMIT dataChanged(index(existing), index(existing));
        if (existing == 0)
            Q_EMIT currentChanged();
    } else {
        n.id = m_nextId++;
        if (m_nextId == 0)
            m_nextId = 1;
        while (m_items.size() >= kMaxQueued)
            close(m_items.first().id, Expired);
        beginInsertRows({}, int(m_items.size()), int(m_items.size()));
        m_items.append(n);
        armTimer(m_items.last(), expireTimeout);
        endInsertRows();
        Q_EMIT countChanged();
        if (m_items.size() == 1)
            Q_EMIT currentChanged();
    }
    qCDebug(lcNotify) << "notification" << n.id << "from" << appName;
    return n.id;
}

uint NotificationServer::post(const QString &appName, const QString &summary, const QString &body,
                              const QString &icon, int timeout)
{
    return notify(appName, 0, icon, summary, body, {}, {}, timeout);
}

void NotificationServer::armTimer(Notification &n, int expireTimeout)
{
    n.timer = nullptr;
    // Critical notifications stay until dismissed, as do explicit 0 timeouts.
    if (expireTimeout == 0 || (expireTimeout < 0 && n.urgency == 2))
        return;
    const uint id = n.id;
    n.timer = new QTimer(this);
    n.timer->setSingleShot(true);
    n.timer->setInterval(expireTimeout < 0 ? m_defaultTimeout : expireTimeout);
    connect(n.timer, &QTimer::timeout, this, [this, id] { close(id, Expired); });
    n.timer->start();
}

void NotificationServer::invokeAction(uint id, const QString &key)
{
    if (rowForId(id) < 0)
        return;
    Q_EMIT ActionInvoked(id, key);
    close(id, Dismissed);
}

void NotificationServer::close(uint id, CloseReason reason)
{
    const int row = rowForId(id);
    if (row < 0)
        return;
    beginRemoveRows({}, row, row);
    delete m_items.takeAt(row).timer;
    endRemoveRows();
    Q_EMIT countChanged();
    if (row == 0)
        Q_EMIT currentChanged();
    Q_EMIT NotificationClosed(id, reason);
}

QVariantMap NotificationServer::current() const
{
    QVariantMap out;
    if (m_items.isEmpty())
        return out;
    const auto roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        out.insert(QString::fromUtf8(it.value()), data(index(0), it.key()));
    return out;
}

QImage NotificationServer::image(uint id) const
{
    const int row = rowForId(id);
    return row < 0 ? QImage() : m_items.at(row).image;
}

int NotificationServer::rowForId(uint id) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id)
            return i;
    }
    return -1;
}

QString NotificationServer::sanitizeBody(const QString &body)
{
    static const QRegularExpression token(QStringLiteral("<[^>]*>|&(#[0-9]+|#x[0-9a-fA-F]+|[a-zA-Z]+);|&|\n"));
    static const QRegularExpression altAttr(QStringLiteral("\\balt\\s*=\\s*(\"([^\"]*)\"|'([^']*)')"));
    static const QStringList keptEntities = {QStringLiteral("amp"), QStringLiteral("lt"), QStringLiteral("gt"),
                                             QStringLiteral("quot"), QStringLiteral("apos")};
    QString out;
    qsizetype pos = 0;
    auto it = token.globalMatch(body);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        out += body.mid(pos, m.capturedStart() - pos).toHtmlEscaped();
        pos = m.capturedEnd();
        const QString t = m.captured(0);
        if (t == QLatin1String("\n")) {
            out += QStringLiteral("<br/>");
        } else if (t == QLatin1String("&")) {
            out += QStringLiteral("&amp;");
        } else if (t.startsWith(QLatin1Char('&'))) {
            const QString name = m.captured(1);
            out += name.startsWith(QLatin1Char('#')) || keptEntities.contains(name) ? t : t.toHtmlEscaped();
        } else {
            // A tag: keep the few the spec allows, reduce the rest to text.
            static const QRegularExpression tagName(QStringLiteral("^<\\s*(/?)\\s*([a-zA-Z]+)"));
            const QRegularExpressionMatch tm = tagName.match(t);
            const QString name = tm.captured(2).toLower();
            const bool closing = !tm.captured(1).isEmpty();
            if (name == QLatin1String("b") || name == QLatin1String("i") || name == QLatin1String("u"))
                out += QStringLiteral("<%1%2>").arg(closing ? QStringLiteral("/") : QString(), name);
            else if (name == QLatin1String("br") && !closing)
                out += QStringLiteral("<br/>");
            else if (name == QLatin1String("img") && !closing) {
                const QRegularExpressionMatch am = altAttr.match(t);
                out += (am.hasCaptured(2) ? am.captured(2) : am.captured(3)).toHtmlEscaped();
            }
            // <a>, unknown tags and stray '<' are dropped; their text stays.
        }
    }
    out += body.mid(pos).toHtmlEscaped();
    return out;
}

NotificationImageProvider::NotificationImageProvider(NotificationServer *server)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_server(server)
{
}

QImage NotificationImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QImage image = m_server->image(id.section(QLatin1Char('?'), 0, 0).toUInt());
    if (!image.isNull() && requestedSize.isValid())
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (size)
        *size = image.size();
    return image;
}

#include "NotificationServer.moc"
