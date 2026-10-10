// SPDX-License-Identifier: GPL-3.0-or-later
#include "ToplevelModel.h"

ToplevelModel::ToplevelModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ToplevelModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_windows.size());
}

QVariant ToplevelModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_windows.size())
        return {};
    const Window &w = m_windows.at(index.row());
    switch (role) {
    case IdRole: return w.id;
    case Qt::DisplayRole:
    case TitleRole: return w.title;
    case AppIdRole: return w.appId;
    case ActiveRole: return w.active;
    case MinimizedRole: return w.minimized;
    case MaximizedRole: return w.maximized;
    case FullscreenRole: return w.fullscreen;
    case NeedsAttentionRole: return w.needsAttention;
    default: return {};
    }
}

QVariantMap ToplevelModel::get(int row) const
{
    QVariantMap out;
    if (row < 0 || row >= m_windows.size())
        return out;
    const auto roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        out.insert(QString::fromUtf8(it.value()), data(index(row), it.key()));
    return out;
}

QHash<int, QByteArray> ToplevelModel::roleNames() const
{
    return {
        {IdRole, "windowId"},
        {TitleRole, "title"},
        {AppIdRole, "appId"},
        {ActiveRole, "active"},
        {MinimizedRole, "minimized"},
        {MaximizedRole, "maximized"},
        {FullscreenRole, "fullscreen"},
        {NeedsAttentionRole, "needsAttention"},
    };
}

int ToplevelModel::rowForId(quint64 id) const
{
    for (int i = 0; i < m_windows.size(); ++i) {
        if (m_windows.at(i).id == id)
            return i;
    }
    return -1;
}

void ToplevelModel::taskButtonClicked(int row)
{
    if (row < 0 || row >= m_windows.size())
        return;
    const Window &w = m_windows.at(row);
    if (w.active && !w.minimized)
        minimize(row);
    else
        activate(row);
}

void ToplevelModel::activate(int row)
{
    if (row < 0 || row >= m_windows.size())
        return;
    const Window &w = m_windows.at(row);
    if (w.minimized)
        requestMinimized(w.id, false);
    requestActivate(w.id);
}

void ToplevelModel::minimize(int row)
{
    if (row >= 0 && row < m_windows.size())
        requestMinimized(m_windows.at(row).id, true);
}

void ToplevelModel::toggleMaximize(int row)
{
    if (row >= 0 && row < m_windows.size())
        requestMaximized(m_windows.at(row).id, !m_windows.at(row).maximized);
}

void ToplevelModel::close(int row)
{
    if (row >= 0 && row < m_windows.size())
        requestClose(m_windows.at(row).id);
}

void ToplevelModel::toggleShowDesktop()
{
    if (m_showingDesktop) {
        const QList<quint64> restore = m_desktopRestore;
        m_desktopRestore.clear();
        for (quint64 id : restore) {
            if (rowForId(id) >= 0) {
                requestMinimized(id, false);
                requestActivate(id);
            }
        }
        setShowingDesktop(false);
        return;
    }
    m_desktopRestore.clear();
    // Activate the previously active window last when restoring.
    quint64 activeId = 0;
    for (const Window &w : std::as_const(m_windows)) {
        if (w.minimized)
            continue;
        if (w.active)
            activeId = w.id;
        else
            m_desktopRestore.append(w.id);
        requestMinimized(w.id, true);
    }
    if (activeId)
        m_desktopRestore.append(activeId);
    setShowingDesktop(true);
}

void ToplevelModel::setShowingDesktop(bool showing)
{
    if (m_showingDesktop == showing)
        return;
    m_showingDesktop = showing;
    Q_EMIT showingDesktopChanged();
}

void ToplevelModel::addWindow(const Window &window)
{
    beginInsertRows({}, int(m_windows.size()), int(m_windows.size()));
    m_windows.append(window);
    endInsertRows();
    Q_EMIT countChanged();
    if (window.active && !window.minimized)
        setShowingDesktop(false);
}

void ToplevelModel::updateWindow(const Window &window)
{
    const int row = rowForId(window.id);
    if (row < 0)
        return;
    m_windows[row] = window;
    const QModelIndex idx = index(row);
    Q_EMIT dataChanged(idx, idx);
    // Showing the desktop ends once the user brings any window back.
    if (m_showingDesktop && window.active && !window.minimized && !m_desktopRestore.isEmpty()
        && !m_desktopRestore.contains(window.id)) {
        m_desktopRestore.clear();
        setShowingDesktop(false);
    }
}

void ToplevelModel::removeWindow(quint64 id)
{
    const int row = rowForId(id);
    if (row < 0)
        return;
    beginRemoveRows({}, row, row);
    m_windows.removeAt(row);
    endRemoveRows();
    m_desktopRestore.removeAll(id);
    Q_EMIT countChanged();
}

// --- Preview ---------------------------------------------------------------

PreviewToplevelModel::PreviewToplevelModel(QObject *parent)
    : ToplevelModel(parent)
{
}

quint64 PreviewToplevelModel::addPreviewWindow(const QString &title, const QString &appId, bool minimized)
{
    Window w;
    w.id = m_nextId++;
    w.title = title;
    w.appId = appId;
    w.minimized = minimized;
    addWindow(w);
    if (!minimized)
        makeActive(w.id);
    return w.id;
}

void PreviewToplevelModel::setNeedsAttention(int row, bool attention)
{
    if (row < 0 || row >= count())
        return;
    Window w = window(row);
    w.needsAttention = attention;
    updateWindow(w);
}

void PreviewToplevelModel::record(const QString &entry)
{
    m_log.append(entry);
    Q_EMIT logChanged();
}

void PreviewToplevelModel::makeActive(quint64 id)
{
    for (int i = 0; i < count(); ++i) {
        Window w = window(i);
        const bool active = w.id == id;
        if (w.active != active || (active && w.needsAttention)) {
            w.active = active;
            if (active)
                w.needsAttention = false;
            updateWindow(w);
        }
    }
}

void PreviewToplevelModel::requestActivate(quint64 id)
{
    record(QStringLiteral("activate %1").arg(id));
    makeActive(id);
}

void PreviewToplevelModel::requestMinimized(quint64 id, bool minimized)
{
    record(QStringLiteral("%1 %2").arg(minimized ? QStringLiteral("minimize") : QStringLiteral("unminimize")).arg(id));
    const int row = rowForId(id);
    if (row < 0)
        return;
    Window w = window(row);
    w.minimized = minimized;
    if (minimized)
        w.active = false;
    updateWindow(w);
}

void PreviewToplevelModel::requestMaximized(quint64 id, bool maximized)
{
    record(QStringLiteral("%1 %2").arg(maximized ? QStringLiteral("maximize") : QStringLiteral("unmaximize")).arg(id));
    const int row = rowForId(id);
    if (row < 0)
        return;
    Window w = window(row);
    w.maximized = maximized;
    updateWindow(w);
}

void PreviewToplevelModel::requestClose(quint64 id)
{
    record(QStringLiteral("close %1").arg(id));
    removeWindow(id);
}
