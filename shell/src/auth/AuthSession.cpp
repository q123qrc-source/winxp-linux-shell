// SPDX-License-Identifier: GPL-3.0-or-later
#include "AuthSession.h"

#include <QFileInfo>
#include <QLoggingCategory>

#include <cstring>

Q_LOGGING_CATEGORY(lcAuth, "meadow.shell.auth")

#ifndef MEADOW_LIBEXECDIR
#define MEADOW_LIBEXECDIR "/usr/libexec/meadow"
#endif

namespace {
constexpr int kMaxFrame = 4096;

void wipe(QByteArray &bytes)
{
    if (!bytes.isEmpty())
        std::memset(bytes.data(), 0, size_t(bytes.size()));
    bytes.clear();
}
} // namespace

AuthSession::AuthSession(QObject *parent)
    : QObject(parent)
{
    // stderr carries only "authentication succeeded/failed" from the helper.
    m_process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &AuthSession::readFrames);
    connect(&m_process, &QProcess::finished, this, &AuthSession::onProcessFinished);
}

AuthSession::~AuthSession()
{
    if (isRunning()) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
    wipeBuffer();
}

QString AuthSession::helperPath()
{
    const QString env = qEnvironmentVariable("MEADOW_AUTH_HELPER");
    return env.isEmpty() ? QStringLiteral(MEADOW_LIBEXECDIR "/meadow-auth-helper") : env;
}

bool AuthSession::start()
{
    if (isRunning())
        return false;
    const QString helper = helperPath();
    if (!QFileInfo(helper).isExecutable()) {
        qCWarning(lcAuth) << "authentication helper not found:" << helper;
        return false;
    }
    wipeBuffer();
    m_resultSeen = false;
    m_result = false;
    m_process.start(helper, {});
    if (!m_process.waitForStarted(3000)) {
        qCWarning(lcAuth) << "cannot start the authentication helper:" << m_process.errorString();
        return false;
    }
    return true;
}

void AuthSession::respond(QString secret)
{
    QByteArray bytes = secret.toUtf8();
    // Overwrite the caller's copy too (QString shares data copy-on-write,
    // so this only clears our detached copy; callers clear their own).
    secret.fill(QChar(u'\0'));
    secret.clear();
    if (!isRunning()) {
        wipe(bytes);
        return;
    }
    if (bytes.size() > kMaxFrame)
        bytes.truncate(kMaxFrame);
    QByteArray frame = "RESPONSE " + QByteArray::number(bytes.size()) + '\n' + bytes;
    m_process.write(frame);
    wipe(frame);
    wipe(bytes);
}

void AuthSession::cancel()
{
    if (!isRunning())
        return;
    m_process.write("CANCEL 0\n");
    m_process.closeWriteChannel();
    if (!m_process.waitForFinished(500))
        m_process.kill();
}

void AuthSession::readFrames()
{
    m_buffer.append(m_process.readAllStandardOutput());
    for (;;) {
        const qsizetype nl = m_buffer.indexOf('\n');
        if (nl < 0)
            return;
        const QByteArray header = m_buffer.left(nl);
        const qsizetype space = header.indexOf(' ');
        bool ok = false;
        const int len = space > 0 ? header.mid(space + 1).toInt(&ok) : -1;
        if (!ok || len < 0 || len > kMaxFrame) {
            qCWarning(lcAuth) << "malformed message from the authentication helper";
            m_process.kill();
            wipeBuffer();
            return;
        }
        if (m_buffer.size() < nl + 1 + len)
            return; // wait for the rest
        const QByteArray type = header.left(space);
        const QString payload = QString::fromUtf8(m_buffer.mid(nl + 1, len));
        m_buffer.remove(0, nl + 1 + len);

        if (type == "PROMPT_ECHO_OFF" || type == "PROMPT_ECHO_ON")
            Q_EMIT prompt(payload, type == "PROMPT_ECHO_ON");
        else if (type == "INFO")
            Q_EMIT info(payload);
        else if (type == "ERROR")
            Q_EMIT error(payload);
        else if (type == "RESULT") {
            m_resultSeen = true;
            m_result = payload == QLatin1String("ok");
        }
    }
}

void AuthSession::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    readFrames();
    const bool success = m_resultSeen && m_result && status == QProcess::NormalExit && exitCode == 0;
    // The outcome only; never prompts, messages or input.
    qCInfo(lcAuth) << "authentication" << (success ? "succeeded" : "failed");
    wipeBuffer();
    Q_EMIT finished(success);
}

void AuthSession::wipeBuffer()
{
    wipe(m_buffer);
}
