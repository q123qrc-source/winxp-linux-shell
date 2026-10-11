// SPDX-License-Identifier: GPL-3.0-or-later
#include "PolkitAgent.h"

#include <PolkitQt1/Agent/Listener>
#include <PolkitQt1/Agent/Session>
#include <PolkitQt1/Details>
#include <PolkitQt1/Identity>
#include <PolkitQt1/Subject>

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QPointer>

#include <pwd.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcPolkit, "meadow.shell.polkit")

namespace {
const char *kObjectPath = "/org/meadow/PolicyKit1/AuthenticationAgent";

QString displayName(const PolkitQt1::Identity &identity)
{
    const QString s = identity.toString();
    if (s.startsWith(QLatin1String("unix-user:"))) {
        bool ok = false;
        const uid_t uid = s.mid(10).toUInt(&ok);
        const passwd *pw = ok ? getpwuid(uid) : nullptr;
        if (pw) {
            // "Real Name (login)" when the account has a full name.
            const QString gecos = QString::fromLocal8Bit(pw->pw_gecos).section(QLatin1Char(','), 0, 0);
            const QString login = QString::fromLocal8Bit(pw->pw_name);
            return gecos.isEmpty() ? login : QStringLiteral("%1 (%2)").arg(gecos, login);
        }
        return s.mid(10);
    }
    return s;
}
} // namespace

struct PolkitAgent::Request
{
    QString actionId;
    QString message;
    QString iconName;
    QString cookie;
    PolkitQt1::Identity::List identities;
    PolkitQt1::Agent::AsyncResult *result = nullptr;
};

class PolkitListener : public PolkitQt1::Agent::Listener
{
public:
    explicit PolkitListener(PolkitAgent *agent) : PolkitQt1::Agent::Listener(agent), m_agent(agent) {}

    void initiateAuthentication(const QString &actionId, const QString &message, const QString &iconName,
                                const PolkitQt1::Details &, const QString &cookie,
                                const PolkitQt1::Identity::List &identities,
                                PolkitQt1::Agent::AsyncResult *result) override
    {
        auto request = std::make_unique<PolkitAgent::Request>();
        request->actionId = actionId;
        request->message = message;
        request->iconName = iconName;
        request->cookie = cookie;
        request->identities = identities;
        request->result = result;
        qCInfo(lcPolkit) << "authentication requested for" << actionId;
        m_agent->begin(std::move(request));
    }
    bool initiateAuthenticationFinish() override { return true; }
    void cancelAuthentication() override
    {
        qCInfo(lcPolkit) << "authentication cancelled by polkit";
        m_agent->abort();
    }

private:
    PolkitAgent *m_agent;
};

struct PolkitAgent::Impl
{
    PolkitListener *listener = nullptr;
    QPointer<PolkitQt1::Agent::Session> session;
};

PolkitAgent::PolkitAgent(bool simulate, QObject *parent)
    : QObject(parent)
    , m_simulate(simulate)
    , d(std::make_unique<Impl>())
{
}

PolkitAgent::~PolkitAgent()
{
    if (d->session)
        d->session->cancel();
}

bool PolkitAgent::registerForSession()
{
    if (m_simulate)
        return false;
    // Only for a logind session (not, e.g., a nested test compositor).
    const PolkitQt1::UnixSessionSubject subject(QCoreApplication::applicationPid());
    if (!subject.subject()) {
        qCInfo(lcPolkit) << "no login session; not registering as the authentication agent";
        return false;
    }
    d->listener = new PolkitListener(this);
    m_registered = d->listener->registerListener(subject, QString::fromLatin1(kObjectPath));
    if (m_registered)
        qCInfo(lcPolkit) << "authentication agent registered";
    else
        qCWarning(lcPolkit) << "could not register as the authentication agent (another agent may be running)";
    Q_EMIT changed();
    return m_registered;
}

void PolkitAgent::begin(std::unique_ptr<Request> request)
{
    if (m_active)
        abort(); // polkit serializes requests; a new one replaces a stale one
    m_request = std::move(request);
    m_actionId = m_request->actionId;
    m_message = m_request->message;
    m_iconName = m_request->iconName;
    m_identityNames.clear();
    m_identityIndex = 0;
    const uid_t me = getuid();
    for (int i = 0; i < m_request->identities.size(); ++i) {
        const PolkitQt1::Identity &id = m_request->identities.at(i);
        m_identityNames << displayName(id);
        // Prefer the user's own account when polkit allows it.
        if (id.toString() == QStringLiteral("unix-user:%1").arg(me))
            m_identityIndex = i;
    }
    m_feedback.clear();
    m_active = true;
    Q_EMIT changed();
    startSession();
}

