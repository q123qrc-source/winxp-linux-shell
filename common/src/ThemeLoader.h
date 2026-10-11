// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Logging.h"

#include <QFont>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

class QFileSystemWatcher;

// Loads a scheme's theme.json: the process-wide source of every visual token.
// QML reaches it through the per-engine "Theme" singleton (ThemeObject).
// Every change notifies through themeChanged.
class MEADOW_COMMON_EXPORT ThemeLoader : public QObject
{
    Q_OBJECT

public:
    explicit ThemeLoader(QObject *parent = nullptr);

    static ThemeLoader *instance();

    // Directories searched for <scheme>/theme.json, highest priority first.
    static QStringList themeSearchDirs();
    // Directories searched for user-supplied assets, highest priority first.
    static QStringList overrideDirs();
    // Directories holding bundled artwork (logo, avatars, wallpaper).
    static QStringList assetDirs();
    // Names of the schemes found in themeSearchDirs(), sorted, de-duplicated.
    static QStringList availableSchemes();

    bool loadFile(const QString &path);
    bool loadScheme(const QString &schemeId);

    QString schemeId() const { return m_schemeId; }
    QString filePath() const { return m_filePath; }
    QVariantMap tokenMap() const { return m_tokens; }
    QString errorString() const { return m_error; }

    // Value at a dotted token path such as "taskbar.height"; invalid if absent.
    QVariant token(const QString &path) const;

    // Scale factor applied by px() and fonts. Defaults to MEADOW_SCALE or
    // the scheme's scale.factor; clamped to [0.5, 4].
    qreal scale() const { return m_scale; }
    void setScale(qreal scale);
    // A metric in device-independent pixels, scaled and rounded so edges stay crisp.
    int px(qreal value) const;

    // Font for a role in tokens.fonts (ui, title, start, ...): the first family
    // of the role's list that is installed, at the scaled pixel size.
    QFont font(const QString &role) const;
    QString fontFamily(const QString &role) const;
    QVariantMap fonts() const;

    // Duration of an animation token ("fast", "startMenuOpenMs", ...);
    // 0 when reduced motion is on.
    bool reducedMotion() const;
    void setReducedMotion(bool reduced);
    int duration(const QString &name) const;

    // Re-read the theme file whenever it changes on disk (dev mode).
    bool liveReload() const { return m_watcher != nullptr; }
    void setLiveReload(bool enabled);

    // Returns a file URL for an asset: an override file if one exists,
    // otherwise the bundled file under fallbackDir; empty if neither exists.
    QString resolveAsset(const QString &name, const QString &fallbackDir = {}) const;
    // resolveAsset() over the override folders, then assetDirs().
    QString asset(const QString &name) const;

Q_SIGNALS:
    void themeChanged();
    void liveReloadChanged();

private:
    void applyScaleDefault();
    QString resolveFontFamily(const QString &role) const;
    void rewatch();

    QString m_schemeId;
    QString m_filePath;
    QVariantMap m_tokens;
    // fontFamily() per role for the loaded scheme: every Text asks, and the
    // installed-families list is expensive to build.
    mutable QHash<QString, QString> m_familyCache;
    QString m_error;
    qreal m_scale = 1.0;
    bool m_scaleFromUser = false;
    int m_reducedMotionOverride = -1; // -1: follow the scheme / MEADOW_REDUCED_MOTION
    QFileSystemWatcher *m_watcher = nullptr;
};
