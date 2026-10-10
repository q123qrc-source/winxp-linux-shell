// SPDX-License-Identifier: GPL-3.0-or-later
#include "ThemeLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace {
ThemeLoader *s_instance = nullptr;

constexpr qreal kMinScale = 0.5;
constexpr qreal kMaxScale = 4.0;

bool envFlag(const char *name)
{
    const QByteArray v = qgetenv(name).toLower();
    return v == "1" || v == "true" || v == "yes";
}
} // namespace

ThemeLoader::ThemeLoader(QObject *parent)
    : QObject(parent)
{
    if (const QByteArray env = qgetenv("MEADOW_SCALE"); !env.isEmpty()) {
        bool ok = false;
        const qreal s = env.toDouble(&ok);
        if (ok) {
            m_scale = std::clamp(s, kMinScale, kMaxScale);
            m_scaleFromUser = true;
        }
    }
}

ThemeLoader *ThemeLoader::instance()
{
    if (!s_instance) {
        s_instance = new ThemeLoader(QCoreApplication::instance());
        const QString scheme = qEnvironmentVariable("MEADOW_THEME", QStringLiteral("luna-blue"));
        if (!s_instance->loadScheme(scheme) && scheme != QLatin1String("luna-blue"))
            s_instance->loadScheme(QStringLiteral("luna-blue"));
#ifdef MEADOW_DEV_MODE
        s_instance->setLiveReload(true);
#else
        if (envFlag("MEADOW_THEME_LIVE"))
            s_instance->setLiveReload(true);
#endif
    }
    return s_instance;
}

QStringList ThemeLoader::themeSearchDirs()
{
    QStringList dirs;
    // MEADOW_THEME_DIR lets preview/test mode run straight from a source tree.
    if (const QString dev = qEnvironmentVariable("MEADOW_THEME_DIR"); !dev.isEmpty())
        dirs << dev;
    dirs << QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/meadow/themes");
    dirs << QStringLiteral(MEADOW_DATADIR "/themes");
    return dirs;
}

QStringList ThemeLoader::overrideDirs()
{
    return {
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/meadow/assets-override"),
        QStringLiteral(MEADOW_DATADIR "/assets-override"),
    };
}

QStringList ThemeLoader::assetDirs()
{
    QStringList dirs;
    if (const QString dev = qEnvironmentVariable("MEADOW_ASSET_DIR"); !dev.isEmpty())
        dirs << dev;
    dirs << QStringLiteral(MEADOW_DATADIR "/assets");
    return dirs;
}

QStringList ThemeLoader::availableSchemes()
{
    QSet<QString> found;
    for (const QString &dir : themeSearchDirs()) {
        const auto entries = QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &entry : entries) {
            if (QFileInfo::exists(dir + QLatin1Char('/') + entry + QLatin1String("/theme.json")))
                found.insert(entry);
        }
    }
    QStringList list(found.cbegin(), found.cend());
    list.sort();
    return list;
}

bool ThemeLoader::loadFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("cannot open %1: %2").arg(path, file.errorString());
        qCWarning(lcTheme).noquote() << m_error;
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        m_error = QStringLiteral("%1: %2 at offset %3").arg(path, parseError.errorString()).arg(parseError.offset);
        qCWarning(lcTheme).noquote() << m_error;
        return false;
    }
    m_tokens = doc.object().toVariantMap();
    m_schemeId = m_tokens.value(QStringLiteral("meta")).toMap().value(QStringLiteral("id")).toString();
    m_filePath = QFileInfo(path).absoluteFilePath();
    m_error.clear();
    applyScaleDefault();
    rewatch();
    qCInfo(lcTheme).noquote() << "loaded scheme" << m_schemeId << "from" << path;
    Q_EMIT themeChanged();
    return true;
}

bool ThemeLoader::loadScheme(const QString &schemeId)
{
    // Scheme ids are directory names; refuse anything path-like.
    if (schemeId.isEmpty() || schemeId.contains(QLatin1Char('/')) || schemeId.startsWith(QLatin1Char('.'))) {
        m_error = QStringLiteral("invalid scheme id %1").arg(schemeId);
        qCWarning(lcTheme).noquote() << m_error;
        return false;
    }
    for (const QString &dir : themeSearchDirs()) {
        const QString candidate = dir + QLatin1Char('/') + schemeId + QLatin1String("/theme.json");
        if (QFileInfo::exists(candidate))
            return loadFile(candidate);
    }
    m_error = QStringLiteral("scheme %1 not found in %2").arg(schemeId, themeSearchDirs().join(QLatin1String(", ")));
    qCWarning(lcTheme).noquote() << m_error;
    return false;
}

