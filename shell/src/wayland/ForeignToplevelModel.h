// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ToplevelModel.h"

#include <QHash>
#include <memory>

class ForeignToplevelManager;
class ForeignToplevelHandle;

// ToplevelModel backed by zwlr_foreign_toplevel_manager_v1 (labwc, sway,
// wayfire...). Window states arrive in batches closed by `done`; requests go
// back through the handle (activate needs the shell's wl_seat).
class ForeignToplevelModel : public ToplevelModel
{
    Q_OBJECT

public:
    explicit ForeignToplevelModel(QObject *parent = nullptr);
    ~ForeignToplevelModel() override;

    bool available() const override;

protected:
    void requestActivate(quint64 id) override;
    void requestMinimized(quint64 id, bool minimized) override;
    void requestMaximized(quint64 id, bool maximized) override;
    void requestClose(quint64 id) override;

private:
    void handleAdded(ForeignToplevelHandle *handle);

    std::unique_ptr<ForeignToplevelManager> m_manager;
    QHash<quint64, ForeignToplevelHandle *> m_handles;
    quint64 m_nextId = 1;
};
