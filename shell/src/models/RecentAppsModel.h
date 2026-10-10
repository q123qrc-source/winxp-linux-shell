// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>

// Most-frequently-used apps and pinned Start menu items, persisted.
// Stub: implemented in phase 7.
class RecentAppsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit RecentAppsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
};
