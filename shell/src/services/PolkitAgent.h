// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>
#include <memory>

// polkit authentication agent for the session, shown as the Luna
// "Authenticate" dialog (AuthenticateDialog.qml). Programs that need
// administrator rights (Date and Time, User Accounts, power actions with
// other users logged in, ...) ask polkit, which asks this agent.
//
// One request at a time: `active` while the dialog should show. The user
// picks an identity (an administrator account, or their own when allowed)
// and answers the prompt; a wrong password shows an error and asks again.
// Passwords go straight to polkit's helper and are never logged.
//
// Registration fails (and is logged) when another agent already serves the
// session. In --preview requests are simulated (requestPreview()) and the
// password "meadow" is accepted.
class PolkitAgent : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY changed)
    Q_PROPERTY(bool registered READ isRegistered NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString iconName READ iconName NOTIFY changed)
    Q_PROPERTY(QString actionId READ actionId NOTIFY changed)
    Q_PROPERTY(QStringList identities READ identities NOTIFY changed)
    Q_PROPERTY(int identityIndex READ identityIndex WRITE setIdentityIndex NOTIFY changed)
    Q_PROPERTY(QString prompt READ prompt NOTIFY changed)
    Q_PROPERTY(bool echo READ echo NOTIFY changed)
    Q_PROPERTY(QString feedback READ feedback NOTIFY changed)
    Q_PROPERTY(bool busy READ isBusy NOTIFY changed)

public:
    explicit PolkitAgent(bool simulate, QObject *parent = nullptr);
    ~PolkitAgent() override;

    // Registers with polkit for this session (not in --preview).
    bool registerForSession();

    bool isActive() const { return m_active; }
    bool isRegistered() const { return m_registered; }
    QString message() const { return m_message; }
    QString iconName() const { return m_iconName; }
    QString actionId() const { return m_actionId; }
    QStringList identities() const { return m_identityNames; }
    int identityIndex() const { return m_identityIndex; }
    void setIdentityIndex(int index);
    QString prompt() const { return m_prompt; }
    bool echo() const { return m_echo; }
    QString feedback() const { return m_feedback; }
    bool isBusy() const { return m_busy; }

    // The dialog's answer to the current prompt.
    Q_INVOKABLE void submit(const QString &response);
    // The dialog's Cancel.
    Q_INVOKABLE void cancel();
    // Preview: shows a sample request.
    Q_INVOKABLE void requestPreview();

    // Internal: called by the polkit listener.
    struct Request;
    void begin(std::unique_ptr<Request> request);
    void abort();

Q_SIGNALS:
    void changed();
    // A request finished (authorized or not).
    void finished(bool authorized);

private:
    void startSession();
    void endRequest(bool authorized);

    bool m_simulate;
    bool m_registered = false;
    bool m_active = false;
    bool m_busy = false;
    bool m_echo = false;
    QString m_message;
    QString m_iconName;
    QString m_actionId;
    QString m_prompt;
    QString m_feedback;
    QStringList m_identityNames;
    int m_identityIndex = 0;
    std::unique_ptr<Request> m_request;
    struct Impl;
    std::unique_ptr<Impl> d;
};
