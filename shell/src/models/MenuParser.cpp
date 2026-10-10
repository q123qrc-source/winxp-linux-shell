// SPDX-License-Identifier: GPL-3.0-or-later
#include "MenuParser.h"

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSet>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcMenu, "meadow.shell.menu")

struct MenuParser::Rule {
    enum Kind { Filename, Category, All, And, Or, Not };
    Kind kind = Or;
    QString value;
    QList<std::shared_ptr<Rule>> children;

    bool matches(const QString &id, const DesktopEntry *e) const
    {
        switch (kind) {
        case Filename: return id == value;
        case Category: return e && e->categories.contains(value);
        case All: return true;
        case And:
            return std::all_of(children.cbegin(), children.cend(), [&](const auto &c) { return c->matches(id, e); });
        case Or:
            return std::any_of(children.cbegin(), children.cend(), [&](const auto &c) { return c->matches(id, e); });
        case Not:
            return !std::any_of(children.cbegin(), children.cend(), [&](const auto &c) { return c->matches(id, e); });
        }
        return false;
    }
};

namespace {
constexpr int kMaxMergeDepth = 8;

std::shared_ptr<MenuParser::Rule> parseRule(const QDomElement &el)
{
    auto rule = std::make_shared<MenuParser::Rule>();
    const QString tag = el.tagName();
    if (tag == QLatin1String("Filename")) {
        rule->kind = MenuParser::Rule::Filename;
        rule->value = el.text().trimmed();
        return rule;
    }
    if (tag == QLatin1String("Category")) {
        rule->kind = MenuParser::Rule::Category;
        rule->value = el.text().trimmed();
        return rule;
    }
    if (tag == QLatin1String("All")) {
        rule->kind = MenuParser::Rule::All;
        return rule;
    }
    rule->kind = tag == QLatin1String("And") ? MenuParser::Rule::And
               : tag == QLatin1String("Not") ? MenuParser::Rule::Not
                                             : MenuParser::Rule::Or; // Or, Include, Exclude
    for (QDomElement c = el.firstChildElement(); !c.isNull(); c = c.nextSiblingElement()) {
        if (auto r = parseRule(c))
            rule->children << r;
    }
    return rule;
}

QString resolvePath(const QString &path, const QString &baseDir)
{
    return QFileInfo(path).isAbsolute() ? path : QDir(baseDir).absoluteFilePath(path);
}

// Folds menus with the same Name into the first one, recursively.
void consolidate(MenuParser::Menu &menu)
{
    QList<std::shared_ptr<MenuParser::Menu>> merged;
    for (const auto &child : std::as_const(menu.children)) {
        auto it = std::find_if(merged.begin(), merged.end(), [&](const auto &m) { return m->name == child->name; });
        if (it == merged.end()) {
            merged << child;
            continue;
        }
        MenuParser::Menu &into = **it;
        into.directoryFiles << child->directoryFiles;
        into.appDirs << child->appDirs;
        into.directoryDirs << child->directoryDirs;
        into.rules << child->rules;
        into.children << child->children;
        into.onlyUnallocated = child->onlyUnallocated;
        into.deleted = child->deleted;
    }
    menu.children = merged;
    for (auto &child : menu.children)
        consolidate(*child);
}

struct Evaluator {
    const MenuParser::Lookup &lookup;
    const std::function<bool(const QString &)> &visible;
    const QStringList &allIds;
    QSet<QString> allocated;

    QStringList select(const MenuParser::Menu &menu) const
    {
        QStringList ids;
        QSet<QString> in;
        for (const auto &[include, rule] : menu.rules) {
            if (include) {
                for (const QString &id : allIds) {
                    if (!in.contains(id) && rule->matches(id, lookup(id))) {
                        in.insert(id);
                        ids << id;
                    }
                }
            } else {
                ids.erase(std::remove_if(ids.begin(), ids.end(), [&](const QString &id) {
                              if (!rule->matches(id, lookup(id)))
                                  return false;
                              in.remove(id);
                              return true;
                          }),
                          ids.end());
            }
        }
        return ids;
    }

    // Pass 1: menus that take any match; records what they allocate.
    void allocate(const MenuParser::Menu &menu)
    {
        if (menu.deleted)
            return;
        if (!menu.onlyUnallocated) {
            for (const QString &id : select(menu))
                allocated.insert(id);
        }
        for (const auto &child : menu.children)
            allocate(*child);
    }

    static QString findDirectoryFile(const QStringList &dirs, const QStringList &files)
    {
        // The last <Directory> and the last <DirectoryDir> listed win.
        for (auto f = files.crbegin(); f != files.crend(); ++f) {
            for (auto d = dirs.crbegin(); d != dirs.crend(); ++d) {
                const QString path = *d + QLatin1Char('/') + *f;
                if (QFile::exists(path))
                    return path;
            }
        }
        return {};
    }

    // Pass 2: build the tree; OnlyUnallocated menus get what is left.
    MenuParser::Node build(const MenuParser::Menu &menu, QStringList directoryDirs) const
    {
        MenuParser::Node node;
        directoryDirs << menu.directoryDirs;
        node.name = menu.name;
        const QString dirFile = findDirectoryFile(directoryDirs, menu.directoryFiles);
        if (!dirFile.isEmpty()) {
            const DesktopEntry d = DesktopEntry::parse(dirFile, QFileInfo(dirFile).fileName());
            if (!d.name.isEmpty())
                node.name = d.name;
            node.icon = d.icon;
            if (d.noDisplay)
                return {};
        }
        for (const QString &id : select(menu)) {
            if (menu.onlyUnallocated && allocated.contains(id))
                continue;
            if (visible(id))
                node.apps << id;
        }
        for (const auto &child : menu.children) {
            if (child->deleted)
                continue;
            MenuParser::Node c = build(*child, directoryDirs);
            if (!c.apps.isEmpty() || !c.children.isEmpty())
                node.children << c;
        }
        return node;
    }
};
} // namespace

