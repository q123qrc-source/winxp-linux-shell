// SPDX-License-Identifier: GPL-3.0-or-later
#include "TrayTypes.h"

#include <QDBusMetaType>
#include <QDBusVariant>
#include <QtEndian>

QImage SniIconPixmap::toImage() const
{
    if (width <= 0 || height <= 0 || bytes.size() < width * height * 4)
        return {};
    QImage image(width, height, QImage::Format_ARGB32);
    const auto *src = reinterpret_cast<const quint32 *>(bytes.constData());
    for (int y = 0; y < height; ++y) {
        auto *line = reinterpret_cast<quint32 *>(image.scanLine(y));
        for (int x = 0; x < width; ++x)
            line[x] = qFromBigEndian(src[y * width + x]);
    }
    return image;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, SniIconPixmap &icon)
{
    arg.beginStructure();
    arg >> icon.width >> icon.height >> icon.bytes;
    arg.endStructure();
    return arg;
}

QDBusArgument &operator<<(QDBusArgument &arg, const SniIconPixmap &icon)
{
    arg.beginStructure();
    arg << icon.width << icon.height << icon.bytes;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, SniToolTip &tip)
{
    arg.beginStructure();
    arg >> tip.iconName >> tip.image >> tip.title >> tip.text;
    arg.endStructure();
    return arg;
}

QDBusArgument &operator<<(QDBusArgument &arg, const SniToolTip &tip)
{
    arg.beginStructure();
    arg << tip.iconName << tip.image << tip.title << tip.text;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, DBusMenuItem &item)
{
    arg.beginStructure();
    arg >> item.id >> item.properties;
    arg.beginArray();
    while (!arg.atEnd()) {
        QDBusVariant child;
        arg >> child;
        DBusMenuItem childItem;
        child.variant().value<QDBusArgument>() >> childItem;
        item.children.append(childItem);
    }
    arg.endArray();
    arg.endStructure();
    return arg;
}

QDBusArgument &operator<<(QDBusArgument &arg, const DBusMenuItem &item)
{
    arg.beginStructure();
    arg << item.id << item.properties;
    arg.beginArray(qMetaTypeId<QDBusVariant>());
    for (const DBusMenuItem &child : item.children)
        arg << QDBusVariant(QVariant::fromValue(child));
    arg.endArray();
    arg.endStructure();
    return arg;
}

void registerTrayTypes()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    qDBusRegisterMetaType<SniIconPixmap>();
    qDBusRegisterMetaType<SniIconPixmapList>();
    qDBusRegisterMetaType<SniToolTip>();
    qDBusRegisterMetaType<DBusMenuItem>();
}

QImage bestIcon(const SniIconPixmapList &list, int size)
{
    const SniIconPixmap *best = nullptr;
    for (const SniIconPixmap &p : list) {
        if (!best)
            best = &p;
        else if (best->width < size ? p.width > best->width : (p.width >= size && p.width < best->width))
            best = &p;
    }
    return best ? best->toImage() : QImage();
}

QString stripMnemonic(const QString &label)
{
    QString out;
    for (int i = 0; i < label.size(); ++i) {
        if (label.at(i) == QLatin1Char('_')) {
            if (i + 1 < label.size() && label.at(i + 1) == QLatin1Char('_'))
                out += QLatin1Char('_'), ++i;
            continue;
        }
        out += label.at(i);
    }
    return out;
}

QVariantList menuToVariant(const DBusMenuItem &root)
{
    QVariantList out;
    for (const DBusMenuItem &item : root.children) {
        const QVariantMap &p = item.properties;
        QVariantMap m;
        m.insert(QStringLiteral("id"), item.id);
        m.insert(QStringLiteral("label"), stripMnemonic(p.value(QStringLiteral("label")).toString()));
        m.insert(QStringLiteral("enabled"), p.value(QStringLiteral("enabled"), true).toBool());
        m.insert(QStringLiteral("visible"), p.value(QStringLiteral("visible"), true).toBool());
        m.insert(QStringLiteral("separator"), p.value(QStringLiteral("type")).toString() == QLatin1String("separator"));
        m.insert(QStringLiteral("toggleType"), p.value(QStringLiteral("toggle-type")).toString());
        m.insert(QStringLiteral("checked"), p.value(QStringLiteral("toggle-state")).toInt() == 1);
        m.insert(QStringLiteral("iconName"), p.value(QStringLiteral("icon-name")).toString());
        m.insert(QStringLiteral("children"), menuToVariant(item));
        out << m;
    }
    return out;
}
