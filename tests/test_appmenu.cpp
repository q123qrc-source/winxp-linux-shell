// SPDX-License-Identifier: GPL-3.0-or-later
// Placeholder until phase 7 implements XDG menu parsing; it keeps the test
// target wired up so the real cases drop straight in.
#include "models/AppMenuModel.h"

#include <QTest>

class TestAppMenu : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emptyModel()
    {
        AppMenuModel model;
        QCOMPARE(model.rowCount(), 0);
    }
};

QTEST_GUILESS_MAIN(TestAppMenu)
#include "test_appmenu.moc"
