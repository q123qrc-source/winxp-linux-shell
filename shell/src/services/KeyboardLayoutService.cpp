// SPDX-License-Identifier: GPL-3.0-or-later
#include "KeyboardLayoutService.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLoggingCategory>
#include <QProcess>
#include <QSaveFile>
#include <QXmlStreamReader>

Q_LOGGING_CATEGORY(lcKeyboard, "meadow.shell.keyboard")

namespace {
struct XkbNames {
    QHash<QString, QPair<QString, QString>> layouts;  // layout -> (short, description)
    QHash<QString, QString> variants;                 // "layout(variant)" -> description
};

XkbNames readRules(const QString &path)
{
    // evdev.xml: layoutList/layout/configItem/{name,shortDescription,description}
    // and layout/variantList/variant/configItem/{name,description}.
    XkbNames names;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return names;
    QXmlStreamReader xml(&f);
    QStringList path_;
    QString layout, layoutShort, variant;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isEndElement()) {
            path_.removeLast();
            continue;
        }
        if (!xml.isStartElement())
            continue;
        const QString el = xml.name().toString();
        const bool inLayoutItem = path_.size() >= 2 && path_.last() == QLatin1String("configItem")
            && path_.at(path_.size() - 2) == QLatin1String("layout");
        const bool inVariantItem = path_.size() >= 2 && path_.last() == QLatin1String("configItem")
            && path_.at(path_.size() - 2) == QLatin1String("variant");
        if ((inLayoutItem || inVariantItem)
            && (el == QLatin1String("name") || el == QLatin1String("shortDescription") || el == QLatin1String("description"))) {
            const QString text = xml.readElementText(); // consumes the end element
            if (inLayoutItem) {
                if (el == QLatin1String("name"))
                    layout = text, layoutShort.clear();
                else if (el == QLatin1String("shortDescription"))
                    layoutShort = text;
                else
                    names.layouts.insert(layout, {layoutShort, text});
            } else {
                if (el == QLatin1String("name"))
                    variant = text;
                else if (el == QLatin1String("description"))
                    names.variants.insert(layout + QLatin1Char('(') + variant + QLatin1Char(')'), text);
            }
            continue;
        }
        path_ << el;
    }
    return names;
}
} // namespace

KeyboardLayoutService::KeyboardLayoutService(QObject *parent)
    : QObject(parent)
{
    QString layouts = qEnvironmentVariable("XKB_DEFAULT_LAYOUT", QStringLiteral("us"));
    QString variants = qEnvironmentVariable("XKB_DEFAULT_VARIANT");
    // A restarted shell inherits the session's original environment; the
    // order it last wrote is the one labwc uses now.
    QFile envFile(qEnvironmentVariable("MEADOW_LABWC_ENV_FILE"));
    if (!envFile.fileName().isEmpty() && envFile.open(QIODevice::ReadOnly)) {
        const QStringList lines = QString::fromUtf8(envFile.readAll()).split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            if (line.startsWith(QLatin1String("XKB_DEFAULT_LAYOUT=")))
                layouts = line.section(QLatin1Char('='), 1);
            else if (line.startsWith(QLatin1String("XKB_DEFAULT_VARIANT=")))
                variants = line.section(QLatin1Char('='), 1);
        }
    }
    // xkb's own group-switch options would change the layout behind the
    // language bar's back; the shell does the switching instead.
    const QStringList options = qEnvironmentVariable("XKB_DEFAULT_OPTIONS").split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &o : options) {
        if (!o.startsWith(QLatin1String("grp:")))
            m_options << o;
    }
    load(layouts, variants, QStringLiteral("/usr/share/X11/xkb/rules/evdev.xml"));
}

KeyboardLayoutService::KeyboardLayoutService(const QString &layouts, const QString &variants, const QString &rulesXml, QObject *parent)
    : QObject(parent)
{
    load(layouts, variants, rulesXml);
}

void KeyboardLayoutService::load(const QString &layouts, const QString &variants, const QString &rulesXml)
{
    m_rulesXml = rulesXml;
    const XkbNames names = readRules(rulesXml);
    const QStringList ls = layouts.split(QLatin1Char(','));
    const QStringList vs = variants.split(QLatin1Char(','));
    for (int i = 0; i < ls.size(); ++i) {
        Layout l;
        l.layout = ls.at(i).trimmed();
        if (l.layout.isEmpty())
            continue;
        l.variant = vs.value(i).trimmed();
        const auto known = names.layouts.value(l.layout);
        // Short name: xkeyboard-config's language code, else the layout code.
        l.shortName = (known.first.isEmpty() ? l.layout : known.first).left(3).toUpper();
        l.name = names.variants.value(l.layout + QLatin1Char('(') + l.variant + QLatin1Char(')'),
                                      known.second.isEmpty() ? l.layout : known.second);
        m_layouts.append(l);
    }
}

