// SPDX-License-Identifier: GPL-3.0-or-later
#include "RecentAppsModel.h"

RecentAppsModel::RecentAppsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int RecentAppsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 0;
}

QVariant RecentAppsModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(index)
    Q_UNUSED(role)
    return {};
}