QVariant ThemeLoader::token(const QString &path) const
{
    QVariant node = m_tokens;
    const auto parts = path.split(QLatin1Char('.'));
    for (const QString &part : parts) {
        const QVariantMap map = node.toMap();
        auto it = map.constFind(part);
        if (it == map.cend())
            return {};
        node = *it;
    }
    return node;
}

void ThemeLoader::applyScaleDefault()
{
    if (m_scaleFromUser)
        return;
    const qreal s = token(QStringLiteral("scale.factor")).toReal();
    m_scale = s > 0 ? std::clamp(s, kMinScale, kMaxScale) : 1.0;
}

void ThemeLoader::setScale(qreal scale)
{
    scale = std::clamp(scale, kMinScale, kMaxScale);
    m_scaleFromUser = true;
    if (qFuzzyCompare(m_scale, scale))
        return;
    m_scale = scale;
    Q_EMIT themeChanged();
}

int ThemeLoader::px(qreal value) const
{
    return int(std::lround(value * m_scale));
}

QString ThemeLoader::fontFamily(const QString &role) const
{
    const QStringList families = token(QStringLiteral("fonts.%1.families").arg(role)).toStringList();
    if (families.isEmpty())
        return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
    if (qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        const QStringList installed = QFontDatabase::families();
        for (const QString &family : families) {
            if (installed.contains(family, Qt::CaseInsensitive))
                return family;
        }
    }
    // Nothing from the list is installed; the generic last entry ("sans-serif")
    // lets fontconfig choose.
    return families.constLast();
}

QFont ThemeLoader::font(const QString &role) const
{
    const QVariantMap spec = token(QStringLiteral("fonts.%1").arg(role)).toMap();
    QFont f(fontFamily(role));
    f.setPixelSize(std::max(1, px(spec.value(QStringLiteral("pixelSize"), 11).toReal())));
    f.setBold(spec.value(QStringLiteral("bold")).toBool());
    f.setItalic(spec.value(QStringLiteral("italic")).toBool());
    f.setHintingPreference(QFont::PreferFullHinting);
    return f;
}

QVariantMap ThemeLoader::fonts() const
{
    QVariantMap out;
    const QVariantMap roles = m_tokens.value(QStringLiteral("fonts")).toMap();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        out.insert(it.key(), font(it.key()));
    return out;
}

bool ThemeLoader::reducedMotion() const
{
    if (m_reducedMotionOverride >= 0)
        return m_reducedMotionOverride == 1;
    return envFlag("MEADOW_REDUCED_MOTION") || token(QStringLiteral("animation.reducedMotion")).toBool();
}

void ThemeLoader::setReducedMotion(bool reduced)
{
    if (m_reducedMotionOverride == int(reduced))
        return;
    m_reducedMotionOverride = int(reduced);
    Q_EMIT themeChanged();
}

int ThemeLoader::duration(const QString &name) const
{
    if (reducedMotion())
        return 0;
    return token(QStringLiteral("animation.") + name).toInt();
}

void ThemeLoader::setLiveReload(bool enabled)
{
    if (enabled == liveReload())
        return;
    if (enabled) {
        m_watcher = new QFileSystemWatcher(this);
        connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
            // Editors often replace the file (write + rename); wait for it to
            // settle, reload, and re-add the path the watcher has dropped.
            QTimer::singleShot(100, this, [this, path] {
                if (QFileInfo::exists(path) && !loadFile(path))
                    qCWarning(lcTheme) << "live reload failed; keeping the previous theme";
                rewatch();
            });
        });
        rewatch();
        qCInfo(lcTheme) << "live theme reload enabled";
    } else {
        delete m_watcher;
        m_watcher = nullptr;
    }
    Q_EMIT liveReloadChanged();
}

void ThemeLoader::rewatch()
{
    if (!m_watcher || m_filePath.isEmpty())
        return;
    if (!m_watcher->files().isEmpty())
        m_watcher->removePaths(m_watcher->files());
    m_watcher->addPath(m_filePath);
}

QString ThemeLoader::asset(const QString &name) const
{
    for (const QString &dir : assetDirs()) {
        const QString url = resolveAsset(name, dir);
        if (!url.isEmpty())
            return url;
    }
    return resolveAsset(name);
}

QString ThemeLoader::resolveAsset(const QString &name, const QString &fallbackDir) const
{
    // Reject anything that could escape the asset directories.
    if (name.isEmpty() || name.contains(QLatin1String("..")) || QDir::isAbsolutePath(name))
        return {};
    for (const QString &dir : overrideDirs()) {
        const QString candidate = dir + QLatin1Char('/') + name;
        if (QFileInfo::exists(candidate))
            return QUrl::fromLocalFile(candidate).toString();
    }
    if (!fallbackDir.isEmpty()) {
        const QString candidate = fallbackDir + QLatin1Char('/') + name;
        if (QFileInfo::exists(candidate))
            return QUrl::fromLocalFile(candidate).toString();
    }
    return {};
}
