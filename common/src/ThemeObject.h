// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ThemeLoader.h"

#include <QFont>
#include <QJSValue>
#include <QObject>
#include <QPointer>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

// The "Theme" QML singleton: one per QML engine, all backed by the
// process-wide ThemeLoader.
//
//   import Meadow.Controls
//   height: Theme.px(Theme.tokens.taskbar.height)
//   font: Theme.fonts.ui
//   Behavior on opacity { NumberAnimation { duration: Theme.duration("fast") } }
//
// One object per engine matters: SDDM creates a QQuickView (and engine) per
// screen, and the JS token object below belongs to exactly one engine.
// Every property notifies through themeChanged, so bindings that read
// Theme.tokens re-evaluate on scheme switch, scale change or live reload.
class MEADOW_COMMON_EXPORT ThemeObject : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Theme)
    QML_SINGLETON
    Q_PROPERTY(QString schemeId READ schemeId NOTIFY themeChanged)
    Q_PROPERTY(QJSValue tokens READ tokens NOTIFY themeChanged)
    Q_PROPERTY(QVariantMap fonts READ fonts NOTIFY themeChanged)
    Q_PROPERTY(qreal scale READ scale WRITE setScale NOTIFY themeChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY themeChanged)
    Q_PROPERTY(bool liveReload READ liveReload WRITE setLiveReload NOTIFY liveReloadChanged)

public:
    ThemeObject(QJSEngine *engine, ThemeLoader *loader);
    static ThemeObject *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    QString schemeId() const { return m_loader->schemeId(); }
    QJSValue tokens();
    QVariantMap fonts() const { return m_loader->fonts(); }
    qreal scale() const { return m_loader->scale(); }
    void setScale(qreal scale) { m_loader->setScale(scale); }
    bool reducedMotion() const { return m_loader->reducedMotion(); }
    void setReducedMotion(bool reduced) { m_loader->setReducedMotion(reduced); }
    bool liveReload() const { return m_loader->liveReload(); }
    void setLiveReload(bool enabled) { m_loader->setLiveReload(enabled); }

    Q_INVOKABLE bool loadScheme(const QString &schemeId) { return m_loader->loadScheme(schemeId); }
    Q_INVOKABLE QStringList availableSchemes() const { return ThemeLoader::availableSchemes(); }
    Q_INVOKABLE QVariant token(const QString &path) const { return m_loader->token(path); }
    Q_INVOKABLE int px(qreal value) const { return m_loader->px(value); }
    Q_INVOKABLE QFont font(const QString &role) const { return m_loader->font(role); }
    Q_INVOKABLE int duration(const QString &name) const { return m_loader->duration(name); }
    // File URL of a bundled asset (logo, avatar, ...) honoring the override folders.
    Q_INVOKABLE QString asset(const QString &name) const { return m_loader->asset(name); }
    // True if a QML module (e.g. "QtQuick.VirtualKeyboard") is installed in
    // this engine's import path, so optional features can be offered without
    // triggering import errors.
    Q_INVOKABLE bool moduleAvailable(const QString &uri) const;
    Q_INVOKABLE QString resolveAsset(const QString &name, const QString &fallbackDir = {}) const
    {
        return m_loader->resolveAsset(name, fallbackDir);
    }

Q_SIGNALS:
    void themeChanged();
    void liveReloadChanged();

private:
    QPointer<QJSEngine> m_engine;
    ThemeLoader *m_loader;
    QJSValue m_tokens;
    bool m_tokensDirty = true;
};
