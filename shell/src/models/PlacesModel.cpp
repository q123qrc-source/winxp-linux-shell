// SPDX-License-Identifier: GPL-3.0-or-later
#include "PlacesModel.h"

PlacesModel::PlacesModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PlacesModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 0;
}

QVariant PlacesModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(index)
    Q_UNUSED(role)
    return {};
}
