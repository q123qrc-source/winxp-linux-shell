// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>

// Taskbar window list via wlr-foreign-toplevel-management-unstable-v1.
// Stub: implemented in phase 6.
class ForeignToplevelModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit ForeignToplevelModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
};
