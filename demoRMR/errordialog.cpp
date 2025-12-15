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
    // Len zavrie okno
    this->close();
}

void errorDialog::on_pushButton_Pomocka_clicked()
{
    // Vyšle signál do MainWindow (ktorý to pošle do Visualizera)
    emit requestZoneHighlight(true);
    // A zavrie okno
    this->close();
}
