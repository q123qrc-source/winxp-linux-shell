// SPDX-License-Identifier: GPL-3.0-or-later
// meadow-gallery: shows every Meadow.Controls control in every state.
//
//   meadow-gallery [--scheme luna-olive] [--scale 2] [--grab out.png]
//
// With --grab it renders once, saves the window to a PNG and exits; that is
// how tools/screenshot-test.py produces and checks golden images.
#include "ThemeLoader.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QStyleHints>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("meadow-gallery"));
    QGuiApplication::setApplicationVersion(QStringLiteral(MEADOW_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Meadow control gallery"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption schemeOpt(QStringLiteral("scheme"), QStringLiteral("Color scheme id."), QStringLiteral("id"));
    const QCommandLineOption scaleOpt(QStringLiteral("scale"), QStringLiteral("Scale factor (1, 1.25, 1.5, 2)."), QStringLiteral("factor"));
    const QCommandLineOption grabOpt(QStringLiteral("grab"), QStringLiteral("Save a screenshot to <file> and exit."), QStringLiteral("file"));
    const QCommandLineOption delayOpt(QStringLiteral("delay"), QStringLiteral("Milliseconds to wait before grabbing (default 500)."), QStringLiteral("ms"), QStringLiteral("500"));
    parser.addOptions({schemeOpt, scaleOpt, grabOpt, delayOpt});
    parser.process(app);

    ThemeLoader *theme = ThemeLoader::instance();
    if (parser.isSet(schemeOpt) && !theme->loadScheme(parser.value(schemeOpt)))
        return 1;
    if (theme->tokenMap().isEmpty())
        return 1;
    if (parser.isSet(scaleOpt))
        theme->setScale(parser.value(scaleOpt).toDouble());
    if (parser.isSet(grabOpt))
        theme->setReducedMotion(true); // Screenshots must not catch an animation mid-way.
    if (parser.isSet(grabOpt))
        QGuiApplication::styleHints()->setCursorFlashTime(0); // steady text cursor in screenshots

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Meadow.Gallery", "Gallery");
    if (engine.rootObjects().isEmpty())
        return 1;

    if (parser.isSet(grabOpt)) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        if (!window)
            return 1;
        const QString path = parser.value(grabOpt);
        QTimer::singleShot(parser.value(delayOpt).toInt(), &app, [window, path] {
            const QImage image = window->grabWindow();
            if (image.isNull() || !image.save(path)) {
                qCritical("meadow-gallery: could not save %s", qPrintable(path));
                QCoreApplication::exit(1);
                return;
            }
            QCoreApplication::exit(0);
        });
    }
    return app.exec();
}
