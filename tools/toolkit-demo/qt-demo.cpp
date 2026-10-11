// SPDX-License-Identifier: GPL-3.0-or-later
// A still Qt Widgets window showing the controls the MeadowLuna Kvantum theme
// styles (tools/toolkit-screenshot.sh). Nothing animates, so screenshots of
// it are the same on every run.
//
//   meadow-qt-demo [--menu] [--icon-theme NAME] [--font "Fam1,Fam2"] [--font-px N]
//
// --menu opens the File menu once the window is shown. Prints "ready" when
// the window is drawn.
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCommandLineParser>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <cstdio>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption menuOpt(QStringLiteral("menu"), QStringLiteral("Open the File menu."));
    QCommandLineOption iconOpt(QStringLiteral("icon-theme"), QStringLiteral("Icon theme."), QStringLiteral("name"));
    QCommandLineOption fontOpt(QStringLiteral("font"), QStringLiteral("Comma-separated families."), QStringLiteral("families"));
    QCommandLineOption pxOpt(QStringLiteral("font-px"), QStringLiteral("Font pixel size."), QStringLiteral("px"));
    parser.addOptions({menuOpt, iconOpt, fontOpt, pxOpt});
    parser.process(app);
    if (parser.isSet(iconOpt))
        QIcon::setThemeName(parser.value(iconOpt));
    if (parser.isSet(fontOpt)) {
        QFont f;
        f.setFamilies(parser.value(fontOpt).split(QLatin1Char(',')));
        if (parser.isSet(pxOpt))
            f.setPixelSize(parser.value(pxOpt).toInt());
        QApplication::setFont(f);
    }

    QMainWindow win;
    win.setWindowTitle(QStringLiteral("Meadow Qt controls"));
    QMenu *file = win.menuBar()->addMenu(QStringLiteral("&File"));
    file->addAction(QIcon::fromTheme(QStringLiteral("document-new")), QStringLiteral("New"));
    file->addAction(QIcon::fromTheme(QStringLiteral("document-open")), QStringLiteral("Open…"));
    file->addSeparator();
    QAction *bar = file->addAction(QStringLiteral("Status Bar"));
    bar->setCheckable(true);
    bar->setChecked(true);
    file->addAction(QStringLiteral("Exit"));
    win.menuBar()->addMenu(QStringLiteral("&Edit"))->addAction(QStringLiteral("Copy"));
    win.menuBar()->addMenu(QStringLiteral("&View"));
    win.menuBar()->addMenu(QStringLiteral("&Help"));

    auto *central = new QWidget;
    auto *col = new QVBoxLayout(central);

    auto *buttons = new QHBoxLayout;
    auto *ok = new QPushButton(QStringLiteral("OK"));
    ok->setDefault(true);
    auto *apply = new QPushButton(QStringLiteral("Apply"));
    apply->setEnabled(false);
    auto *toggle = new QPushButton(QStringLiteral("Pressed"));
    toggle->setCheckable(true);
    toggle->setChecked(true);
    buttons->addWidget(ok);
    buttons->addWidget(new QPushButton(QStringLiteral("Cancel")));
    buttons->addWidget(apply);
    buttons->addWidget(toggle);
    buttons->addStretch();
    col->addLayout(buttons);

    auto *checks = new QHBoxLayout;
    auto *c1 = new QCheckBox(QStringLiteral("Unchecked"));
    auto *c2 = new QCheckBox(QStringLiteral("Checked"));
    c2->setChecked(true);
    auto *c3 = new QCheckBox(QStringLiteral("Mixed"));
    c3->setTristate(true);
    c3->setCheckState(Qt::PartiallyChecked);
    auto *c4 = new QCheckBox(QStringLiteral("Disabled"));
    c4->setChecked(true);
    c4->setEnabled(false);
    auto *r1 = new QRadioButton(QStringLiteral("Blue"));
    r1->setChecked(true);
    auto *r3 = new QRadioButton(QStringLiteral("Silver"));
    r3->setEnabled(false);
    for (QWidget *w : {static_cast<QWidget *>(c1), static_cast<QWidget *>(c2), static_cast<QWidget *>(c3),
                       static_cast<QWidget *>(c4), static_cast<QWidget *>(r1),
                       static_cast<QWidget *>(new QRadioButton(QStringLiteral("Olive"))), static_cast<QWidget *>(r3)})
        checks->addWidget(w);
    checks->addStretch();
    col->addLayout(checks);

    auto *inputs = new QHBoxLayout;
    auto *edit = new QLineEdit(QStringLiteral("Meadow"));
    auto *ro = new QLineEdit(QStringLiteral("Read only"));
    ro->setEnabled(false);
    auto *spin = new QSpinBox;
    spin->setValue(5);
    auto *combo = new QComboBox;
    combo->addItems({QStringLiteral("Luna Blue"), QStringLiteral("Olive Green"), QStringLiteral("Silver")});
    inputs->addWidget(edit);
    inputs->addWidget(ro);
    inputs->addWidget(spin);
    inputs->addWidget(combo);
    inputs->addStretch();
    col->addLayout(inputs);

    auto *ranges = new QHBoxLayout;
    auto *slider = new QSlider(Qt::Horizontal);
    slider->setValue(40);
    slider->setFixedWidth(160);
    auto *progress = new QProgressBar;
    progress->setValue(60);
    progress->setTextVisible(false);
    progress->setFixedWidth(200);
    ranges->addWidget(slider);
    ranges->addWidget(progress);
    ranges->addStretch();
    col->addLayout(ranges);

    auto *tabs = new QTabWidget;
    auto *tree = new QTreeWidget;
    tree->setHeaderLabels({QStringLiteral("Name"), QStringLiteral("Type"), QStringLiteral("Size")});
    tree->setRootIsDecorated(false);
    tree->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    const char *rows[][3] = {{"My Documents", "File Folder", "2 KB"}, {"Budget.ods", "Spreadsheet", "18 KB"},
                             {"Holiday.png", "PNG Image", "640 KB"}, {"Notes.txt", "Text Document", "1 KB"},
                             {"Setup.sh", "Shell Script", "4 KB"}, {"Meadow.svg", "SVG Image", "12 KB"},
                             {"Readme.md", "Markdown", "3 KB"}, {"Song.oga", "Audio", "2.1 MB"},
                             {"Report.pdf", "PDF Document", "96 KB"}, {"Archive.tar", "Archive", "1.4 MB"}};
    for (const auto &r : rows) {
        auto *item = new QTreeWidgetItem(tree, {QString::fromUtf8(r[0]), QString::fromUtf8(r[1]), QString::fromUtf8(r[2])});
        item->setIcon(0, QIcon::fromTheme(&r == &rows[0] ? QStringLiteral("folder") : QStringLiteral("text-x-generic")));
    }
    tree->setCurrentItem(tree->topLevelItem(1));
    tree->header()->resizeSection(0, 160);
    tabs->addTab(tree, QStringLiteral("General"));
    tabs->addTab(new QLabel(QStringLiteral("Details")), QStringLiteral("Details"));
    tabs->addTab(new QLabel(QStringLiteral("Sharing")), QStringLiteral("Sharing"));
    col->addWidget(tabs, 1);

    auto *group = new QGroupBox(QStringLiteral("Group"));
    auto *gl = new QVBoxLayout(group);
    gl->addWidget(new QLabel(QStringLiteral("Labels, frames and <a href=\"https://example.org/\">a link</a>")));
    col->addWidget(group);

    win.setCentralWidget(central);
    win.resize(720, 560);
    win.show();
    QTimer::singleShot(800, &win, [&] {
        if (parser.isSet(menuOpt)) {
            QMenuBar *mb = win.menuBar();
            file->popup(mb->mapToGlobal(mb->actionGeometry(file->menuAction()).bottomLeft()));
            QTimer::singleShot(400, &win, [file] {
                file->setActiveAction(file->actions().first());
                std::puts("ready");
                std::fflush(stdout);
            });
            return;
        }
        std::puts("ready");
        std::fflush(stdout);
    });
    return app.exec();
}
