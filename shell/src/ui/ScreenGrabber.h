// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QImage>
#include <QObject>
#include <QQuickImageProvider>
#include <QtQml/qqmlregistration.h>

class QProcess;
class QQuickWindow;

// The picture behind the Log Off and Turn Off dialogs: a capture of the
// screen in color and in grayscale, which the dialog cross-fades (XP's
// "fade to gray"). In a session the capture comes from `grim` (wlr-screencopy);
// in --preview from the preview window. If capturing fails, `ready` stays
// false and the dialog falls back to a dark overlay. Captures live in memory
// only and are dropped on clear().
//
// QML image sources: image://screen/color?<revision>, image://screen/gray?<revision>.
class ScreenGrabber : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(int revision READ revision NOTIFY readyChanged)

public:
    explicit ScreenGrabber(QObject *parent = nullptr);

    bool ready() const { return !m_color.isNull(); }
    int revision() const { return m_revision; }
    QImage image(bool gray) const { return gray ? m_gray : m_color; }

    // Captures `window` (preview). Emits finished(ok).
    Q_INVOKABLE void grab(QQuickWindow *window);
    // Captures one output (QScreen::name, e.g. "DP-1"; empty: all) with grim.
    Q_INVOKABLE void grabOutput(const QString &output);
    Q_INVOKABLE void clear();

    static QImage toGray(const QImage &image);

Q_SIGNALS:
    void readyChanged();
    void finished(bool ok);

private:
    void setImage(const QImage &image);

    QImage m_color;
    QImage m_gray;
    int m_revision = 0;
    QProcess *m_process = nullptr;
};

class ScreenImageProvider : public QQuickImageProvider
{
public:
    explicit ScreenImageProvider(ScreenGrabber *grabber);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    ScreenGrabber *m_grabber;
};
