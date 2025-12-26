#include "helpwindow.h"
#include "ui_helpwindow.h"

HelpWindow::HelpWindow(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::HelpWindow)
{
    ui->setupUi(this);

    this->setStyleSheet("border-image: url(:/ikonky/Help.png) 0 0 0 0 stretch stretch;");
}

HelpWindow::~HelpWindow()
{
    delete ui;
}
