// SPDX-License-Identifier: GPL-3.0-or-later
#include "VolumeService.h"

#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>

namespace {
constexpr int kPollMs = 3000;
constexpr qreal kMaxVolume = 1.0; // the slider stops at 100%, like the classic mixer
const QString kSink = QStringLiteral("@DEFAULT_AUDIO_SINK@");
}

VolumeService::VolumeService(QObject *parent)
    : QObject(parent)
{
    m_poll.setInterval(kPollMs);
    connect(&m_poll, &QTimer::timeout, this, &VolumeService::refresh);
    if (!program().isEmpty()) {
        m_poll.start();
        refresh();
    }
}

QString VolumeService::program() const
{
    const QString env = qEnvironmentVariable("MEADOW_WPCTL");
    return env.isEmpty() ? QStandardPaths::findExecutable(QStringLiteral("wpctl")) : env;
}

bool VolumeService::parse(const QString &output, qreal *volume, bool *muted)
{
    static const QRegularExpression re(QStringLiteral(R"(Volume:\s*([0-9]+(?:\.[0-9]+)?)(\s*\[MUTED\])?)"));
    const auto m = re.match(output);
    if (!m.hasMatch())
        return false;
    *volume = m.captured(1).toDouble();
    *muted = !m.captured(2).isEmpty();
    return true;
}

void VolumeService::refresh()
{
    const QString exe = program();
    if (exe.isEmpty())
        return;
    auto *p = new QProcess(this);
    connect(p, &QProcess::finished, this, [this, p](int code) {
        p->deleteLater();
        qreal v = 0;
        bool m = false;
        const bool ok = code == 0 && parse(QString::fromUtf8(p->readAllStandardOutput()), &v, &m);
        if (ok == m_available && (!ok || (qFuzzyCompare(v + 1, m_volume + 1) && m == m_muted)))
            return;
        m_available = ok;
        if (ok) {
            m_volume = v;
            m_muted = m;
        }
        Q_EMIT changed();
    });
    p->start(exe, {QStringLiteral("get-volume"), kSink});
}

void VolumeService::run(const QStringList &args, bool refreshAfter)
{
    const QString exe = program();
    if (exe.isEmpty())
        return;
    auto *p = new QProcess(this);
    connect(p, &QProcess::finished, this, [this, p, refreshAfter] {
        p->deleteLater();
        if (refreshAfter)
            refresh();
    });
    p->start(exe, args);
}

void VolumeService::setVolume(qreal volume)
{
    volume = std::clamp(volume, 0.0, kMaxVolume);
    if (qFuzzyCompare(volume + 1, m_volume + 1))
        return;
    m_volume = volume; // optimistic, confirmed by the refresh
    Q_EMIT changed();
    run({QStringLiteral("set-volume"), kSink, QString::number(volume, 'f', 2)}, true);
}

void VolumeService::setMuted(bool muted)
{
    if (muted == m_muted)
        return;
    m_muted = muted;
    Q_EMIT changed();
    run({QStringLiteral("set-mute"), kSink, muted ? QStringLiteral("1") : QStringLiteral("0")}, true);
}
