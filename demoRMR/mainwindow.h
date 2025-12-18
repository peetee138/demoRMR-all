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
#include "batteryindicator.h" // <--- PRIDAT
#include <QDir>

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
public slots:
    void receiveFrontLidarPoints(const std::vector<double> &uhol, const std::vector<double> &vzdialenost);

private slots:
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();
    void on_pushButton_3_clicked();
    void on_pushButton_4_clicked();
    void on_pushButton_5_clicked();
    void on_pushButton_6_clicked();

    void on_pushButton_8_clicked();
    void on_pushButton_9_clicked();
    void on_pushButton_10_clicked();
    void on_pushButton_11_clicked();
    void on_pushButton_12_clicked();
    void on_pushButton_13_clicked();

    void on_pushButton_15_clicked();

    void on_lineEdit_returnPressed();

    void updatePointsTable(const std::vector<MapPoint> &points);

    void showForbiddenError();

    //void keyPressEvent(QKeyEvent *event);

    void recordLidarFrame(); // Slot, ktorý sa bude volat 20x za sekundu
    //void closeEvent(QCloseEvent *event) override;

    void recordStatsFrame();

    void navigationLoop(); // Slot, ktorý sa bude volať každých 50ms

    int paintThisLidar(const LaserMeasurement &laserData);

    void onRowHeaderClicked(int index);
    void onCellClicked(int row, int column);
#ifndef DISABLE_OPENCV
    int paintThisCamera(const cv::Mat &cameraData);
#endif
#ifndef DISABLE_SKELETON
    int paintThisSkeleton(const skeleton &skeledata);
#endif

    //void on_pushButton_8_clicked();
    void on_pushButton_7_clicked();

protected:
    void keyPressEvent(QKeyEvent *event) override; // <--- Toto pridaj do triedy



private:
    bool prekazkaActive = false;

    QLabel *labelX;
    QLabel *labelY;
    QLabel *labelFi;

    double getDistanceToBall(double angleRad);

    std::vector<double> uhol_update;
    std::vector<double> vzdialenost_update;

    // --- NAHRÁVANIE ---
    cv::VideoWriter videoWriterCamera; // Premenoval som pre prehľadnosť
    cv::VideoWriter videoWriterLidar;  // Nový writer pre lidar

    cv::VideoWriter videoWriterStats;   // Premenované z Battery na Stats
    cv::Size statsVideoSize;            // Rozmery

    QTimer *lidarRecordTimer;          // Časovač pre snímanie lidaru
    cv::Size lidarVideoSize;

    bool recording;
    bool koniecMisie; // Premenná, ktorú si chcel

    void startRecording(); // Funkcia na vytvorenie priečinka a start
    void stopRecording();  // Funkcia na ukončenie a uloženie


    bool isDarkMode;
    void updateTheme();

    bool notaus = false;

    QTimer *navTimer;           // Časovač pre riadenie pohybu
    std::vector<MapPoint> navigationPoints; // Zoznam bodov na prejdenie
    int currentPointIndex;      // Ktorý bod práve riešime

    void showCollisionError();
    // Stavy navigácie
    enum NavState {
        IDLE,       // Stojí
        MOVING,     // Hýbe sa k bodu
        ROTATING    // Robí task (otočku)
    };
    NavState state;

    double totalRotatedAngle;
    double lastRobotTheta;

    robot _robot;
    LidarVisualizer *lidarVis; // <--- PRIDAJ TOTO
    QLabel *cameraLabel;       // Widget pre zobrazenie kamery
    bool isLidarBig;           // Premenna stavu

    QString videoPath;          // vygeneruje sa automaticky v konstruktore
    QString photoPath;          // vygeneruje sa automaticky v konstruktore

    bool photoTaken = false;    // fotka sa uloží iba raz

    bool detectBall(const cv::Mat &frame, float &outRadius, cv::Point &outCenter);


    double robot_X;
    double robot_Y;
    double robot_Fi;


    BatteryIndicator *batteryVis;
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
