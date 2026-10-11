// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Logging.h"

#include <QObject>
#include <QQuickWindow>
#include <QString>
#include <QtQml/qqmlregistration.h>


// Opt-in timing marks for the performance budget (tools/perf-bench.sh).
// With MEADOW_PERF_LOG=1, mark("shell", "qml-loaded") logs
//   meadow.shell.perf: qml-loaded 412.3 ms
// to the "meadow.<component>.perf" category, in milliseconds since the
// process started (so exec and library loading are included). Without it,
// every call returns at once.
class MEADOW_COMMON_EXPORT Perf : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool enabled READ enabled CONSTANT)

public:
    using QObject::QObject;

    static bool enabled();
    static double sinceStartMs();
    static void mark(const char *component, const QString &name);
    // Marks `name` when `window` next shows a new frame.
    static void markNextFrame(QQuickWindow *window, const char *component, const QString &name);

    Q_INVOKABLE void mark(const QString &component, const QString &name);
    Q_INVOKABLE void markNextFrame(QQuickWindow *window, const QString &component, const QString &name);
};
