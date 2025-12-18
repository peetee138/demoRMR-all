#ifndef REPLAYDIALOG_H
#define REPLAYDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QTimer>
#include <QImage>

// OpenCV Include
#include <opencv2/opencv.hpp>

namespace Ui {
class ReplayDialog;
}

class ReplayDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReplayDialog(QWidget *parent = nullptr);
    ~ReplayDialog();

private slots:
    void on_pushButton_clicked();   // Select Folder
    void on_pushButton_2_clicked(); // Play
    void on_pushButton_3_clicked(); // Swap

    void playNextFrame();           // Slučka časovača

private:
    Ui::ReplayDialog *ui;

    long currentFrameMaster;
    long currentFrameSlave;

    // --- OpenCV Videá ---
    cv::VideoCapture capCamera;
    cv::VideoCapture capLidar;

    // --- Zobrazovacie plochy (Labely) ---
    QLabel *labelCamera;
    QLabel *labelLidar;

    // --- Časovač pre prehrávanie ---
    QTimer *timer;

    // --- Premenné pre synchronizáciu ---
    bool isCameraBig;
    bool mediaLoaded;

    double fpsCamera;
    double fpsLidar;
    long totalFramesCamera;
    long totalFramesLidar;

    // Ktoré video je "Master" (to kratšie, podľa ktorého ideme)
    // 0 = Camera je master, 1 = Lidar je master
    int masterVideo;
    double speedRatio; // Koľko snímkov "Slave" videa pripadá na 1 snímok "Master"
    double accumulator; // Pomocná premenná na preskakovanie snímkov

    // Pomocná funkcia na konverziu
    QImage matToQImage(const cv::Mat &mat);
};

#endif // REPLAYDIALOG_H