void PolkitAgent::startSession()
{
    m_prompt.clear();
    m_echo = false;
    m_busy = true;
    Q_EMIT changed();
    if (m_simulate) {
        m_prompt = tr("Password:");
        m_busy = false;
        Q_EMIT changed();
        return;
    }
    if (!m_request || m_identityIndex < 0 || m_identityIndex >= m_request->identities.size())
        return;
    if (d->session) {
        d->session->cancel();
        d->session->deleteLater();
    }
    auto *session = new PolkitQt1::Agent::Session(m_request->identities.at(m_identityIndex), m_request->cookie, nullptr, this);
    d->session = session;
    connect(session, &PolkitQt1::Agent::Session::request, this, [this](const QString &request, bool echo) {
        m_prompt = request;
        m_echo = echo;
        m_busy = false;
        Q_EMIT changed();
    });
    // polkit text is shown, never logged.
    connect(session, &PolkitQt1::Agent::Session::showError, this, [this](const QString &text) {
        m_feedback = text;
        Q_EMIT changed();
    });
    connect(session, &PolkitQt1::Agent::Session::showInfo, this, [this](const QString &text) {
        m_feedback = text;
        Q_EMIT changed();
    });
    connect(session, &PolkitQt1::Agent::Session::completed, this, [this, session](bool authorized) {
        session->deleteLater();
        if (authorized) {
            endRequest(true);
            return;
        }
        // Wrong password: say so and ask again, like the XP dialog.
        qCInfo(lcPolkit) << "authentication failed; asking again";
        m_feedback = tr("The password is incorrect. Please try again.");
        startSession();
    });
    session->initiate();
}

void PolkitAgent::setIdentityIndex(int index)
{
    if (index == m_identityIndex || index < 0 || index >= m_identityNames.size())
        return;
    m_identityIndex = index;
    m_feedback.clear();
    Q_EMIT changed();
    if (m_active)
        startSession();
}

void PolkitAgent::submit(const QString &response)
{
    if (!m_active || m_busy)
        return;
    m_busy = true;
    m_feedback.clear();
    Q_EMIT changed();
    if (m_simulate) {
        if (response == QLatin1String("meadow")) {
            endRequest(true);
        } else {
            m_feedback = tr("The password is incorrect. Please try again.");
            startSession();
        }
        return;
    }
    if (d->session)
        d->session->setResponse(response);
}

void PolkitAgent::cancel()
{
    if (!m_active)
        return;
    qCInfo(lcPolkit) << "authentication dismissed";
    if (d->session) {
        d->session->cancel();
        d->session->deleteLater();
    }
    if (m_request && m_request->result) {
        m_request->result->setError(tr("Authentication was cancelled."));
        m_request->result->setCompleted();
        m_request->result = nullptr;
    }
    endRequest(false);
}

void PolkitAgent::abort()
{
    if (d->session) {
        d->session->cancel();
        d->session->deleteLater();
    }
    if (m_request && m_request->result) {
        m_request->result->setCompleted();
        m_request->result = nullptr;
    }
    endRequest(false);
}

void PolkitAgent::endRequest(bool authorized)
{
    if (m_request && m_request->result) {
        m_request->result->setCompleted();
        m_request->result = nullptr;
    }
    m_request.reset();
    m_active = false;
    m_busy = false;
    m_prompt.clear();
    m_feedback.clear();
    qCInfo(lcPolkit) << "authentication" << (authorized ? "granted" : "not granted");
    Q_EMIT changed();
    Q_EMIT finished(authorized);
}

void PolkitAgent::requestPreview()
{
    if (!m_simulate)
        return;
    m_actionId = QStringLiteral("org.freedesktop.timedate1.set-timezone");
    m_message = tr("Authentication is required to set the system timezone.");
    m_iconName = QStringLiteral("preferences-system-time");
    m_identityNames = {QStringLiteral("Meadow Admin (admin)")};
    m_identityIndex = 0;
    m_feedback.clear();
    m_request = std::make_unique<Request>();
    m_active = true;
    Q_EMIT changed();
    startSession();
}
