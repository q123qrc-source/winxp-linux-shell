// SPDX-License-Identifier: GPL-3.0-or-later
#include "ShellIpc.h"
#include "shelladaptor.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcIpc, "meadow.shell.ipc")

ShellIpc::ShellIpc(QObject *parent)
    : QObject(parent)
{
    new ShellAdaptor(this);
}

bool ShellIpc::registerOnSessionBus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCWarning(lcIpc) << "no session bus; keybind actions are unavailable";
        return false;
    }
    if (!bus.registerObject(QString::fromLatin1(kPath), this)) {
        qCWarning(lcIpc) << "could not export" << kPath;
        return false;
    }
    if (!bus.registerService(QString::fromLatin1(kService))) {
        qCWarning(lcIpc).noquote() << "could not own" << kService << "- is another meadow-shell running?"
                                   << bus.lastError().message();
        bus.unregisterObject(QString::fromLatin1(kPath));
        return false;
    }
    qCInfo(lcIpc) << "listening on" << kService;
    return true;
}

void ShellIpc::ToggleStartMenu()
{
    qCInfo(lcIpc) << "ToggleStartMenu";
    Q_EMIT toggleStartMenuRequested();
}

void ShellIpc::ShowRunDialog()
{
    qCInfo(lcIpc) << "ShowRunDialog";
    Q_EMIT runDialogRequested();
}

void ShellIpc::ShowDesktop()
{
    qCInfo(lcIpc) << "ShowDesktop";
    Q_EMIT showDesktopRequested();
}

void ShellIpc::Lock()
{
    qCInfo(lcIpc) << "Lock";
    Q_EMIT lockRequested();
}

void ShellIpc::ShowLogOffDialog()
{
    qCInfo(lcIpc) << "ShowLogOffDialog";
    Q_EMIT logOffDialogRequested();
}

void ShellIpc::ShowTurnOffDialog()
{
    qCInfo(lcIpc) << "ShowTurnOffDialog";
    Q_EMIT turnOffDialogRequested();
}

void ShellIpc::NextKeyboardLayout()
{
    qCInfo(lcIpc) << "NextKeyboardLayout";
    Q_EMIT nextKeyboardLayoutRequested();
}

void ShellIpc::SwitchWindow(int direction)
{
    qCInfo(lcIpc) << "SwitchWindow" << direction;
    Q_EMIT switchWindowRequested(direction < 0 ? -1 : 1);
}

void ShellIpc::ShowControlPanel(const QString &page)
{
    qCInfo(lcIpc) << "ShowControlPanel" << page;
    Q_EMIT controlPanelRequested(page);
}

void ShellIpc::ShowDisplayProperties(const QString &tab)
{
    qCInfo(lcIpc) << "ShowDisplayProperties" << tab;
    Q_EMIT displayPropertiesRequested(tab);
}

void ShellIpc::setLocked(bool locked)
{
    if (m_locked == locked)
        return;
    m_locked = locked;
    Q_EMIT LockedChanged(locked);
}

QString ShellIpc::Version() const
{
    return QStringLiteral(MEADOW_VERSION);
}
