// SPDX-License-Identifier: GPL-3.0-or-later
#include "DesktopEntry.h"

#include <QFile>
#include <QHash>
#include <QLocale>
#include <QStandardPaths>
#include <QTextStream>

#include <algorithm>

namespace {
// Unescape a value: \s \n \t \r \\ (and \; inside lists).
QString unescape(const QString &value)
{
    QString out;
    out.reserve(value.size());
    for (int i = 0; i < value.size(); ++i) {
        const QChar c = value.at(i);
        if (c == QLatin1Char('\\') && i + 1 < value.size()) {
            const QChar n = value.at(++i);
            switch (n.unicode()) {
            case 's': out += QLatin1Char(' '); break;
            case 'n': out += QLatin1Char('\n'); break;
            case 't': out += QLatin1Char('\t'); break;
            case 'r': out += QLatin1Char('\r'); break;
            case '\\': out += QLatin1Char('\\'); break;
            case ';': out += QLatin1String("\\;"); break; // kept for list splitting
            default: out += QLatin1Char('\\'); out += n; break;
            }
        } else {
            out += c;
        }
    }
    return out;
}

QStringList splitList(const QString &value)
{
    QStringList out;
    QString current;
    for (int i = 0; i < value.size(); ++i) {
        if (value.at(i) == QLatin1Char('\\') && i + 1 < value.size() && value.at(i + 1) == QLatin1Char(';')) {
            current += QLatin1Char(';');
            ++i;
        } else if (value.at(i) == QLatin1Char(';')) {
            out << current;
            current.clear();
        } else {
            current += value.at(i);
        }
    }
    if (!current.isEmpty())
        out << current;
    return out;
}

bool isTrue(const QString &v)
{
    return v.trimmed() == QLatin1String("true");
}
} // namespace

DesktopEntry DesktopEntry::parse(const QString &path, const QString &id, const QString &locale)
{
    DesktopEntry e;
    e.id = id;
    e.path = path;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return e;

    // Locale variants, most specific first: ll_CC@mod, ll_CC, ll@mod, ll.
    const QString loc = locale.isEmpty() ? QLocale().name() : locale;
    QStringList variants{loc};
    if (loc.contains(QLatin1Char('_')))
        variants << loc.section(QLatin1Char('_'), 0, 0);

    QHash<QString, QString> values;
    bool inMain = false;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('['))) {
            inMain = line == QLatin1String("[Desktop Entry]");
            continue;
        }
        if (!inMain)
            continue;
        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        values.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }

    const auto localized = [&](const QString &key) {
        for (const QString &v : std::as_const(variants)) {
            const QString k = key + QLatin1Char('[') + v + QLatin1Char(']');
            if (values.contains(k))
                return unescape(values.value(k));
        }
        return unescape(values.value(key));
    };

    e.type = values.value(QStringLiteral("Type"));
    e.name = localized(QStringLiteral("Name"));
    e.genericName = localized(QStringLiteral("GenericName"));
    e.comment = localized(QStringLiteral("Comment"));
    e.icon = unescape(values.value(QStringLiteral("Icon")));
    e.exec = values.value(QStringLiteral("Exec"));
    e.tryExec = unescape(values.value(QStringLiteral("TryExec")));
    e.startupWMClass = values.value(QStringLiteral("StartupWMClass"));
    e.categories = splitList(values.value(QStringLiteral("Categories")));
    e.onlyShowIn = splitList(values.value(QStringLiteral("OnlyShowIn")));
    e.notShowIn = splitList(values.value(QStringLiteral("NotShowIn")));
    e.mimeTypes = splitList(values.value(QStringLiteral("MimeType")));
    e.noDisplay = isTrue(values.value(QStringLiteral("NoDisplay")));
    e.hidden = isTrue(values.value(QStringLiteral("Hidden")));
    e.terminal = isTrue(values.value(QStringLiteral("Terminal")));
    return e;
}

bool DesktopEntry::shownIn(const QString &desktop) const
{
    return shownIn(QStringList{desktop});
}

bool DesktopEntry::shownIn(const QStringList &desktops) const
{
    if (hidden || noDisplay || type != QLatin1String("Application"))
        return false;
    const auto any = [&desktops](const QStringList &list) {
        return std::any_of(desktops.cbegin(), desktops.cend(), [&list](const QString &d) { return list.contains(d); });
    };
    if (!onlyShowIn.isEmpty() && !any(onlyShowIn))
        return false;
    if (any(notShowIn))
        return false;
    if (!tryExec.isEmpty() && QStandardPaths::findExecutable(tryExec).isEmpty()
        && !(tryExec.startsWith(QLatin1Char('/')) && QFile::exists(tryExec)))
        return false;
    return true;
}

QStringList DesktopEntry::execArguments(const QStringList &files) const
{
    // 1. Split into arguments, honoring double quotes and backslash escapes.
    QStringList args;
    QString current;
    bool inQuotes = false, have = false;
    const QString value = unescape(exec).replace(QLatin1String("\\;"), QLatin1String(";"));
    for (int i = 0; i < value.size(); ++i) {
        const QChar c = value.at(i);
        if (inQuotes) {
            if (c == QLatin1Char('\\') && i + 1 < value.size()
                && QStringLiteral("\"`$\\").contains(value.at(i + 1))) {
                current += value.at(++i);
            } else if (c == QLatin1Char('"')) {
                inQuotes = false;
            } else {
                current += c;
            }
        } else if (c == QLatin1Char('"')) {
            inQuotes = true;
            have = true;
        } else if (c.isSpace()) {
            if (have || !current.isEmpty())
                args << current;
            current.clear();
            have = false;
        } else {
            current += c;
        }
    }
    if (have || !current.isEmpty())
        args << current;

    // 2. Field codes.
    QStringList out;
    for (const QString &arg : std::as_const(args)) {
        if (arg == QLatin1String("%f") || arg == QLatin1String("%u")) {
            if (!files.isEmpty())
                out << files.first();
        } else if (arg == QLatin1String("%F") || arg == QLatin1String("%U")) {
            out << files;
        } else if (arg == QLatin1String("%i")) {
            if (!icon.isEmpty())
                out << QStringLiteral("--icon") << icon;
        } else {
            QString a = arg;
            a.replace(QLatin1String("%c"), name).replace(QLatin1String("%k"), path);
            // Deprecated codes (%d %D %n %N %v %m) are removed; %% is a literal %.
            static const QStringList removed{QStringLiteral("%d"), QStringLiteral("%D"), QStringLiteral("%n"),
                                             QStringLiteral("%N"), QStringLiteral("%v"), QStringLiteral("%m"),
                                             QStringLiteral("%f"), QStringLiteral("%u"), QStringLiteral("%F"), QStringLiteral("%U")};
            for (const QString &code : removed)
                a.remove(code);
            a.replace(QLatin1String("%%"), QLatin1String("%"));
            if (!a.isEmpty())
                out << a;
        }
    }
    return out;
}
