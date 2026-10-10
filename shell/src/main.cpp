// SPDX-License-Identifier: GPL-3.0-or-later
#include "ThemeLoader.h"
#include "ipc/ShellIpc.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QTimer>

Q_LOGGING_CATEGORY(lcShell, "meadow.shell")

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("meadow-shell"));
    QGuiApplication::setOrganizationName(QStringLiteral("meadow"));
    QGuiApplication::setApplicationVersion(QStringLiteral(MEADOW_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Meadow desktop shell"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption previewOpt(QStringLiteral("preview"),
        QStringLiteral("Run in a normal window without layer-shell (no root, any compositor)."));
    const QCommandLineOption themeOpt(QStringLiteral("theme"),
        QStringLiteral("Load the theme.json at <path> instead of the configured scheme."), QStringLiteral("path"));
    const QCommandLineOption exitAfterOpt(QStringLiteral("exit-after-ms"),
        QStringLiteral("Quit after <ms> milliseconds (used by smoke and screenshot tests)."), QStringLiteral("ms"));
    parser.addOptions({previewOpt, themeOpt, exitAfterOpt});
    parser.process(app);

    ThemeLoader *theme = ThemeLoader::instance();
    if (parser.isSet(themeOpt) && !theme->loadFile(parser.value(themeOpt)))
        return 1;
    if (theme->tokenMap().isEmpty()) {
        qCCritical(lcShell).noquote() << "no theme loaded:" << theme->errorString();
        return 1;
    }

    if (!parser.isSet(previewOpt))
        qCWarning(lcShell) << "layer-shell surfaces arrive in phase 6; running the preview window";

    // Keybind actions arrive over D-Bus (meadow-shellctl). The preview window
    // may run next to a real shell, so failing to claim the name is not fatal.
    ShellIpc ipc;
    ipc.registerOnSessionBus();

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Meadow.Shell", "Main");

    if (parser.isSet(exitAfterOpt))
        QTimer::singleShot(parser.value(exitAfterOpt).toInt(), &app, &QCoreApplication::quit);

    return app.exec();
}
