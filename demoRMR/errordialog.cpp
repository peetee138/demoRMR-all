#include "errordialog.h"
#include "ui_errordialog.h"

errorDialog::errorDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::errorDialog)
{
    ui->setupUi(this);
}

errorDialog::~errorDialog()
{
    delete ui;
}

void errorDialog::on_pushButton_Accept_clicked()
{
    this->close();
}

void errorDialog::on_pushButton_Pomocka_clicked()
{
    emit requestZoneHighlight(true);
    this->close();
}
