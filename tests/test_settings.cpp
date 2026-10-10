// SPDX-License-Identifier: GPL-3.0-or-later
// Placeholder until phase 6 implements Settings (shell.conf).
#include "services/Settings.h"

#include <QTest>

class TestSettings : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void constructs()
    {
        Settings settings;
        QVERIFY(settings.parent() == nullptr);
    }
};

QTEST_GUILESS_MAIN(TestSettings)
#include "test_settings.moc"
