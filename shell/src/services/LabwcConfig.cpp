// SPDX-License-Identifier: GPL-3.0-or-later
#include "LabwcConfig.h"

#include "Settings.h"

#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcess>
#include <QTimer>

#include <algorithm>
#include <cstdio>

Q_LOGGING_CATEGORY(lcLabwc, "meadow.shell.labwc")

namespace {
// Returns the element at `path` (e.g. "keyboard/repeatRate") under `root`,
// creating missing elements along the way.
QDomElement ensure(QDomDocument &doc, QDomElement root, const QStringList &path,
                   const QString &attr = {}, const QString &attrValue = {})
{
    QDomElement e = root;
    for (int i = 0; i < path.size(); ++i) {
        QDomElement child;
        for (QDomElement c = e.firstChildElement(path.at(i)); !c.isNull(); c = c.nextSiblingElement(path.at(i))) {
            // The element that carries the attribute (libinput/device category="default").
            if (i == 0 || attr.isEmpty() || i != 1 || c.attribute(attr) == attrValue) {
                child = c;
                break;
            }
        }
        if (child.isNull()) {
            child = doc.createElement(path.at(i));
            if (i == 1 && !attr.isEmpty())
                child.setAttribute(attr, attrValue);
            e.appendChild(child);
        }
        e = child;
    }
    return e;
}

void setText(QDomDocument &doc, QDomElement e, const QString &text)
{
    while (e.hasChildNodes())
        e.removeChild(e.firstChild());
    e.appendChild(doc.createTextNode(text));
}
} // namespace

LabwcConfig::LabwcConfig(Settings *settings, QObject *parent)
    : LabwcConfig(settings, qEnvironmentVariable("MEADOW_LABWC_OVERLAY"), qEnvironmentVariable("MEADOW_LABWC_SOURCE"),
                  true, parent)
{
}

LabwcConfig::LabwcConfig(Settings *settings, const QString &overlayDir, const QString &sourceDir, bool reconfigure,
                         QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_overlay(overlayDir)
    , m_source(sourceDir)
    , m_reconfigure(reconfigure)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    m_timer->setInterval(300);
    connect(m_timer, &QTimer::timeout, this, &LabwcConfig::apply);
    if (m_settings)
        connect(m_settings, &Settings::changed, this, &LabwcConfig::scheduleApply);
}

QString LabwcConfig::themeForScheme(const QString &schemeId)
{
    if (schemeId == QLatin1String("luna-olive"))
        return QStringLiteral("Meadow-Luna-Olive");
    if (schemeId == QLatin1String("luna-silver"))
        return QStringLiteral("Meadow-Luna-Silver");
    return QStringLiteral("Meadow-Luna");
}

QByteArray LabwcConfig::renderRcXml() const
{
    QFile f(m_source + QStringLiteral("/rc.xml"));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QDomDocument doc;
    if (!doc.setContent(&f)) {
        qCWarning(lcLabwc) << "cannot parse" << f.fileName();
        return {};
    }
    QDomElement root = doc.documentElement();
    const Settings *s = m_settings;
    auto str = [s](const char *key, const QString &fallback) {
        return s ? s->stringValue(QString::fromLatin1(key), fallback) : fallback;
    };
    auto num = [s](const char *key, int fallback) {
        return s ? s->intSetting(QString::fromLatin1(key), fallback) : fallback;
    };

    setText(doc, ensure(doc, root, {QStringLiteral("theme"), QStringLiteral("name")}),
            themeForScheme(str("Appearance/scheme", QStringLiteral("luna-blue"))));
    setText(doc, ensure(doc, root, {QStringLiteral("keyboard"), QStringLiteral("repeatRate")}),
            QString::number(std::clamp(num("Keyboard/repeatRate", 25), 1, 100)));
    setText(doc, ensure(doc, root, {QStringLiteral("keyboard"), QStringLiteral("repeatDelay")}),
            QString::number(std::clamp(num("Keyboard/repeatDelay", 600), 100, 2000)));
    setText(doc, ensure(doc, root, {QStringLiteral("mouse"), QStringLiteral("doubleClickTime")}),
            QString::number(std::clamp(num("Mouse/doubleClickMs", 500), 100, 1500)));
    const double speed = std::clamp(s ? s->realSetting(QStringLiteral("Mouse/pointerSpeed"), 0) : 0.0, -1.0, 1.0);
    const QStringList device{QStringLiteral("libinput"), QStringLiteral("device")};
    setText(doc, ensure(doc, root, device + QStringList{QStringLiteral("pointerSpeed")}, QStringLiteral("category"), QStringLiteral("default")),
            QString::number(speed, 'f', 2));
    setText(doc, ensure(doc, root, device + QStringList{QStringLiteral("leftHanded")}, QStringLiteral("category"), QStringLiteral("default")),
            s && s->boolSetting(QStringLiteral("Mouse/swapButtons"), false) ? QStringLiteral("yes") : QStringLiteral("no"));

    doc.insertBefore(doc.createComment(QStringLiteral(" Written by meadow-shell from %1 for this session "
                                                      "(Display Properties, Mouse, Keyboard). Recreated every session. ")
                                           .arg(f.fileName())),
                     root);
    return doc.toByteArray(2);
}

void LabwcConfig::scheduleApply()
{
    m_timer->start();
}

bool LabwcConfig::apply()
{
    if (m_overlay.isEmpty() || m_source.isEmpty()) {
        qCInfo(lcLabwc) << "no labwc session overlay; window settings apply from the next session";
        return false;
    }
    const QByteArray content = renderRcXml();
    if (content.isEmpty())
        return false;
    if (content == m_lastWritten)
        return true;
    // A temporary file renamed over overlay/rc.xml: rename(2) replaces the
    // symlink itself. (QSaveFile would follow the symlink and overwrite the
    // real configuration.)
    const QString target = m_overlay + QStringLiteral("/rc.xml");
    const QString tmp = m_overlay + QStringLiteral("/.rc.xml.meadow-tmp");
    QFile out(tmp);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(content) != content.size()) {
        qCWarning(lcLabwc) << "cannot write the session rc.xml";
        out.remove();
        return false;
    }
    out.close();
    if (::rename(QFile::encodeName(tmp).constData(), QFile::encodeName(target).constData()) != 0) {
        qCWarning(lcLabwc) << "cannot replace the session rc.xml";
        QFile::remove(tmp);
        return false;
    }
    m_lastWritten = content;
    if (m_reconfigure)
        QProcess::startDetached(QStringLiteral("labwc"), {QStringLiteral("--reconfigure")});
    qCInfo(lcLabwc) << "labwc settings applied";
    Q_EMIT applied();
    return true;
}
