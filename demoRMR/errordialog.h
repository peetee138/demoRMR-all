#ifndef ERRORDIALOG_H
#define ERRORDIALOG_H

#include <QDialog>

namespace Ui {
class errorDialog;
}

class errorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit errorDialog(QWidget *parent = nullptr);
    ~errorDialog();

signals:
     void requestZoneHighlight(bool enable);
private slots:
     void on_pushButton_Accept_clicked(); // Tlačidlo OK
     void on_pushButton_Pomocka_clicked(); // Tlačidlo Pomôcka
private:    
    Ui::errorDialog *ui;
};

#endif // ERRORDIALOG_H
