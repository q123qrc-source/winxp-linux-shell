// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QDBusConnection>
#include <QHash>
#include <QImage>
#include <QQuickImageProvider>
#include <QStringList>
#include <QVariantMap>

class QTimer;

// org.freedesktop.Notifications (spec 1.2) server. Pending notifications form
// a queue that QML shows one at a time as balloons over the notification
// area, like XP. Notification text is never logged: it can carry secrets
// (one-time codes, message previews).
class NotificationServer : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    // Used when a client asks for the server default (expire_timeout -1).
    Q_PROPERTY(int defaultTimeout READ defaultTimeout WRITE setDefaultTimeout NOTIFY defaultTimeoutChanged)
    Q_PROPERTY(bool registered READ isRegistered NOTIFY registeredChanged)
    // The notification on screen (the oldest pending one), by role name;
    // empty when none.
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        AppNameRole,
        IconSourceRole,  // QML image source, or empty
        SummaryRole,
        BodyRole,        // sanitized Qt StyledText
        ActionsRole,     // list of {key, label}
        UrgencyRole,     // 0 low, 1 normal, 2 critical
        DesktopEntryRole, // "desktop-entry" hint, for an icon fallback
    };

    enum CloseReason : uint {
        Expired = 1,
        Dismissed = 2,
        ClosedByCall = 3,
        Undefined = 4,
    };

    static constexpr const char *kService = "org.freedesktop.Notifications";
    static constexpr const char *kPath = "/org/freedesktop/Notifications";
    static constexpr int kMaxQueued = 20;

    explicit NotificationServer(QObject *parent = nullptr);
    ~NotificationServer() override;

    // Claims org.freedesktop.Notifications; false when another daemon runs.
    bool registerOn(QDBusConnection bus);
    bool isRegistered() const { return m_registered; }

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int defaultTimeout() const { return m_defaultTimeout; }
    void setDefaultTimeout(int ms);

    // The D-Bus Notify call; also used by preview mode and tests.
    uint notify(const QString &appName, uint replacesId, const QString &appIcon, const QString &summary,
                const QString &body, const QStringList &actions, const QVariantMap &hints, int expireTimeout);
    Q_INVOKABLE uint post(const QString &appName, const QString &summary, const QString &body,
                          const QString &icon = {}, int timeout = -1);
    Q_INVOKABLE void dismiss(uint id) { close(id, Dismissed); }
    Q_INVOKABLE void invokeAction(uint id, const QString &key);
    void close(uint id, CloseReason reason);

    QImage image(uint id) const;
    QVariantMap current() const;

    // Reduces body markup to what the spec allows (b, i, u, a, img, br) in a
    // form Qt's StyledText renders; links and images become plain text.
    static QString sanitizeBody(const QString &body);

Q_SIGNALS:
    void countChanged();
    void defaultTimeoutChanged();
    void registeredChanged();
    void currentChanged();
    void NotificationClosed(uint id, uint reason);
    void ActionInvoked(uint id, const QString &actionKey);

private:
    struct Notification {
        uint id = 0;
        QString appName, iconSource, summary, body, desktopEntry;
        QVariantList actions;
        int urgency = 1;
        QImage image;
        QTimer *timer = nullptr;
    };

    int rowForId(uint id) const;
    void armTimer(Notification &n, int expireTimeout);

    QList<Notification> m_items;
    uint m_nextId = 1;
    int m_defaultTimeout = 10000;
    bool m_registered = false;
    QDBusConnection m_bus;
};

class NotificationImageProvider : public QQuickImageProvider
{
public:
    explicit NotificationImageProvider(NotificationServer *server);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    NotificationServer *m_server;
};
