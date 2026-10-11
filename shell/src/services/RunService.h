// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>

class AppRegistry;

// The Run dialog's logic.
//
// run(text) opens, in order of preference:
//   - an existing file or folder (absolute, ~/…, or file://), with its
//     default application or the file manager;
//   - a URL (scheme://… or mailto:) with its default handler;
//   - a program from PATH, with arguments (split like a shell would, but
//     never run by a shell: no pipes, redirection or variables);
//   - an application by desktop file id or exact name ("firefox",
//     "Text Editor").
// Successful entries go to the history (~/.local/share/meadow/run-history,
// newest first, at most kMaxHistory). Nothing typed is logged.
class RunService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList history READ history NOTIFY historyChanged)

public:
    static constexpr int kMaxHistory = 26;
    static constexpr int kMaxCompletions = 12;

    RunService(AppRegistry *apps, QObject *parent = nullptr);
    RunService(AppRegistry *apps, const QString &historyFile, QObject *parent = nullptr);

    QStringList history() const { return m_history; }

    // Runs `text`; returns an empty string on success, else a message for
    // the user.
    Q_INVOKABLE QString run(const QString &text);
    // Completions for `prefix`: history first, then folders/files for paths,
    // else PATH programs.
    Q_INVOKABLE QStringList complete(const QString &prefix) const;
    // Opens the desktop portal's file chooser; answers with browsed(path,
    // tag) so several dialogs can share it (tag: "run", "wallpaper", ...).
    Q_INVOKABLE void browse(const QString &title, const QString &tag = QStringLiteral("run"));

Q_SIGNALS:
    void historyChanged();
    void browsed(const QString &path, const QString &tag);

private Q_SLOTS:
    void onPortalResponse(uint response, const QVariantMap &results);

private:
    void remember(const QString &text);
    void saveHistory() const;
    static QString expandHome(const QString &path);

    AppRegistry *m_apps;
    QString m_historyFile;
    QStringList m_history;
    QString m_requestPath;
    QString m_browseTag;
};
