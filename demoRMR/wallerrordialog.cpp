#include "wallerrordialog.h"
#include "ui_wallerrordialog.h""

wallErrorDialog::wallErrorDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::wallErrorDialog)
{
    ui->setupUi(this);
}

wallErrorDialog::~wallErrorDialog()
{
    delete ui;
}

void wallErrorDialog::on_pushButton_Accept_clicked()
{
    // Len zavrie okno
    this->close();
}

void wallErrorDialog::on_pushButton_Delete_clicked()
{
    emit deleteRequested();
    this->close();
}
