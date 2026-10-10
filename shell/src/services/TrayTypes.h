// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusArgument>
#include <QImage>
#include <QList>
#include <QString>
#include <QVariantMap>

// D-Bus types of the StatusNotifierItem and com.canonical.dbusmenu specs.

// a(iiay): ARGB32 pixels in network byte order.
struct SniIconPixmap {
    int width = 0;
    int height = 0;
    QByteArray bytes;
    QImage toImage() const;
};
using SniIconPixmapList = QList<SniIconPixmap>;

// (sa(iiay)ss): icon name, icon pixmaps, title, text.
struct SniToolTip {
    QString iconName;
    SniIconPixmapList image;
    QString title;
    QString text;
};

// (ia{sv}av): one dbusmenu node and its children.
struct DBusMenuItem {
    int id = 0;
    QVariantMap properties;
    QList<DBusMenuItem> children;
};

const QDBusArgument &operator>>(const QDBusArgument &arg, SniIconPixmap &icon);
QDBusArgument &operator<<(QDBusArgument &arg, const SniIconPixmap &icon);
const QDBusArgument &operator>>(const QDBusArgument &arg, SniToolTip &tip);
QDBusArgument &operator<<(QDBusArgument &arg, const SniToolTip &tip);
const QDBusArgument &operator>>(const QDBusArgument &arg, DBusMenuItem &item);
QDBusArgument &operator<<(QDBusArgument &arg, const DBusMenuItem &item);

Q_DECLARE_METATYPE(SniIconPixmap)
Q_DECLARE_METATYPE(SniToolTip)
Q_DECLARE_METATYPE(DBusMenuItem)

void registerTrayTypes();
// Picks the pixmap closest to (at least) `size` and converts it.
QImage bestIcon(const SniIconPixmapList &list, int size);
// dbusmenu node -> plain data for QML: {id, label, enabled, visible,
// separator, toggleType, checked, iconName, children}.
QVariantList menuToVariant(const DBusMenuItem &root);
// "_File" -> "File", "__" -> "_" (dbusmenu mnemonics).
QString stripMnemonic(const QString &label);
