// SPDX-License-Identifier: GPL-3.0-or-later
#include "ShellContext.h"

#include <QFileInfo>
#include <QGuiApplication>

#include "ipc/ShellIpc.h"
#include "models/AppRegistry.h"
#include "services/BatteryService.h"
#include "services/KeyboardLayoutService.h"
#include "services/NetworkService.h"
#include "services/NotificationServer.h"
#include "services/Settings.h"
#include "services/TrayModel.h"
#include "services/VolumeService.h"
#include "wayland/ToplevelModel.h"
#include "models/AppMenuModel.h"
#include "models/PlacesModel.h"
#include "models/RecentAppsModel.h"
#include "services/PowerService.h"
#include "services/RunService.h"
#include "services/SessionService.h"
#include "ui/ScreenGrabber.h"

#include <QJSEngine>
#include <QLoggingCategory>
#include <QTimer>

ShellContext *ShellContext::s_instance = nullptr;

ShellContext::ShellContext(const Services &services, bool preview, QObject *parent)
    : QObject(parent)
    , m_s(services)
    , m_preview(preview)
    , m_clock(new QTimer(this))
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    const QString fake = qEnvironmentVariable("MEADOW_FAKE_TIME");
    if (!fake.isEmpty()) {
        m_now = QDateTime::fromString(fake, Qt::ISODate);
        m_fakeTime = m_now.isValid();
    }
    m_clock->setSingleShot(true);
    connect(m_clock, &QTimer::timeout, this, &ShellContext::tick);
    if (!m_fakeTime)
        tick();
}

ShellContext::~ShellContext()
{
    s_instance = nullptr;
}

ShellContext *ShellContext::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    // Owned by main(), never by the QML engine.
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void ShellContext::tick()
{
    m_now = QDateTime::currentDateTime();
    Q_EMIT nowChanged();
    // Wake just after the next minute starts.
    const int msIntoMinute = m_now.time().second() * 1000 + m_now.time().msec();
    m_clock->start(60000 - msIntoMinute + 50);
}

void ShellContext::setStartMenuOpen(bool open)
{
    if (open == m_startMenuOpen)
        return;
    m_startMenuOpen = open;
    Q_EMIT startMenuOpenChanged();
}

QString ShellContext::userName()
{
    const QString user = qEnvironmentVariable("USER");
    return user.isEmpty() ? qEnvironmentVariable("LOGNAME") : user;
}

int ShellContext::keyboardModifiers() const
{
    Qt::KeyboardModifiers mods = QGuiApplication::queryKeyboardModifiers();
    // Test hook: labwc reports only its physical keyboards' modifiers when
    // focus moves, so a virtual keyboard's (wtype's) held Alt is invisible
    // there. While the file named by MEADOW_TEST_ALT_HELD_FILE exists, Alt
    // counts as held (tools/alttab-session-test.sh, held-Alt steps).
    static const QString altFile = qEnvironmentVariable("MEADOW_TEST_ALT_HELD_FILE");
    if (!altFile.isEmpty() && QFileInfo::exists(altFile))
        mods |= Qt::AltModifier;
    return int(mods);
}
