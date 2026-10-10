// SPDX-License-Identifier: GPL-3.0-or-later
#include "ForeignToplevelModel.h"

ForeignToplevelModel::ForeignToplevelModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ForeignToplevelModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 0;
}

QVariant ForeignToplevelModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(index)
    Q_UNUSED(role)
    return {};
}
