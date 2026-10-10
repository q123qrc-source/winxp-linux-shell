// SPDX-License-Identifier: GPL-3.0-or-later
// meadow-decor-test --title "My Documents" --size 420x260 [--maximized]
// One plain window painted in the theme's window background, so screenshots
// show labwc's server-side Meadow decorations around a neutral client area.
#include "ThemeLoader.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QPainter>
#include <QRasterWindow>

namespace {
class PlainWindow : public QRasterWindow
{
public:
    explicit PlainWindow(const QString &text)
        : m_text(text) {}

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        ThemeLoader *t = ThemeLoader::instance();
        const QRect area(QPoint(0, 0), size());
        p.fillRect(area, QColor(t->token(QStringLiteral("colors.windowBackground")).toString()));
        p.setPen(QColor(t->token(QStringLiteral("colors.windowText")).toString()));
        p.setFont(t->font(QStringLiteral("ui")));
        const int margin = t->px(t->token(QStringLiteral("dialogs.padding")).toReal());
        p.drawText(area.adjusted(margin, margin, -margin, -margin), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_text);
    }

private:
    QString m_text;
};
} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setDesktopFileName(QStringLiteral("meadow-decor-test"));

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption titleOpt(QStringLiteral("title"), QStringLiteral("Window title."), QStringLiteral("text"), QStringLiteral("Untitled"));
    const QCommandLineOption sizeOpt(QStringLiteral("size"), QStringLiteral("Client size WxH."), QStringLiteral("WxH"), QStringLiteral("400x240"));
    const QCommandLineOption maxOpt(QStringLiteral("maximized"), QStringLiteral("Start maximized."));
    parser.addOptions({titleOpt, sizeOpt, maxOpt});
    parser.process(app);

    const QStringList wh = parser.value(sizeOpt).split(QLatin1Char('x'));
    PlainWindow window(QStringLiteral("Client area of “%1”.\nThe title bar, buttons and border are drawn by labwc.").arg(parser.value(titleOpt)));
    window.setTitle(parser.value(titleOpt));
    window.resize(wh.value(0).toInt(), wh.value(1).toInt());
    if (parser.isSet(maxOpt))
        window.showMaximized();
    else
        window.show();
    return app.exec();
}
