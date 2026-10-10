// SPDX-License-Identifier: GPL-3.0-or-later
#include "ThemeObject.h"

#include <QJSEngine>
#include <QFileInfo>
#include <QQmlEngine>

ThemeObject::ThemeObject(QJSEngine *engine, ThemeLoader *loader)
    : QObject(nullptr)
    , m_engine(engine)
    , m_loader(loader)
{
    connect(loader, &ThemeLoader::themeChanged, this, [this] {
        m_tokensDirty = true;
        Q_EMIT themeChanged();
    });
    connect(loader, &ThemeLoader::liveReloadChanged, this, &ThemeObject::liveReloadChanged);
}

ThemeObject *ThemeObject::create(QQmlEngine *, QJSEngine *jsEngine)
{
    // Owned by the engine (JavaScriptOwnership is the default for singletons
    // returned from create()); the loader outlives every engine.
    return new ThemeObject(jsEngine, ThemeLoader::instance());
}

QJSValue ThemeObject::tokens()
{
    // Converting the QVariantMap on every access would copy the whole tree;
    // build one JS object per load and hand out references to it.
    if (m_tokensDirty && m_engine) {
        m_tokens = m_engine->toScriptValue(m_loader->tokenMap());
        m_tokensDirty = false;
    }
    return m_tokens;
}

bool ThemeObject::moduleAvailable(const QString &uri) const
{
    const auto *engine = qobject_cast<QQmlEngine *>(m_engine.data());
    if (!engine || uri.isEmpty() || uri.contains(QLatin1Char('/')))
        return false;
    const QString relative = QString(uri).replace(QLatin1Char('.'), QLatin1Char('/')) + QLatin1String("/qmldir");
    const QStringList paths = engine->importPathList();
    for (const QString &path : paths) {
        if (QFileInfo::exists(path + QLatin1Char('/') + relative))
            return true;
    }
    return false;
}
