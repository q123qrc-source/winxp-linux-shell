// SPDX-License-Identifier: GPL-3.0-or-later
#include "AppMenuModel.h"

AppMenuModel::AppMenuModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AppMenuModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 0;
}

QVariant AppMenuModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(index)
    Q_UNUSED(role)
    return {};
}
