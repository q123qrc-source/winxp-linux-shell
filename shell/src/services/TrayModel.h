// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "TrayTypes.h"

#include <QAbstractListModel>
#include <QDBusConnection>
#include <QHash>
#include <QImage>
#include <QQuickImageProvider>

class StatusNotifierWatcher;
class DBusProperties;

// Notification-area icons: a StatusNotifierHost. Uses meadow-shell's own
// StatusNotifierWatcher when it runs one, otherwise the existing watcher.
// Icons are served to QML by TrayImageProvider ("image://tray/<key>").
class TrayModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        KeyRole = Qt::UserRole + 1,
        ItemIdRole,
        TitleRole,
        StatusRole,      // "Passive", "Active" or "NeedsAttention"
        IconSourceRole,  // QML image source
        ToolTipRole,
        HasMenuRole,
    };

    explicit TrayModel(QObject *parent = nullptr);
    ~TrayModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QImage image(const QString &key) const;

    Q_INVOKABLE void activate(const QString &key, int x, int y);
    Q_INVOKABLE void secondaryActivate(const QString &key, int x, int y);
    Q_INVOKABLE void scroll(const QString &key, int delta, bool horizontal);
    // Fetches the item's DBusMenu; answers with menuReady(key, entries) or,
    // for items without a menu, sends ContextMenu(x, y) to the item.
    Q_INVOKABLE void requestMenu(const QString &key, int x, int y);
    Q_INVOKABLE void triggerMenuItem(const QString &key, int id);
    Q_INVOKABLE void menuClosed(const QString &key);

Q_SIGNALS:
    void countChanged();
    void menuReady(const QString &key, const QVariantList &entries);

private Q_SLOTS:
    void addItem(const QString &key);
    void removeItem(const QString &key);

private:
    struct Item {
        QString key, service, path;
        DBusProperties *props = nullptr;
        QImage icon;
        int revision = 0;
    };

    void itemChanged(const QString &key);
    int rowForKey(const QString &key) const;
    void callItem(const QString &key, const QString &method, const QVariantList &args);

    QDBusConnection m_bus;
    StatusNotifierWatcher *m_watcher = nullptr;
    QList<Item> m_items;
};

class TrayImageProvider : public QQuickImageProvider
{
public:
    explicit TrayImageProvider(TrayModel *model);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    TrayModel *m_model;
};
