// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Logging.h"

#include <QFont>
#include <QJSValue>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QFileSystemWatcher;
class QQmlEngine;
class QJSEngine;

// Loads a scheme's theme.json and exposes it to QML as the "Theme" singleton:
//
//   import Meadow.Controls
//   height: Theme.px(Theme.tokens.taskbar.height)
//   font: Theme.fonts.ui
//   Behavior on opacity { NumberAnimation { duration: Theme.duration("fast") } }
//
// Every property notifies through themeChanged, so a binding that reads
// Theme.tokens re-evaluates on scheme switch, scale change or live reload.
class MEADOW_COMMON_EXPORT ThemeLoader : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Theme)
    QML_SINGLETON
    Q_PROPERTY(QString schemeId READ schemeId NOTIFY themeChanged)
    Q_PROPERTY(QJSValue tokens READ tokensJs NOTIFY themeChanged)
    Q_PROPERTY(QVariantMap fonts READ fonts NOTIFY themeChanged)
    Q_PROPERTY(qreal scale READ scale WRITE setScale NOTIFY themeChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY themeChanged)
    Q_PROPERTY(bool liveReload READ liveReload WRITE setLiveReload NOTIFY liveReloadChanged)

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
    // Names of the schemes found in themeSearchDirs(), sorted, de-duplicated.
    Q_INVOKABLE static QStringList availableSchemes();

    bool loadFile(const QString &path);
    Q_INVOKABLE bool loadScheme(const QString &schemeId);

    QString schemeId() const { return m_schemeId; }
    QString filePath() const { return m_filePath; }
    QVariantMap tokenMap() const { return m_tokens; }
    QJSValue tokensJs();
    QString errorString() const { return m_error; }

    // Value at a dotted token path such as "taskbar.height"; invalid if absent.
    Q_INVOKABLE QVariant token(const QString &path) const;

    // Scale factor applied by px() and fonts. Defaults to MEADOW_SCALE or
    // the scheme's scale.factor; clamped to [0.5, 4].
    qreal scale() const { return m_scale; }
    void setScale(qreal scale);
    // A metric in device-independent pixels, scaled and rounded so edges stay crisp.
    Q_INVOKABLE int px(qreal value) const;

    // Font for a role in tokens.fonts (ui, title, start, ...): the first family
    // of the role's list that is installed, at the scaled pixel size.
    Q_INVOKABLE QFont font(const QString &role) const;
    Q_INVOKABLE QString fontFamily(const QString &role) const;
    QVariantMap fonts() const;

    // Duration of an animation token ("fast", "startMenuOpenMs", ...);
    // 0 when reduced motion is on.
    bool reducedMotion() const;
    void setReducedMotion(bool reduced);
    Q_INVOKABLE int duration(const QString &name) const;

    // Re-read the theme file whenever it changes on disk (dev mode).
    bool liveReload() const { return m_watcher != nullptr; }
    void setLiveReload(bool enabled);

    // Returns a file URL for an asset: an override file if one exists,
    // otherwise the bundled file under fallbackDir; empty if neither exists.
    Q_INVOKABLE QString resolveAsset(const QString &name, const QString &fallbackDir = {}) const;

Q_SIGNALS:
    void themeChanged();
    void liveReloadChanged();

private:
    void applyScaleDefault();
    void rewatch();

    QString m_schemeId;
    QString m_filePath;
    QVariantMap m_tokens;
    QString m_error;
    qreal m_scale = 1.0;
    bool m_scaleFromUser = false;
    int m_reducedMotionOverride = -1; // -1: follow the scheme / MEADOW_REDUCED_MOTION
    QPointer<QJSEngine> m_engine;
    QJSValue m_tokensJs;
    bool m_tokensJsDirty = true;
    QFileSystemWatcher *m_watcher = nullptr;
};
