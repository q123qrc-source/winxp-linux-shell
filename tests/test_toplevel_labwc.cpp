// SPDX-License-Identifier: GPL-3.0-or-later
// ForeignToplevelModel against a real compositor: run by
// tools/with-headless-labwc.sh, it opens test windows and checks that the
// window list follows them and that activate/minimize/maximize/close and
// show-desktop requests reach labwc.
#include "wayland/ForeignToplevelModel.h"

#include <QGuiApplication>
#include <QProcess>
#include <QTest>

class TestToplevelLabwc : public QObject
{
    Q_OBJECT

    ForeignToplevelModel *m_model = nullptr;
    QList<QProcess *> m_clients;

    QProcess *open(const QString &title, const QString &appId)
    {
        auto *p = new QProcess(this);
        p->start(QStringLiteral(MEADOW_DECOR_TEST),
                 {QStringLiteral("--title"), title, QStringLiteral("--app-id"), appId, QStringLiteral("--size"), QStringLiteral("300x200")});
        m_clients << p;
        return p;
    }
    int rowOf(const QString &title) const
    {
        for (int i = 0; i < m_model->count(); ++i) {
            if (m_model->window(i).title == title)
                return i;
        }
        return -1;
    }
    bool state(const QString &title, bool ToplevelModel::Window::*field) const
    {
        const int row = rowOf(title);
        return row >= 0 && m_model->window(row).*field;
    }

private Q_SLOTS:
    void initTestCase()
    {
        if (!QGuiApplication::platformName().startsWith(QLatin1String("wayland")))
            QSKIP("needs a Wayland compositor (run through with-headless-labwc.sh)");
        m_model = new ForeignToplevelModel(this);
        QTRY_VERIFY(m_model->available());
    }

    void cleanupTestCase()
    {
        for (QProcess *p : std::as_const(m_clients)) {
            p->kill();
            p->waitForFinished();
        }
    }

    void listFollowsWindows()
    {
        open(QStringLiteral("Alpha"), QStringLiteral("org.example.alpha"));
        open(QStringLiteral("Beta"), QStringLiteral("org.example.beta"));
        open(QStringLiteral("Gamma"), QStringLiteral("org.example.alpha"));
        QTRY_COMPARE_WITH_TIMEOUT(m_model->count(), 3, 10000);
        QCOMPARE(m_model->window(rowOf(QStringLiteral("Beta"))).appId, QStringLiteral("org.example.beta"));
        QCOMPARE(m_model->window(rowOf(QStringLiteral("Gamma"))).appId, QStringLiteral("org.example.alpha"));
    }

    void activate()
    {
        m_model->activate(rowOf(QStringLiteral("Alpha")));
        QTRY_VERIFY(state(QStringLiteral("Alpha"), &ToplevelModel::Window::active));
        QVERIFY(!state(QStringLiteral("Gamma"), &ToplevelModel::Window::active));
    }

    void clickActiveMinimizes()
    {
        m_model->taskButtonClicked(rowOf(QStringLiteral("Alpha")));
        QTRY_VERIFY(state(QStringLiteral("Alpha"), &ToplevelModel::Window::minimized));
        // Clicking it again restores and focuses it.
        m_model->taskButtonClicked(rowOf(QStringLiteral("Alpha")));
        QTRY_VERIFY(!state(QStringLiteral("Alpha"), &ToplevelModel::Window::minimized));
        QTRY_VERIFY(state(QStringLiteral("Alpha"), &ToplevelModel::Window::active));
    }

    void maximize()
    {
        m_model->toggleMaximize(rowOf(QStringLiteral("Beta")));
        QTRY_VERIFY(state(QStringLiteral("Beta"), &ToplevelModel::Window::maximized));
        m_model->toggleMaximize(rowOf(QStringLiteral("Beta")));
        QTRY_VERIFY(!state(QStringLiteral("Beta"), &ToplevelModel::Window::maximized));
    }

    void showDesktop()
    {
        m_model->toggleShowDesktop();
        QTRY_VERIFY(state(QStringLiteral("Alpha"), &ToplevelModel::Window::minimized)
                    && state(QStringLiteral("Beta"), &ToplevelModel::Window::minimized)
                    && state(QStringLiteral("Gamma"), &ToplevelModel::Window::minimized));
        QVERIFY(m_model->showingDesktop());
        m_model->toggleShowDesktop();
        QTRY_VERIFY(!state(QStringLiteral("Alpha"), &ToplevelModel::Window::minimized)
                    && !state(QStringLiteral("Beta"), &ToplevelModel::Window::minimized)
                    && !state(QStringLiteral("Gamma"), &ToplevelModel::Window::minimized));
        // The window that was active before comes back on top.
        QTRY_VERIFY(state(QStringLiteral("Alpha"), &ToplevelModel::Window::active));
    }

    void close()
    {
        QProcess *beta = m_clients.at(1);
        m_model->close(rowOf(QStringLiteral("Beta")));
        QTRY_COMPARE(m_model->count(), 2);
        // The client got a close request and quit (its last window closed).
        QTRY_COMPARE_WITH_TIMEOUT(beta->state(), QProcess::NotRunning, 5000);
        QCOMPARE(rowOf(QStringLiteral("Beta")), -1);
    }

    void tenWindows()
    {
        for (int i = 0; i < 8; ++i)
            open(QStringLiteral("Window %1").arg(i + 1), QStringLiteral("org.example.many"));
        QTRY_COMPARE_WITH_TIMEOUT(m_model->count(), 10, 15000);
    }
};

QTEST_MAIN(TestToplevelLabwc)
#include "test_toplevel_labwc.moc"
