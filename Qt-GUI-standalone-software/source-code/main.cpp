#include "mainwindow.h"

#include <QApplication>
#include <QStyleFactory>


static void setDarkTheme(QApplication& app) {
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette p;
    p.setColor(QPalette::Window, QColor(53,53,53));
    p.setColor(QPalette::WindowText, QColor(220,220,220));
    p.setColor(QPalette::Base, QColor(35,35,35));
    p.setColor(QPalette::AlternateBase, QColor(53,53,53));
    p.setColor(QPalette::ToolTipBase, QColor(53,53,53));
    p.setColor(QPalette::ToolTipText, QColor(220,220,220));
    p.setColor(QPalette::Text, QColor(220,220,220));
    p.setColor(QPalette::Button, QColor(53,53,53));
    p.setColor(QPalette::ButtonText, QColor(220,220,220));
    p.setColor(QPalette::BrightText, QColor(255,0,0));
    p.setColor(QPalette::Highlight, QColor(42,130,218));
    p.setColor(QPalette::HighlightedText, QColor(255,255,255));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(127,127,127));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127,127,127));

    app.setPalette(p);

    // Optional: nicer tooltips on dark
    app.setStyleSheet(
        "QToolTip { color: #dddddd; background: #2a2a2a; border: 1px solid #444; }"
        );
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    setDarkTheme(a);
    MainWindow w;
    w.show();
    return a.exec();
}
