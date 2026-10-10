// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QVariantList>

class AppRegistry;
class Settings;
class UsageStore;

// The Start menu's left column: pinned items above the separator, the most
// frequently used programs below it.
//
// Pinned ids are desktop file ids or the placeholders meadow:internet and
// meadow:email, which show as "Internet" / "E-mail" with the default browser
// or mail client (mimeapps.list) as subtitle. Until the user pins or unpins
// anything, shell.conf [StartMenu] pinned applies.
//
// Entries: {id, desktopId, label, subtitle, iconName, pinned}.
class RecentAppsModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList pinned READ pinned NOTIFY changed)
    Q_PROPERTY(QVariantList frequent READ frequent NOTIFY changed)
    Q_PROPERTY(int frequentCount READ frequentCount WRITE setFrequentCount NOTIFY changed)

public:
    RecentAppsModel(AppRegistry *apps, UsageStore *usage, Settings *settings, QObject *parent = nullptr);

    QVariantList pinned() const { return m_pinned; }
    QVariantList frequent() const { return m_frequent; }
    int frequentCount() const { return m_frequentCount; }
    void setFrequentCount(int n);

    Q_INVOKABLE bool activate(const QString &id);
    Q_INVOKABLE bool isPinned(const QString &id) const;
    Q_INVOKABLE void pin(const QString &id, int index = -1);
    Q_INVOKABLE void unpin(const QString &id);
    Q_INVOKABLE void removeFromFrequent(const QString &id);

    // Desktop id of the default handler for a URL scheme ("http", "mailto"),
    // from mimeapps.list, else the first installed app declaring it.
    QString defaultHandler(const QString &scheme) const;
    // The desktop id a pinned id stands for (empty if not installed).
    Q_INVOKABLE QString resolve(const QString &id) const;

Q_SIGNALS:
    void changed();

private:
    void rebuild();
    QStringList pinnedIds() const;
    QVariantMap entryFor(const QString &id, bool pinned) const;

    AppRegistry *m_apps;
    UsageStore *m_usage;
    Settings *m_settings;
    int m_frequentCount = 6;
    QVariantList m_pinned;
    QVariantList m_frequent;
};
