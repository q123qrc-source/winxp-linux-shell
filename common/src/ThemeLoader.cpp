// SPDX-License-Identifier: GPL-3.0-or-later
#include "ThemeLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUrl>

namespace {
ThemeLoader *s_instance = nullptr;
}

ThemeLoader::ThemeLoader(QObject *parent)
    : QObject(parent)
{
}

ThemeLoader *ThemeLoader::instance()
{
    if (!s_instance) {
        s_instance = new ThemeLoader(QCoreApplication::instance());
        const QString scheme = qEnvironmentVariable("MEADOW_THEME", QStringLiteral("luna-blue"));
        if (!s_instance->loadScheme(scheme) && scheme != QLatin1String("luna-blue"))
            s_instance->loadScheme(QStringLiteral("luna-blue"));
    }
    return s_instance;
}

ThemeLoader *ThemeLoader::create(QQmlEngine *, QJSEngine *)
{
    ThemeLoader *loader = instance();
    // The singleton is owned by the application, not by the QML engine.
    QJSEngine::setObjectOwnership(loader, QJSEngine::CppOwnership);
    return loader;
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
    m_error.clear();
    qCInfo(lcTheme).noquote() << "loaded scheme" << m_schemeId << "from" << path;
    Q_EMIT themeChanged();
    return true;
}

bool ThemeLoader::loadScheme(const QString &schemeId)
{
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
