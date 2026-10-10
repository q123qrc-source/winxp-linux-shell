// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QStringList>

// The taskbar's window list. Subclasses talk to the compositor
// (ForeignToplevelModel, wlr-foreign-toplevel-management) or simulate one
// (PreviewToplevelModel, for --preview and tests); this class owns the rows
// and the taskbar semantics shared by both.
class ToplevelModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool showingDesktop READ showingDesktop NOTIFY showingDesktopChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        AppIdRole,
        ActiveRole,
        MinimizedRole,
        MaximizedRole,
        FullscreenRole,
        NeedsAttentionRole,
    };
    Q_ENUM(Roles)

    struct Window {
        quint64 id = 0;
        QString title;
        QString appId;
        bool active = false;
        bool minimized = false;
        bool maximized = false;
        bool fullscreen = false;
        bool needsAttention = false;
    };

    explicit ToplevelModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_windows.size()); }
    bool showingDesktop() const { return m_showingDesktop; }
    // False when the compositor offers no window list (no taskbar buttons).
    virtual bool available() const { return true; }

    int rowForId(quint64 id) const;
    // Row data keyed by role name, for QML code outside a delegate.
    Q_INVOKABLE QVariantMap get(int row) const;
    const Window &window(int row) const { return m_windows.at(row); }

    // Taskbar button click: the active window minimizes, any other window
    // is restored (if minimized) and activated.
    Q_INVOKABLE void taskButtonClicked(int row);
    Q_INVOKABLE void activate(int row);
    Q_INVOKABLE void minimize(int row);
    Q_INVOKABLE void toggleMaximize(int row);
    Q_INVOKABLE void close(int row);
    // Win+D: minimize every window; again restores the ones it minimized.
    Q_INVOKABLE void toggleShowDesktop();

Q_SIGNALS:
    void countChanged();
    void showingDesktopChanged();
    void availableChanged();

protected:
    // Compositor requests, implemented by subclasses.
    virtual void requestActivate(quint64 id) = 0;
    virtual void requestMinimized(quint64 id, bool minimized) = 0;
    virtual void requestMaximized(quint64 id, bool maximized) = 0;
    virtual void requestClose(quint64 id) = 0;

    // Row bookkeeping for subclasses.
    void addWindow(const Window &window);
    void updateWindow(const Window &window);
    void removeWindow(quint64 id);

private:
    void setShowingDesktop(bool showing);

    QList<Window> m_windows;
    bool m_showingDesktop = false;
    // Windows Win+D minimized, in stacking order (last one is activated last).
    QList<quint64> m_desktopRestore;
};

// In-memory window list: --preview, screenshots and QML tests. Requests change
// the state immediately and are recorded in `log` ("activate 3", ...).
class PreviewToplevelModel : public ToplevelModel
{
    Q_OBJECT
    Q_PROPERTY(QStringList log READ log NOTIFY logChanged)

public:
    explicit PreviewToplevelModel(QObject *parent = nullptr);

    // Adds a window and returns its id; the newest one becomes active.
    Q_INVOKABLE quint64 addPreviewWindow(const QString &title, const QString &appId, bool minimized = false);
    Q_INVOKABLE void setNeedsAttention(int row, bool attention);
    QStringList log() const { return m_log; }

Q_SIGNALS:
    void logChanged();

protected:
    void requestActivate(quint64 id) override;
    void requestMinimized(quint64 id, bool minimized) override;
    void requestMaximized(quint64 id, bool maximized) override;
    void requestClose(quint64 id) override;

private:
    void record(const QString &entry);
    void makeActive(quint64 id);

    quint64 m_nextId = 1;
    QStringList m_log;
};
