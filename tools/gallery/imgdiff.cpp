// SPDX-License-Identifier: GPL-3.0-or-later
// meadow-imgdiff EXPECTED ACTUAL [--channel-tolerance N] [--max-ratio R] [--diff OUT]
//
// Compares two images pixel by pixel. A pixel differs when any channel differs
// by more than the channel tolerance (default 24). Fails (exit 1) when the
// images have different sizes or more than max-ratio of the pixels differ
// (default 0.0005 = 0.05%), which absorbs small font-rasterizer changes
// while still catching a recolored border on a handful of controls.
// --diff writes a visualization: differing pixels red over a faded copy.
#include <QCoreApplication>
#include <QImage>
#include <QStringList>

#include <cstdio>
#include <cstdlib>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments().mid(1);
    int tolerance = 24;
    double maxRatio = 0.0005;
    QString diffPath;
    QStringList files;
    for (int i = 0; i < args.size(); ++i) {
        if (args[i] == QLatin1String("--channel-tolerance") && i + 1 < args.size())
            tolerance = args[++i].toInt();
        else if (args[i] == QLatin1String("--max-ratio") && i + 1 < args.size())
            maxRatio = args[++i].toDouble();
        else if (args[i] == QLatin1String("--diff") && i + 1 < args.size())
            diffPath = args[++i];
        else
            files << args[i];
    }
    if (files.size() != 2) {
        std::fprintf(stderr, "usage: meadow-imgdiff EXPECTED ACTUAL [--channel-tolerance N] [--max-ratio R] [--diff OUT]\n");
        return 2;
    }
    const QImage a = QImage(files[0]).convertToFormat(QImage::Format_ARGB32);
    const QImage b = QImage(files[1]).convertToFormat(QImage::Format_ARGB32);
    if (a.isNull() || b.isNull()) {
        std::fprintf(stderr, "meadow-imgdiff: cannot read %s\n", qPrintable(a.isNull() ? files[0] : files[1]));
        return 2;
    }
    if (a.size() != b.size()) {
        std::printf("size mismatch: expected %dx%d, got %dx%d\n", a.width(), a.height(), b.width(), b.height());
        return 1;
    }

    QImage diff = diffPath.isEmpty() ? QImage() : a.copy();
    qint64 differing = 0;
    for (int y = 0; y < a.height(); ++y) {
        const QRgb *pa = reinterpret_cast<const QRgb *>(a.constScanLine(y));
        const QRgb *pb = reinterpret_cast<const QRgb *>(b.constScanLine(y));
        QRgb *pd = diff.isNull() ? nullptr : reinterpret_cast<QRgb *>(diff.scanLine(y));
        for (int x = 0; x < a.width(); ++x) {
            const bool differs = std::abs(qRed(pa[x]) - qRed(pb[x])) > tolerance
                || std::abs(qGreen(pa[x]) - qGreen(pb[x])) > tolerance
                || std::abs(qBlue(pa[x]) - qBlue(pb[x])) > tolerance
                || std::abs(qAlpha(pa[x]) - qAlpha(pb[x])) > tolerance;
            if (differs)
                ++differing;
            if (pd) {
                const int g = qGray(pa[x]);
                pd[x] = differs ? qRgb(255, 0, 0) : qRgb(128 + g / 2, 128 + g / 2, 128 + g / 2);
            }
        }
    }
    const double ratio = double(differing) / (double(a.width()) * a.height());
    std::printf("%lld of %d pixels differ (%.4f%%, limit %.4f%%)\n", static_cast<long long>(differing),
                a.width() * a.height(), ratio * 100, maxRatio * 100);
    if (!diffPath.isEmpty() && differing > 0)
        diff.save(diffPath);
    return ratio > maxRatio ? 1 : 0;
}
