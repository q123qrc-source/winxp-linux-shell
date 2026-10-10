// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>

// The one place where meadow-shell starts other programs: applications,
// file manager windows, URLs and Run dialog commands. Programs start
// detached in the home directory, never through a shell.
//
// Test hook: with MEADOW_LAUNCH_LOG=<file>, each request is appended to the
// file as one line (program and arguments, tab-separated) instead of being
// started. Screenshot and QML tests use it to check what an item launches.
class Launcher : public QObject
{
    Q_OBJECT

public:
    static Launcher *instance();

    // Starts `program` with `args`; false if it could not be started.
    static bool start(const QString &program, const QStringList &args = {});
    // True if `program` is an absolute executable or found in PATH.
    static bool available(const QString &program);

    // A folder in the configured file manager (shell.conf [StartMenu]
    // fileManager, default pcmanfm-qt); falls back to xdg-open.
    Q_INVOKABLE bool openFolder(const QString &pathOrUri);
    // A file, folder or URL with its default application (xdg-open).
    Q_INVOKABLE bool open(const QString &pathOrUrl);

    void setFileManager(const QString &command) { m_fileManager = command; }

Q_SIGNALS:
    void started(const QString &program, const QStringList &args);

private:
    explicit Launcher(QObject *parent = nullptr);
    QString m_fileManager = QStringLiteral("pcmanfm-qt");
};