bool MenuParser::load(const QString &path)
{
    m_root = std::make_shared<Menu>();
    m_error.clear();
    if (!parseFile(path, *m_root, 0)) {
        m_root.reset();
        return false;
    }
    consolidate(*m_root);
    return true;
}

bool MenuParser::parseFile(const QString &path, Menu &into, int depth)
{
    if (depth > kMaxMergeDepth) {
        m_error = QStringLiteral("menu merge nesting too deep at %1").arg(path);
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("cannot read %1").arg(path);
        return false;
    }
    QDomDocument doc;
    const auto result = doc.setContent(&f);
    if (!result) {
        m_error = QStringLiteral("%1:%2: %3").arg(path).arg(result.errorLine).arg(result.errorMessage);
        return false;
    }
    const QDomElement root = doc.documentElement();
    if (root.tagName() != QLatin1String("Menu")) {
        m_error = QStringLiteral("%1: root element is not <Menu>").arg(path);
        return false;
    }
    const QString baseDir = QFileInfo(path).absolutePath();
    const QString mergedName = QFileInfo(path).completeBaseName() + QLatin1String("-merged");

    std::function<void(const QDomElement &, Menu &)> parseMenu = [&](const QDomElement &el, Menu &menu) {
        for (QDomElement c = el.firstChildElement(); !c.isNull(); c = c.nextSiblingElement()) {
            const QString tag = c.tagName();
            const QString text = c.text().trimmed();
            if (tag == QLatin1String("Name")) {
                menu.name = text;
            } else if (tag == QLatin1String("Directory")) {
                menu.directoryFiles << text;
            } else if (tag == QLatin1String("AppDir")) {
                menu.appDirs << resolvePath(text, baseDir);
            } else if (tag == QLatin1String("DirectoryDir")) {
                menu.directoryDirs << resolvePath(text, baseDir);
            } else if (tag == QLatin1String("DefaultDirectoryDirs")) {
                // Lowest priority first, so the user's directories win.
                const QStringList data = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
                for (auto it = data.crbegin(); it != data.crend(); ++it)
                    menu.directoryDirs << *it + QLatin1String("/desktop-directories");
            } else if (tag == QLatin1String("OnlyUnallocated")) {
                menu.onlyUnallocated = true;
            } else if (tag == QLatin1String("NotOnlyUnallocated")) {
                menu.onlyUnallocated = false;
            } else if (tag == QLatin1String("Deleted")) {
                menu.deleted = true;
            } else if (tag == QLatin1String("NotDeleted")) {
                menu.deleted = false;
            } else if (tag == QLatin1String("Include") || tag == QLatin1String("Exclude")) {
                menu.rules << qMakePair(tag == QLatin1String("Include"), parseRule(c));
            } else if (tag == QLatin1String("Menu")) {
                auto child = std::make_shared<Menu>();
                parseMenu(c, *child);
                menu.children << child;
            } else if (tag == QLatin1String("MergeFile")) {
                if (c.attribute(QStringLiteral("type"), QStringLiteral("path")) != QLatin1String("path"))
                    continue; // type="parent" is not supported
                const QString file = resolvePath(text, baseDir);
                Menu merged;
                if (QFile::exists(file) && parseFile(file, merged, depth + 1)) {
                    menu.rules << merged.rules;
                    menu.children << merged.children;
                    menu.directoryFiles << merged.directoryFiles;
                    menu.directoryDirs << merged.directoryDirs;
                }
            } else if (tag == QLatin1String("MergeDir") || tag == QLatin1String("DefaultMergeDirs")) {
                QStringList dirs;
                if (tag == QLatin1String("MergeDir")) {
                    dirs << resolvePath(text, baseDir);
                } else {
                    const QStringList config = QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation);
                    for (auto it = config.crbegin(); it != config.crend(); ++it)
                        dirs << *it + QLatin1String("/menus/") + mergedName;
                }
                for (const QString &dir : std::as_const(dirs)) {
                    const QStringList files = QDir(dir).entryList({QStringLiteral("*.menu")}, QDir::Files, QDir::Name);
                    for (const QString &name : files) {
                        Menu merged;
                        if (parseFile(dir + QLatin1Char('/') + name, merged, depth + 1)) {
                            menu.rules << merged.rules;
                            menu.children << merged.children;
                        }
                    }
                }
            }
            // DefaultAppDirs: the registry always covers the XDG app dirs.
            // Layout, DefaultLayout, Move, LegacyDir, KDELegacyDirs: ignored.
        }
    };
    parseMenu(root, into);
    return true;
}

MenuParser::Node MenuParser::evaluate(const QStringList &allIds, const Lookup &lookup,
                                      const std::function<bool(const QString &)> &visible) const
{
    if (!m_root)
        return {};
    Evaluator ev{lookup, visible, allIds, {}};
    ev.allocate(*m_root);
    return ev.build(*m_root, {});
}

QString MenuParser::findMenuFile(const QString &basename, const QString &bundledDir)
{
    // GenericConfigLocation = XDG_CONFIG_HOME, then XDG_CONFIG_DIRS.
    const QStringList config = QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation);
    for (const QString &dir : config) {
        const QString path = dir + QLatin1String("/menus/") + basename;
        if (QFile::exists(path))
            return path;
    }
    const QString bundled = bundledDir + QLatin1Char('/') + basename;
    return QFile::exists(bundled) ? bundled : QString();
}
