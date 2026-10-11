// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

// Keyboard layouts for the language bar.
//
// labwc has no IPC to query or switch the active layout, so the shell owns
// the order: the first entry of XKB_DEFAULT_LAYOUT is the active one.
// Switching rewrites that order in an environment file inside the session's
// runtime labwc config overlay (MEADOW_LABWC_ENV_FILE, set by meadow-session)
// and asks labwc to reconfigure, which reloads the keymap. Alt+Shift is bound
// to `meadow-shellctl next-layout`, so the indicator always matches.
class KeyboardLayoutService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList layouts READ layouts NOTIFY changed)
    Q_PROPERTY(QString currentShortName READ currentShortName NOTIFY changed)
    Q_PROPERTY(QString currentName READ currentName NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)

public:
    struct Layout {
        QString layout;   // xkb layout, e.g. "us"
        QString variant;  // xkb variant, may be empty
        QString shortName; // language code shown in the bar, e.g. "EN"
        QString name;      // "English (US)"
    };

    explicit KeyboardLayoutService(QObject *parent = nullptr);
    // For tests: explicit layout/variant lists and xkb rules file.
    KeyboardLayoutService(const QString &layouts, const QString &variants, const QString &rulesXml, QObject *parent = nullptr);

    QVariantList layouts() const;
    QString currentShortName() const { return m_layouts.isEmpty() ? QString() : m_layouts.first().shortName; }
    QString currentName() const { return m_layouts.isEmpty() ? QString() : m_layouts.first().name; }
    int count() const { return int(m_layouts.size()); }

    Q_INVOKABLE void select(int index);
    Q_INVOKABLE void next();

    // Control Panel > Keyboard > Input Languages.
    // Every base layout xkeyboard-config knows: {code, name, shortName},
    // sorted by name.
    Q_INVOKABLE QVariantList availableLayouts() const;
    // Adds a layout (at the end; not if already present).
    Q_INVOKABLE void addLayout(const QString &code);
    // Removes a layout; the last one cannot be removed.
    Q_INVOKABLE void removeLayout(int index);

    // The environment file content for the current order (tests).
    QString environmentFileContent() const;

Q_SIGNALS:
    void changed();

private:
    void load(const QString &layouts, const QString &variants, const QString &rulesXml);
    void apply();

    QString m_rulesXml;
    QList<Layout> m_layouts;
    QStringList m_options; // XKB_DEFAULT_OPTIONS without grp:* switches
};
