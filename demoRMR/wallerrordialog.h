#ifndef WALLERRORDIALOG_H
#define WALLERRORDIALOG_H

#include <QDialog>

namespace Ui {
class wallErrorDialog;
}

class wallErrorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit wallErrorDialog(QWidget *parent = nullptr);
    ~wallErrorDialog();
signals:
    void deleteRequested();
private slots:
    void on_pushButton_Accept_clicked(); // Tlačidlo OK
    void on_pushButton_Delete_clicked(); // Tlačidlo Delete
private:
    Ui::wallErrorDialog *ui;
};

#endif // WALLERRORDIALOG_H
