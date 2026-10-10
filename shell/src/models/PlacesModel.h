// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>

// Start menu right column: XDG user dirs, My Computer, Control Panel, ...
// Stub: implemented in phase 7.
class PlacesModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit PlacesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
};
