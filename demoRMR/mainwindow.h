#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#ifdef _WIN32
#define NOMINMAX
#include<windows.h>
#endif
#include<iostream>
#include <QDir>
//#include <QCloseEvent>
//#include<arpa/inet.h>
//#include<unistd.h>
//#include<sys/socket.h>
#include <QElapsedTimer>
#include<sys/types.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<vector>
#include <vector>
#include <QTableWidgetItem>
#include <QLabel>        // <--- PRIDANE
#include <QVBoxLayout>   // <--- PRIDANE
//#include "ckobuki.h"
//#include "rplidar.h"
#include "lidarvisualizer.h"
#include <QVBoxLayout> // Dôležité pre vloženie widgetu

#include "robot.h"
#ifndef DISABLE_JOYSTICK
#include <QJoysticks.h>

#endif
namespace Ui {
class MainWindow;
}

///toto je trieda s oknom.. ktora sa spusti ked sa spusti aplikacia.. su tu vsetky gombiky a spustania...
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    #ifndef DISABLE_OPENCV
    bool useCamera1;
    int actIndex;
    cv::Mat frame[3];
#endif

#ifndef DISABLE_SKELETON
    int updateSkeletonPicture;
        skeleton skeleJoints;
#endif
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

   private slots:
    void on_pushButton_9_clicked();

    void on_pushButton_2_clicked();

    void on_pushButton_3_clicked();

    void on_pushButton_6_clicked();

    void on_pushButton_5_clicked();

    void on_pushButton_4_clicked();

    void on_pushButton_clicked();

    void on_lineEdit_returnPressed();

    //void on_pushButton_10_clicked();
    void on_pushButton_12_clicked();
    //void on_pushButton_8_clicked();
    void updatePointsTable(const std::vector<MapPoint> &points);

    //void keyPressEvent(QKeyEvent *event);

    void on_pushButton_13_clicked();
    //void closeEvent(QCloseEvent *event) override;

    int paintThisLidar(const LaserMeasurement &laserData);
#ifndef DISABLE_OPENCV
    int paintThisCamera(const cv::Mat &cameraData);
#endif
#ifndef DISABLE_SKELETON
    int paintThisSkeleton(const skeleton &skeledata);
#endif
    //void on_pushButton_10_clicked();
    //void on_pushButton_8_clicked();
    void on_pushButton_7_clicked();

protected:
    void keyPressEvent(QKeyEvent *event) override; // <--- Toto pridaj do triedy



private:

    robot _robot;
    LidarVisualizer *lidarVis; // <--- PRIDAJ TOTO
    QLabel *cameraLabel;       // Widget pre zobrazenie kamery
    bool isLidarBig;           // Premenna stavu

    cv::VideoWriter videoWriter;
    bool recording = false;

    QString videoPath;          // vygeneruje sa automaticky v konstruktore
    QString photoPath;          // vygeneruje sa automaticky v konstruktore

    bool photoTaken = false;    // fotka sa uloží iba raz

    bool detectBall(const cv::Mat &frame);


    //--skuste tu nic nevymazat... pridavajte co chcete, ale pri odoberani by sa mohol stat nejaky drobny problem, co bude vyhadzovat chyby
    Ui::MainWindow *ui;
     //void paintEvent(QPaintEvent *event);// Q_DECL_OVERRIDE;
     int updateLaserPicture;
     LaserMeasurement copyOfLaserData;
         int datacounter;
     std::string ipaddress;


     QTimer *timer;
#ifndef DISABLE_JOYSTICK
     QJoysticks *instance;
#endif
  public slots:
     void setUiValues(double robotX,double robotY,double robotFi);
#ifndef DISABLE_AMCL
     void setUiAMCLValues(double robotX,double robotY,double robotFi);
#endif

};

#endif // MAINWINDOW_H
