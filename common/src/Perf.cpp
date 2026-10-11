// SPDX-License-Identifier: GPL-3.0-or-later
#include "Perf.h"

#include <QFile>
#include <QHash>
#include <QMutex>
#include <QQuickWindow>

#include <ctime>
#include <memory>
#include <unistd.h>

namespace {

double nowBootMs()
{
    timespec ts {};
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

// Process start in ms since boot, from /proc/self/stat (field 22, clock ticks).
double processStartMs()
{
    QFile f(QStringLiteral("/proc/self/stat"));
    if (!f.open(QIODevice::ReadOnly))
        return nowBootMs();
    const QByteArray stat = f.readAll();
    // The command name (field 2) may contain spaces; fields follow its ')'.
    const QList<QByteArray> fields = stat.mid(stat.lastIndexOf(')') + 2).split(' ');
    // fields[0] is field 3 (state), so starttime (field 22) is fields[19].
    if (fields.size() < 20)
        return nowBootMs();
    return fields.at(19).toDouble() * 1000.0 / double(sysconf(_SC_CLK_TCK));
}

const QLoggingCategory &category(const QString &component)
{
    static QMutex mutex;
    static QHash<QString, std::shared_ptr<QByteArray>> names;
    static QHash<QString, std::shared_ptr<QLoggingCategory>> categories;
    QMutexLocker lock(&mutex);
    auto it = categories.find(component);
    if (it == categories.end()) {
        auto name = std::make_shared<QByteArray>("meadow." + component.toUtf8() + ".perf");
        names.insert(component, name);
        auto cat = std::make_shared<QLoggingCategory>(name->constData());
        cat->setEnabled(QtInfoMsg, true);
        it = categories.insert(component, cat);
    }
    return *it.value();
}

} // namespace

bool Perf::enabled()
{
    static const bool on = qEnvironmentVariableIntValue("MEADOW_PERF_LOG") == 1;
    return on;
}

double Perf::sinceStartMs()
{
    static const double start = processStartMs();
    return nowBootMs() - start;
}

void Perf::mark(const char *component, const QString &name)
{
    if (!enabled())
        return;
    qCInfo(category(QString::fromLatin1(component))).noquote()
        << name << QString::number(sinceStartMs(), 'f', 1) << "ms";
}

void Perf::markNextFrame(QQuickWindow *window, const char *component, const QString &name)
{
    if (!enabled() || !window)
        return;
    auto conn = std::make_shared<QMetaObject::Connection>();
    const QString comp = QString::fromLatin1(component);
    *conn = QObject::connect(window, &QQuickWindow::frameSwapped, window, [conn, comp, name] {
        QObject::disconnect(*conn);
        Perf::mark(comp.toLatin1().constData(), name);
    }, Qt::DirectConnection);
}

void Perf::mark(const QString &component, const QString &name)
{
    mark(component.toLatin1().constData(), name);
}

void Perf::markNextFrame(QQuickWindow *window, const QString &component, const QString &name)
{
    markNextFrame(window, component.toLatin1().constData(), name);
}