QVariantList KeyboardLayoutService::layouts() const
{
    QVariantList out;
    for (const Layout &l : m_layouts)
        out << QVariantMap{{QStringLiteral("shortName"), l.shortName}, {QStringLiteral("name"), l.name}};
    return out;
}

void KeyboardLayoutService::select(int index)
{
    if (index <= 0 || index >= m_layouts.size())
        return;
    // The chosen layout becomes the first (active) one; the others keep order.
    m_layouts.move(index, 0);
    Q_EMIT changed();
    apply();
}

void KeyboardLayoutService::next()
{
    if (m_layouts.size() < 2)
        return;
    // Rotate: the active layout goes to the end.
    m_layouts.move(0, m_layouts.size() - 1);
    Q_EMIT changed();
    apply();
}

QString KeyboardLayoutService::environmentFileContent() const
{
    QStringList layouts, variants;
    for (const Layout &l : m_layouts) {
        layouts << l.layout;
        variants << l.variant;
    }
    QString out = QStringLiteral("# Written by meadow-shell (language bar). Recreated every session.\n");
    out += QStringLiteral("XKB_DEFAULT_LAYOUT=%1\n").arg(layouts.join(QLatin1Char(',')));
    out += QStringLiteral("XKB_DEFAULT_VARIANT=%1\n").arg(variants.join(QLatin1Char(',')));
    out += QStringLiteral("XKB_DEFAULT_OPTIONS=%1\n").arg(m_options.join(QLatin1Char(',')));
    return out;
}

void KeyboardLayoutService::apply()
{
    const QString envFile = qEnvironmentVariable("MEADOW_LABWC_ENV_FILE");
    if (envFile.isEmpty()) {
        qCInfo(lcKeyboard) << "layout order changed to" << currentShortName() << "(no session overlay; not applied)";
        return;
    }
    QDir().mkpath(QFileInfo(envFile).absolutePath());
    QSaveFile f(envFile);
    if (!f.open(QIODevice::WriteOnly) || f.write(environmentFileContent().toUtf8()) < 0 || !f.commit()) {
        qCWarning(lcKeyboard) << "cannot write" << envFile;
        return;
    }
    // labwc reloads its environment files and the keymap on reconfigure.
    QProcess::startDetached(QStringLiteral("labwc"), {QStringLiteral("--reconfigure")});
    qCInfo(lcKeyboard) << "keyboard layout" << currentShortName();
}

QVariantList KeyboardLayoutService::availableLayouts() const
{
    const XkbNames names = readRules(m_rulesXml);
    QList<QVariantMap> list;
    for (auto it = names.layouts.cbegin(); it != names.layouts.cend(); ++it) {
        if (it.key() == QLatin1String("custom"))
            continue; // xkeyboard-config's placeholder for a user-written layout
        const QString shortName = (it.value().first.isEmpty() ? it.key() : it.value().first).left(3).toUpper();
        list << QVariantMap{{QStringLiteral("code"), it.key()},
                            {QStringLiteral("name"), it.value().second.isEmpty() ? it.key() : it.value().second},
                            {QStringLiteral("shortName"), shortName}};
    }
    std::sort(list.begin(), list.end(), [](const QVariantMap &a, const QVariantMap &b) {
        return QString::localeAwareCompare(a.value(QStringLiteral("name")).toString(), b.value(QStringLiteral("name")).toString()) < 0;
    });
    QVariantList out;
    for (const QVariantMap &m : std::as_const(list))
        out << m;
    return out;
}

void KeyboardLayoutService::addLayout(const QString &code)
{
    for (const Layout &l : std::as_const(m_layouts)) {
        if (l.layout == code && l.variant.isEmpty())
            return;
    }
    const XkbNames names = readRules(m_rulesXml);
    if (!names.layouts.contains(code))
        return;
    Layout l;
    l.layout = code;
    const auto known = names.layouts.value(code);
    l.shortName = (known.first.isEmpty() ? code : known.first).left(3).toUpper();
    l.name = known.second.isEmpty() ? code : known.second;
    m_layouts.append(l);
    Q_EMIT changed();
    apply();
}

void KeyboardLayoutService::removeLayout(int index)
{
    if (m_layouts.size() <= 1 || index < 0 || index >= m_layouts.size())
        return;
    m_layouts.removeAt(index);
    Q_EMIT changed();
    apply();
}
