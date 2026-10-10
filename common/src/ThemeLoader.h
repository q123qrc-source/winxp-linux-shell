// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Logging.h"

#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// Loads a scheme's theme.json and exposes its tokens to QML as the "Theme"
// singleton (import Meadow.Controls; Theme.tokens.taskbar.height).
//
// Phase 1: loading, scheme lookup and asset override resolution.
// Phase 2: typed accessors, scale factor, font fallback resolution and live
// reload in dev mode.
class MEADOW_COMMON_EXPORT ThemeLoader : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Theme)
    QML_SINGLETON
    Q_PROPERTY(QString schemeId READ schemeId NOTIFY themeChanged)
    Q_PROPERTY(QVariantMap tokens READ tokens NOTIFY themeChanged)

public:
    // Deliberately not default-constructible: QML must use create() so every
    // engine shares the application-wide instance() instead of an empty copy.
    explicit ThemeLoader(QObject *parent);

    static ThemeLoader *create(QQmlEngine *, QJSEngine *);
    static ThemeLoader *instance();

    // Directories searched for <scheme>/theme.json, highest priority first.
    static QStringList themeSearchDirs();
    // Directories searched for user-supplied assets, highest priority first.
    static QStringList overrideDirs();

    bool loadFile(const QString &path);
    Q_INVOKABLE bool loadScheme(const QString &schemeId);

    QString schemeId() const { return m_schemeId; }
    QVariantMap tokens() const { return m_tokens; }
    QString errorString() const { return m_error; }

    // Value at a dotted token path such as "taskbar.height"; invalid if absent.
    Q_INVOKABLE QVariant token(const QString &path) const;

    // Returns a file URL for an asset: an override file if one exists,
    // otherwise the bundled file under fallbackDir; empty if neither exists.
    Q_INVOKABLE QString resolveAsset(const QString &name, const QString &fallbackDir = {}) const;

Q_SIGNALS:
    void themeChanged();

private:
    QString m_schemeId;
    QVariantMap m_tokens;
    QString m_error;
};
