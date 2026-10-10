// SPDX-License-Identifier: GPL-3.0-or-later
#include "RecentAppsModel.h"

#include "AppRegistry.h"
#include "UsageStore.h"
#include "services/Settings.h"

#include <QFile>
#include <QSettings>
#include <QStandardPaths>

namespace {
const QString kInternet = QStringLiteral("meadow:internet");
const QString kEmail = QStringLiteral("meadow:email");

// mimeapps.list search order (XDG MIME Applications spec): config dirs,
// then the applications/ folders of the data dirs.
QStringList mimeappsFiles()
{
    QStringList files;
    for (const QString &dir : QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation))
        files << dir + QLatin1String("/mimeapps.list");
    for (const QString &dir : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
        files << dir + QLatin1String("/applications/mimeapps.list");
    return files;
}
} // namespace

RecentAppsModel::RecentAppsModel(AppRegistry *apps, UsageStore *usage, Settings *settings, QObject *parent)
    : QObject(parent)
    , m_apps(apps)
    , m_usage(usage)
    , m_settings(settings)
{
    connect(m_apps, &AppRegistry::changed, this, &RecentAppsModel::rebuild);
    connect(m_apps, &AppRegistry::launched, m_usage, &UsageStore::recordLaunch);
    connect(m_usage, &UsageStore::changed, this, &RecentAppsModel::rebuild);
    connect(m_settings, &Settings::changed, this, &RecentAppsModel::rebuild);
    rebuild();
}

void RecentAppsModel::setFrequentCount(int n)
{
    if (n == m_frequentCount)
        return;
    m_frequentCount = n;
    rebuild();
}

QString RecentAppsModel::defaultHandler(const QString &scheme) const
{
    const QString mime = QStringLiteral("x-scheme-handler/") + scheme;
    for (const QString &file : mimeappsFiles()) {
        if (!QFile::exists(file))
            continue;
        QSettings ini(file, QSettings::IniFormat);
        QVariant v = ini.value(QStringLiteral("Default Applications/") + mime);
        const QStringList ids = v.typeId() == QMetaType::QStringList ? v.toStringList()
                                                                     : v.toString().split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString &id : ids) {
            if (m_apps->isShown(id.trimmed()))
                return id.trimmed();
        }
    }
    QStringList candidates;
    for (auto it = m_apps->entries().cbegin(); it != m_apps->entries().cend(); ++it) {
        if (it->mimeTypes.contains(mime) && m_apps->isShown(it.key()))
            candidates << it.key();
    }
    candidates.sort();
    return candidates.value(0);
}

QString RecentAppsModel::resolve(const QString &id) const
{
    if (id == kInternet)
        return defaultHandler(QStringLiteral("http"));
    if (id == kEmail)
        return defaultHandler(QStringLiteral("mailto"));
    return m_apps->isShown(id) ? id : QString();
}

QStringList RecentAppsModel::pinnedIds() const
{
    return m_usage->hasPinned() ? m_usage->pinned() : m_settings->listValue(QStringLiteral("StartMenu/pinned"));
}

QVariantMap RecentAppsModel::entryFor(const QString &id, bool pinned) const
{
    const QString desktopId = resolve(id);
    if (desktopId.isEmpty())
        return {};
    const DesktopEntry e = m_apps->entry(desktopId);
    QString label = e.name, subtitle;
    if (id == kInternet || id == kEmail) {
        label = id == kInternet ? tr("Internet") : tr("E-mail");
        subtitle = e.name;
    }
    return {{QStringLiteral("id"), id},
            {QStringLiteral("desktopId"), desktopId},
            {QStringLiteral("label"), label},
            {QStringLiteral("subtitle"), subtitle},
            {QStringLiteral("iconName"), e.icon},
            {QStringLiteral("pinned"), pinned}};
}

void RecentAppsModel::rebuild()
{
    QVariantList pinned, frequent;
    QStringList shownIds;
    for (const QString &id : pinnedIds()) {
        const QVariantMap e = entryFor(id, true);
        if (e.isEmpty())
            continue;
        pinned << e;
        shownIds << e.value(QStringLiteral("desktopId")).toString();
    }
    QStringList candidates = m_usage->byScore();
    // Until usage fills the list, suggest the packaged defaults.
    for (const QString &id : m_settings->listValue(QStringLiteral("StartMenu/defaultFrequent"))) {
        if (!candidates.contains(id) && !m_usage->isRemoved(id))
            candidates << id;
    }
    for (const QString &id : std::as_const(candidates)) {
        if (frequent.size() >= m_frequentCount)
            break;
        if (shownIds.contains(id))
            continue;
        const QVariantMap e = entryFor(id, false);
        if (e.isEmpty())
            continue;
        frequent << e;
        shownIds << id;
    }
    if (pinned != m_pinned || frequent != m_frequent) {
        m_pinned = pinned;
        m_frequent = frequent;
        Q_EMIT changed();
    }
}

bool RecentAppsModel::activate(const QString &id)
{
    const QString desktopId = resolve(id);
    return !desktopId.isEmpty() && m_apps->launch(desktopId);
}

bool RecentAppsModel::isPinned(const QString &id) const
{
    const QStringList ids = pinnedIds();
    if (ids.contains(id))
        return true;
    // A desktop id is also "pinned" through meadow:internet / meadow:email.
    return std::any_of(ids.cbegin(), ids.cend(), [&](const QString &p) { return p.startsWith(QLatin1String("meadow:")) && resolve(p) == id; });
}

void RecentAppsModel::pin(const QString &id, int index)
{
    QStringList ids = pinnedIds();
    const int from = int(ids.indexOf(id));
    if (from >= 0) {
        // Re-pinning moves it (drag and drop reordering).
        ids.removeAt(from);
        if (index > from)
            --index;
    }
    if (index < 0 || index > ids.size())
        index = int(ids.size());
    ids.insert(index, id);
    m_usage->setPinned(ids);
}

void RecentAppsModel::unpin(const QString &id)
{
    QStringList ids = pinnedIds();
    ids.removeAll(id);
    m_usage->setPinned(ids);
}

void RecentAppsModel::removeFromFrequent(const QString &id)
{
    m_usage->removeFromFrequent(id);
}
