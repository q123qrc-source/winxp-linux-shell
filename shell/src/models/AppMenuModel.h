// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>

// XDG menu spec + .desktop parsing into the "All Programs" tree.
// Stub: implemented in phase 7.
class AppMenuModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit AppMenuModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
};
