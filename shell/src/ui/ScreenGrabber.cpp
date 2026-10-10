// SPDX-License-Identifier: GPL-3.0-or-later
#include "ScreenGrabber.h"

#include <QLoggingCategory>
#include <QProcess>
#include <QQuickWindow>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcGrab, "meadow.shell.grab")

ScreenGrabber::ScreenGrabber(QObject *parent)
    : QObject(parent)
{
}

QImage ScreenGrabber::toGray(const QImage &image)
{
    // Luma, then back to RGB so it can be cross-faded with the color image.
    return image.convertToFormat(QImage::Format_Grayscale8).convertToFormat(QImage::Format_RGB32);
}

void ScreenGrabber::setImage(const QImage &image)
{
    m_color = image.convertToFormat(QImage::Format_RGB32);
    m_gray = m_color.isNull() ? QImage() : toGray(m_color);
    ++m_revision;
    Q_EMIT readyChanged();
    Q_EMIT finished(!m_color.isNull());
}

void ScreenGrabber::grab(QQuickWindow *window)
{
    setImage(window ? window->grabWindow() : QImage());
}

void ScreenGrabber::grabOutput(const QString &output)
{
    const QString grim = QStandardPaths::findExecutable(QStringLiteral("grim"));
    if (grim.isEmpty()) {
        qCInfo(lcGrab) << "grim is not installed; no fade";
        setImage({});
        return;
    }
    if (m_process)
        m_process->kill();
    m_process = new QProcess(this);
    QProcess *p = m_process;
    connect(p, &QProcess::finished, this, [this, p](int code, QProcess::ExitStatus status) {
        p->deleteLater();
        if (m_process == p)
            m_process = nullptr;
        QImage image;
        if (status == QProcess::NormalExit && code == 0)
            image.loadFromData(p->readAllStandardOutput(), "PPM");
        if (image.isNull())
            qCInfo(lcGrab) << "screen capture failed; no fade";
        setImage(image);
    });
    // PPM: no compression, so the capture is quick.
    QStringList args{QStringLiteral("-t"), QStringLiteral("ppm")};
    if (!output.isEmpty())
        args << QStringLiteral("-o") << output;
    p->start(grim, args << QStringLiteral("-"));
}

void ScreenGrabber::clear()
{
    if (m_color.isNull())
        return;
    m_color = {};
    m_gray = {};
    Q_EMIT readyChanged();
}

ScreenImageProvider::ScreenImageProvider(ScreenGrabber *grabber)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_grabber(grabber)
{
}

QImage ScreenImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QImage image = m_grabber->image(id.startsWith(QLatin1String("gray")));
    if (!image.isNull() && requestedSize.isValid() && !requestedSize.isEmpty())
        image = image.scaled(requestedSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (size)
        *size = image.size();
    return image;
}
