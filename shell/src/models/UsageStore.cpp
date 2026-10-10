// SPDX-License-Identifier: GPL-3.0-or-later
#include "UsageStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(lcUsage, "meadow.shell.usage")

UsageStore::UsageStore(QObject *parent)
    : UsageStore(defaultFile(), parent)
{
}

UsageStore::UsageStore(const QString &file, QObject *parent)
    : QObject(parent)
    , m_file(file)
{
    load();
}

QString UsageStore::defaultFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/meadow/start-menu.json");
}

void UsageStore::load()
{
    QFile f(m_file);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    m_initialized = o.value(QStringLiteral("initialized")).toBool();
    const QJsonObject launches = o.value(QStringLiteral("launches")).toObject();
    for (auto it = launches.begin(); it != launches.end(); ++it) {
        const QJsonObject l = it.value().toObject();
        m_launches.insert(it.key(), {l.value(QStringLiteral("score")).toDouble(), qint64(l.value(QStringLiteral("last")).toDouble())});
    }
    if (o.contains(QStringLiteral("pinned"))) {
        m_pinnedSet = true;
        for (const auto &v : o.value(QStringLiteral("pinned")).toArray())
            m_pinned << v.toString();
    }
    for (const auto &v : o.value(QStringLiteral("removed")).toArray())
        m_removed << v.toString();
    const QJsonObject seen = o.value(QStringLiteral("seen")).toObject();
    for (auto it = seen.begin(); it != seen.end(); ++it)
        m_seen.insert(it.key(), qint64(it.value().toDouble()));
}

void UsageStore::save()
{
    QJsonObject launches;
    for (auto it = m_launches.cbegin(); it != m_launches.cend(); ++it)
        launches.insert(it.key(), QJsonObject{{QStringLiteral("score"), it->score}, {QStringLiteral("last"), double(it->last)}});
    QJsonObject seen;
    for (auto it = m_seen.cbegin(); it != m_seen.cend(); ++it)
        seen.insert(it.key(), double(it.value()));
    QJsonObject o{
        {QStringLiteral("version"), 1},
        {QStringLiteral("initialized"), m_initialized},
        {QStringLiteral("launches"), launches},
        {QStringLiteral("removed"), QJsonArray::fromStringList(m_removed)},
        {QStringLiteral("seen"), seen},
    };
    if (m_pinnedSet)
        o.insert(QStringLiteral("pinned"), QJsonArray::fromStringList(m_pinned));
    QDir().mkpath(QFileInfo(m_file).absolutePath());
    QSaveFile f(m_file);
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(o).toJson()) < 0 || !f.commit())
        qCWarning(lcUsage) << "cannot write" << m_file;
}

double UsageStore::score(const QString &id) const
{
    const auto it = m_launches.constFind(id);
    if (it == m_launches.cend())
        return 0;
    const double days = std::max<qint64>(0, now().toSecsSinceEpoch() - it->last) / 86400.0;
    return it->score * std::pow(0.5, days / kHalfLifeDays);
}

void UsageStore::recordLaunch(const QString &id)
{
    if (id.isEmpty())
        return;
    const double current = score(id);
    m_launches[id] = {current + 1.0, now().toSecsSinceEpoch()};
    m_removed.removeAll(id);
    save();
    Q_EMIT changed();
}

QStringList UsageStore::byScore() const
{
    QStringList ids = m_launches.keys();
    // Ties (equal scores) fall back to the name so the order is stable.
    std::sort(ids.begin(), ids.end(), [this](const QString &a, const QString &b) {
        const double sa = score(a), sb = score(b);
        return sa != sb ? sa > sb : a < b;
    });
    return ids;
}

void UsageStore::setPinned(const QStringList &ids)
{
    QStringList unique;
    for (const QString &id : ids) {
        if (!id.isEmpty() && !unique.contains(id))
            unique << id;
    }
    if (m_pinnedSet && unique == m_pinned)
        return;
    m_pinned = unique;
    m_pinnedSet = true;
    save();
    Q_EMIT changed();
}

void UsageStore::removeFromFrequent(const QString &id)
{
    m_launches.remove(id);
    if (!m_removed.contains(id))
        m_removed << id;
    save();
    Q_EMIT changed();
}

void UsageStore::noteInstalled(const QStringList &ids)
{
    const qint64 now = this->now().toSecsSinceEpoch();
    bool dirty = false;
    for (const QString &id : ids) {
        if (!m_seen.contains(id)) {
            // Before the first run everything counts as old.
            m_seen.insert(id, m_initialized ? now : 0);
            dirty = true;
        }
    }
    if (!m_initialized) {
        m_initialized = true;
        dirty = true;
    }
    if (dirty)
        save();
}

bool UsageStore::isNew(const QString &id, int days) const
{
    const qint64 seen = m_seen.value(id, 0);
    if (seen == 0 || m_launches.contains(id))
        return false; // old, or already used
    return now().toSecsSinceEpoch() - seen < qint64(days) * 86400;
}
