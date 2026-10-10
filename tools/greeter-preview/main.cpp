// SPDX-License-Identifier: GPL-3.0-or-later
// meadow-greeter-preview: the Welcome screen without SDDM.
//
//   meadow-greeter-preview [--size 1920x1080] [--select 0] [--password wrong]
//                          [--caps] [--turnoff] [--secondary] [--rtl]
//                          [--config key=value ...] [--grab out.png]
//                          [--fingerprint enrolled:alice --fp-result success]
//
// Mock users: alice (picture, password), bart (long name, password),
// guest (no password). The mock accepts the password "meadow"; actions are
// reported on stdout as "sddm-mock: ...".
#include "MockSddm.h"
#include "ThemeLoader.h"

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QStyleHints>
#include <QImage>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QTimer>

#include <cstdio>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("meadow-greeter-preview"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Meadow Welcome screen with mock SDDM objects"));
    parser.addHelpOption();
    const QCommandLineOption themeOpt(QStringLiteral("theme"), QStringLiteral("Greeter theme directory."), QStringLiteral("dir"),
                                      QStringLiteral(MEADOW_SOURCE_DIR "/greeter"));
    const QCommandLineOption sizeOpt(QStringLiteral("size"), QStringLiteral("Window size WxH (default 1024x768)."), QStringLiteral("WxH"), QStringLiteral("1024x768"));
    const QCommandLineOption configOpt(QStringLiteral("config"), QStringLiteral("Override a theme.conf key (repeatable)."), QStringLiteral("key=value"));
    const QCommandLineOption selectOpt(QStringLiteral("select"), QStringLiteral("Select the user at <index>."), QStringLiteral("index"));
    const QCommandLineOption passwordOpt(QStringLiteral("password"), QStringLiteral("Submit <text> for the selected user."), QStringLiteral("text"));
    const QCommandLineOption capsOpt(QStringLiteral("caps"), QStringLiteral("Turn Caps Lock on."));
    const QCommandLineOption turnOffOpt(QStringLiteral("turnoff"), QStringLiteral("Open the Turn Off Computer dialog."));
    const QCommandLineOption secondaryOpt(QStringLiteral("secondary"), QStringLiteral("Render as a secondary screen."));
    const QCommandLineOption rtlOpt(QStringLiteral("rtl"), QStringLiteral("Right-to-left layout."));
    const QCommandLineOption noPowerOpt(QStringLiteral("no-power"), QStringLiteral("Report every power action as unavailable."));
    const QCommandLineOption usersOpt(QStringLiteral("extra-users"), QStringLiteral("Add <n> more users (scrolling)."), QStringLiteral("n"), QStringLiteral("0"));
    const QCommandLineOption grabOpt(QStringLiteral("grab"), QStringLiteral("Save a screenshot to <file> and exit."), QStringLiteral("file"));
    const QCommandLineOption delayOpt(QStringLiteral("delay"), QStringLiteral("Milliseconds before grabbing (default 800)."), QStringLiteral("ms"), QStringLiteral("800"));
    const QCommandLineOption fingerprintOpt(QStringLiteral("fingerprint"),
        QStringLiteral("Simulated fprintd: no-reader | unknown | enrolled:user,user."), QStringLiteral("spec"));
    const QCommandLineOption fpResultOpt(QStringLiteral("fp-result"),
        QStringLiteral("Outcome of a fingerprint reading: success | fail | pending (default fail)."), QStringLiteral("result"), QStringLiteral("fail"));
    const QCommandLineOption passwordDelayOpt(QStringLiteral("password-delay"),
        QStringLiteral("Milliseconds after selecting before --password is submitted (default 100)."), QStringLiteral("ms"), QStringLiteral("100"));
    const QCommandLineOption exitOpt(QStringLiteral("exit-after-ms"), QStringLiteral("Quit after <ms>."), QStringLiteral("ms"));
    parser.addOptions({themeOpt, sizeOpt, configOpt, selectOpt, passwordOpt, capsOpt, turnOffOpt, secondaryOpt, rtlOpt,
                       noPowerOpt, usersOpt, grabOpt, delayOpt, fingerprintOpt, fpResultOpt, passwordDelayOpt, exitOpt});
    parser.process(app);

    if (parser.isSet(rtlOpt))
        QGuiApplication::setLayoutDirection(Qt::RightToLeft);
    if (parser.isSet(grabOpt))
        ThemeLoader::instance()->setReducedMotion(true);
    if (parser.isSet(grabOpt))
        QGuiApplication::styleHints()->setCursorFlashTime(0); // steady text cursor in screenshots

    const QString themeDir = QDir(parser.value(themeOpt)).absolutePath();
    const QStringList size = parser.value(sizeOpt).split(QLatin1Char('x'));
    const QSize windowSize(size.value(0).toInt(), size.value(1).toInt());
    if (windowSize.isEmpty()) {
        std::fprintf(stderr, "bad --size\n");
        return 2;
    }

    const QString assets = qEnvironmentVariable("MEADOW_ASSET_DIR", QStringLiteral(MEADOW_SOURCE_DIR "/theme/assets"));
    QList<MockUser> users{
        {QStringLiteral("alice"), QStringLiteral("Alice Meadows"), QStringLiteral("file://") + assets + QStringLiteral("/avatar-kite.svg"), true},
        {QStringLiteral("bart"), QStringLiteral("Bartholomew Maximilian Longname-Fitzgerald"), QStringLiteral("file:///usr/share/sddm/faces/.face.icon"), true},
        {QStringLiteral("guest"), QString(), QStringLiteral("file:///usr/share/sddm/faces/.face.icon"), false},
    };
    for (int i = 0; i < parser.value(usersOpt).toInt(); ++i)
        users.append({QStringLiteral("user%1").arg(i + 1), QStringLiteral("Test User %1").arg(i + 1), QString(), true});

    // Must be set before QML creates its FingerprintQuery.
    if (parser.isSet(fingerprintOpt))
        qputenv("MEADOW_FINGERPRINT_SIMULATE", parser.value(fingerprintOpt).toUtf8());
    else if (!qEnvironmentVariableIsSet("MEADOW_FINGERPRINT_SIMULATE"))
        qputenv("MEADOW_FINGERPRINT_SIMULATE", "no-reader"); // never touch the host's fprintd

    MockGreeterProxy proxy(!parser.isSet(noPowerOpt));
    if (parser.isSet(fingerprintOpt) && parser.value(fingerprintOpt) != QLatin1String("no-reader"))
        proxy.setFingerprintResult(parser.value(fpResultOpt));
    MockUserModel userModel(users);
    MockSessionModel sessionModel;
    MockKeyboardModel keyboard;
    keyboard.setProperty("capsLock", parser.isSet(capsOpt));
    MockScreenModel screenModel(QRect(QPoint(0, 0), windowSize));
    MockThemeConfig config(themeDir + QStringLiteral("/theme.conf"), parser.values(configOpt));

    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    QQmlContext *ctx = view.rootContext();
    ctx->setContextProperty(QStringLiteral("sddm"), &proxy);
    ctx->setContextProperty(QStringLiteral("userModel"), &userModel);
    ctx->setContextProperty(QStringLiteral("sessionModel"), &sessionModel);
    ctx->setContextProperty(QStringLiteral("keyboard"), &keyboard);
    ctx->setContextProperty(QStringLiteral("screenModel"), &screenModel);
    ctx->setContextProperty(QStringLiteral("config"), &config);
    ctx->setContextProperty(QStringLiteral("primaryScreen"), !parser.isSet(secondaryOpt));
    ctx->setContextProperty(QStringLiteral("__sddm_errors"), QString());

    QObject::connect(&proxy, &MockGreeterProxy::loginSucceeded, &view, &QQuickView::close);
    view.resize(windowSize);
    view.setSource(QUrl::fromLocalFile(themeDir + QStringLiteral("/Main.qml")));
    if (view.status() != QQuickView::Ready) {
        for (const QQmlError &e : view.errors())
            std::fprintf(stderr, "%s\n", qPrintable(e.toString()));
        return 1;
    }
    view.show();
    view.requestActivate();

    QObject *root = view.rootObject();
    QTimer::singleShot(50, &app, [&] {
        if (parser.isSet(selectOpt))
            QMetaObject::invokeMethod(root, "selectUser", Q_ARG(QVariant, parser.value(selectOpt).toInt()));
        if (parser.isSet(turnOffOpt))
            QMetaObject::invokeMethod(root, "openTurnOffDialog");
        if (parser.isSet(passwordOpt)) {
            QTimer::singleShot(parser.value(passwordDelayOpt).toInt(), &app, [&] {
                QMetaObject::invokeMethod(root, "submitPassword", Q_ARG(QVariant, parser.value(passwordOpt)));
            });
        }
    });

    if (parser.isSet(grabOpt)) {
        const QString path = parser.value(grabOpt);
        QTimer::singleShot(parser.value(delayOpt).toInt(), &app, [&view, path] {
            const QImage image = view.grabWindow();
            QCoreApplication::exit(!image.isNull() && image.save(path) ? 0 : 1);
        });
    } else if (parser.isSet(exitOpt)) {
        QTimer::singleShot(parser.value(exitOpt).toInt(), &app, &QCoreApplication::quit);
    }
    return app.exec();
}
