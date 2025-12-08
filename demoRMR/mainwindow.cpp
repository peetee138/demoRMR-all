#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPainter>
#include <QDebug>
#include <math.h>
#include <QMessageBox>
#include <QKeyEvent>
#include "helpwindow.h"
#include <QHeaderView>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ipaddress="127.0.0.1";
    ui->setupUi(this);

    // --- 1. Inicializacia vizualizacnych prvkov ---

    // Lidar
    lidarVis = new LidarVisualizer(this);
    lidarVis->setRobot(&_robot);

    // Kamera (QLabel namiesto paintEvent)
    cameraLabel = new QLabel(this);
    cameraLabel->setAlignment(Qt::AlignCenter);
    cameraLabel->setStyleSheet("background-color: black; color: white;");
    cameraLabel->setText("Čakám na kameru...");
    cameraLabel->setScaledContents(false);

    // --- 2. Vytvorenie layoutov pre vsetky schranky (podla tvojho screenshotu) ---


    // A) VELKY STACK (stackedWidget)
    // Stranka "lidar"
    if(ui->lidar && !ui->lidar->layout()) {
        QVBoxLayout* l = new QVBoxLayout(ui->lidar);
        l->setContentsMargins(0,0,0,0);
    }
    // Stranka "camera"
    if(ui->camera && !ui->camera->layout()) {
        QVBoxLayout* l = new QVBoxLayout(ui->camera);
        l->setContentsMargins(0,0,0,0);
    }

    // B) MALY STACK (stackedWidget_2)
    // Stranka "smallLidar" (pre maly lidar)
    if(ui->smallLidar && !ui->smallLidar->layout()) {
        QVBoxLayout* l = new QVBoxLayout(ui->smallLidar);
        l->setContentsMargins(0,0,0,0);
    }
    // Stranka "smallCamera" (pre malu kameru)
    if(ui->smallCamera && !ui->smallCamera->layout()) {
        QVBoxLayout* l = new QVBoxLayout(ui->smallCamera);
        l->setContentsMargins(0,0,0,0);
    }

    // --- 3. Nastavenie pociatocneho stavu ---
    // Chceme: Lidar velky, Kamera mala
    isLidarBig = true;

    // Lidar -> Velky stack (stranka lidar)
    ui->lidar->layout()->addWidget(lidarVis);

    // Kamera -> Maly stack (stranka smallCamera)
    ui->smallCamera->layout()->addWidget(cameraLabel);

    // Nastavenie viditelnych stranok
    ui->stackedWidget->setCurrentWidget(ui->lidar);
    if(ui->stackedWidget_2) {
        ui->stackedWidget_2->setCurrentWidget(ui->smallCamera);
    }

    // --- Ostatne prepojenia ---
    connect(lidarVis, &LidarVisualizer::pointsUpdated, this, &MainWindow::updatePointsTable);
    connect(ui->pushButton_13, &QPushButton::clicked, this, &MainWindow::on_pushButton_13_clicked);

    QStringList headers;
    headers << "X" << "Y" << "Typ";
    ui->tableWidgetPoints->setColumnCount(3);
    ui->tableWidgetPoints->setHorizontalHeaderLabels(headers);
    ui->tableWidgetPoints->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    photoTaken = false;
    recording = false;
    QDateTime now = QDateTime::currentDateTime();
    QString timestamp = now.toString("yyyy_MM_dd_hh_mm_ss");
    videoPath = "C:/Users/petri/Downloads/kamera_kobuki/Zaznam/zaznam_" + timestamp + ".avi";
    photoPath = "C:/Users/petri/Downloads/kamera_kobuki/Fotka/fotka_lopty_" + timestamp + ".jpg";

    datacounter=0;
#ifndef DISABLE_OPENCV
    actIndex=-1;
    useCamera1=false;
#endif

    _robot.startLoging=false;
}

MainWindow::~MainWindow()
{
    delete ui;
}

// --- FUNKCIA NA PREPINANIE (SWAP) ---
void MainWindow::on_pushButton_clicked()
{
    // Bezpecnostna kontrola - ci mame widgety
    if(!lidarVis || !cameraLabel) return;

    // 1. "Odpojíme" widgety z ich aktualnych rodicov
    lidarVis->setParent(nullptr);
    cameraLabel->setParent(nullptr);

    // 2. Prehodime stav
    isLidarBig = !isLidarBig;

    if(isLidarBig)
    {
        // --- STAV A: Lidar Velky, Kamera Mala ---

        // Lidar -> Velky stack (stranka lidar)
        ui->lidar->layout()->addWidget(lidarVis);

        // Kamera -> Maly stack (stranka smallCamera)
        ui->smallCamera->layout()->addWidget(cameraLabel);

        // Prepni StackedWidgety
        ui->stackedWidget->setCurrentWidget(ui->lidar);
        if(ui->stackedWidget_2) ui->stackedWidget_2->setCurrentWidget(ui->smallCamera);

        ui->pushButton->setText("Swap: Cam->Big");
    }
    else
    {
        // --- STAV B: Kamera Velka, Lidar Maly ---

        // Kamera -> Velky stack (stranka camera)
        ui->camera->layout()->addWidget(cameraLabel);

        // Lidar -> Maly stack (stranka smallLidar)
        ui->smallLidar->layout()->addWidget(lidarVis);

        // Prepni StackedWidgety
        ui->stackedWidget->setCurrentWidget(ui->camera);
        if(ui->stackedWidget_2) ui->stackedWidget_2->setCurrentWidget(ui->smallLidar);

        ui->pushButton->setText("Swap: Lidar->Big");
    }

    // Prekreslenie pre istotu
    lidarVis->show();
    cameraLabel->show();
}

void MainWindow::updatePointsTable(const std::vector<MapPoint> &points)
{
    ui->tableWidgetPoints->setRowCount(0);
    for(const auto& p : points) {
        int row = ui->tableWidgetPoints->rowCount();
        ui->tableWidgetPoints->insertRow(row);
        ui->tableWidgetPoints->setItem(row, 0, new QTableWidgetItem(QString::number(p.x)));
        ui->tableWidgetPoints->setItem(row, 1, new QTableWidgetItem(QString::number(p.y)));
        QString typeStr = (p.type == POINT_BLUE) ? "Waypoint" : "Task";
        QTableWidgetItem *itemType = new QTableWidgetItem(typeStr);
        if(p.type == POINT_BLUE) itemType->setForeground(Qt::blue);
        else itemType->setForeground(Qt::magenta);
        ui->tableWidgetPoints->setItem(row, 2, itemType);
    }
}
void MainWindow::on_pushButton_13_clicked()
{
    std::vector<MapPoint> points = lidarVis->getPoints();
    if(points.empty()) {
        QMessageBox::warning(this, "Pozor", "Ziadne body na ulozenie!");
        return;
    }
    QString filename = "trasa_bot.txt";
    QFile file(filename);
    if(file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << "X;Y;TYP\n";
        for(const auto& p : points) {
            QString t = (p.type == POINT_BLUE) ? "WAYPOINT" : "TASK";
            out << p.x << ";" << p.y << ";" << t << "\n";
        }
        file.close();
        QMessageBox::information(this, "Uspech", "Body ulozene do " + filename);
    }
}

// Tento slot bol predtym pre tlacitko 8, mozes ho nechat alebo zmazat
void MainWindow::on_pushButton_12_clicked()
{
    on_pushButton_clicked(); // Pre istotu ho presmerujem na nas novy swap
}

// paintEvent UZ NIE JE POTREBNY - VYMAZANY (alebo zakomentovany)
// void MainWindow::paintEvent(QPaintEvent *event) { ... }

void MainWindow::setUiValues(double robotX,double robotY,double robotFi) {}

#ifndef DISABLE_AMCL
void MainWindow::setUiAMCLValues(double robotX, double robotY, double robotFi)
{
    ui->lineEdit_2->setText("X = " + QString::number(robotX/100));
    ui->lineEdit_3->setText("Y = " + QString::number(robotY/100));
    ui->lineEdit_4->setText("Fi = " + QString::number(robotFi));
}
#endif

void MainWindow::on_pushButton_9_clicked() // START
{
    QString zadany_text = ui->lineEdit->text();
    if (zadany_text.isEmpty()){
        this->ipaddress = "127.0.0.1";
    } else {
        this->ipaddress = zadany_text.toStdString();
    }

    connect(&_robot,SIGNAL(publishPosition(double,double,double)),this,SLOT(setUiValues(double,double,double)));
    connect(&_robot,SIGNAL(publishLidar(const LaserMeasurement &)),this,SLOT(paintThisLidar(const LaserMeasurement &)));
#ifndef DISABLE_OPENCV
    connect(&_robot,SIGNAL(publishCamera(const cv::Mat &)),this,SLOT(paintThisCamera(const cv::Mat &)));
#endif
#ifndef DISABLE_SKELETON
    connect(&_robot,SIGNAL(publishSkeleton(const skeleton &)),this,SLOT(paintThisSkeleton(const skeleton &)));
#endif
#ifndef DISABLE_AMCL
    connect(&_robot,SIGNAL(publishAMCLPosition(double,double,double)),this,SLOT(setUiAMCLValues(double,double,double)));
#endif
    _robot.initAndStartRobot(ipaddress);

#ifndef DISABLE_JOYSTICK
    instance = QJoysticks::getInstance();
    connect(instance, &QJoysticks::axisChanged,
            [this]( const int js, const int axis, const qreal value) {
                double forw=0, rot=0;
                if(axis==1){forw=-value*300;}
                if(axis==0){rot=-value*(3.14159/2.0);}
                this->_robot.setSpeedVal(forw,rot);
            });
#endif
}

void MainWindow::on_lineEdit_returnPressed() { on_pushButton_9_clicked(); }
void MainWindow::on_pushButton_2_clicked() { _robot.setSpeedVal(200,0); }
void MainWindow::on_pushButton_3_clicked() { _robot.setSpeedVal(-100,0); }
void MainWindow::on_pushButton_6_clicked() { _robot.setSpeedVal(0,3.14159/8); }
void MainWindow::on_pushButton_5_clicked() { _robot.setSpeedVal(0,-3.14159/8); }
void MainWindow::on_pushButton_4_clicked() { _robot.setSpeedVal(0,0); }

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if(!lidarVis) return;

    // Klavesova skratka P prehodí obrazovky
    if(event->key() == Qt::Key_P)
    {
        on_pushButton_clicked();
    }
    QMainWindow::keyPressEvent(event);
}

int MainWindow::paintThisLidar(const LaserMeasurement &laserData)
{
    if(lidarVis) {
        lidarVis->updateLidarData(laserData);
    }
    return 0;
}

#ifndef DISABLE_OPENCV
// --- ZOBRAZENIE KAMERY DO QLABEL ---
int MainWindow::paintThisCamera(const cv::Mat &cameraData)
{
    cv::Mat frameCopy;
    cameraData.copyTo(frameCopy);
    frameCopy.copyTo(frame[(actIndex+1)%3]);
    actIndex = (actIndex+1)%3;

    // --- 1. VYNÚTENIE POMERU 16:9 ---
    cv::resize(frameCopy, frameCopy, cv::Size(640, 360));

    // --- 2. Zobrazenie ---
    cv::Mat rgbFrame;
    cv::cvtColor(frameCopy, rgbFrame, cv::COLOR_BGR2RGB);
    QImage qimg((uchar*)rgbFrame.data, rgbFrame.cols, rgbFrame.rows, rgbFrame.step, QImage::Format_RGB888);

    if(cameraLabel) {
        QPixmap pix = QPixmap::fromImage(qimg);
        // Prispôsobí sa veľkosti widgetu (či už je veľký alebo malý)
        // a zachová pomer strán
        QPixmap scaledPix = pix.scaled(cameraLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        cameraLabel->setPixmap(scaledPix);
    }

    if (!recording) {
        int fps = 20;
        cv::Size size(frameCopy.cols, frameCopy.rows);
        if (!videoWriter.isOpened() && !videoPath.isEmpty()) {
            videoWriter.open(videoPath.toStdString(), cv::VideoWriter::fourcc('M','J','P','G'), fps, size, true);
            if(videoWriter.isOpened()) recording = true;
        }
    }
    if (recording && videoWriter.isOpened()) {
        videoWriter.write(frameCopy);
    }

    if (!photoTaken && detectBall(frameCopy)) {
        cv::imwrite(photoPath.toStdString(), frameCopy);
        photoTaken = true;
        if (recording) {
            recording = false;
            videoWriter.release();
        }
    }

    return 0;
}
#endif

#ifndef DISABLE_SKELETON
int MainWindow::paintThisSkeleton(const skeleton &skeledata)
{
    memcpy(&skeleJoints,&skeledata,sizeof(skeleton));
    updateSkeletonPicture=1;
    return 0;
}
#endif

bool MainWindow::detectBall(const cv::Mat &frame)
{
    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
    cv::Mat lowerRed, upperRed, redMask, yellowMask, mask;

    cv::inRange(hsv, cv::Scalar(0, 150, 80), cv::Scalar(10, 255, 255), lowerRed);
    cv::inRange(hsv, cv::Scalar(170, 150, 80), cv::Scalar(180, 255, 255), upperRed);
    redMask = lowerRed | upperRed;
    cv::inRange(hsv, cv::Scalar(15, 120, 120), cv::Scalar(35, 255, 255), yellowMask);
    mask = redMask | yellowMask;

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (auto &c : contours) {
        double area = cv::contourArea(c);
        if (area > 800) return true;
    }
    return false;
}

void MainWindow::on_pushButton_7_clicked()
{
    HelpWindow helpWind;
    helpWind.setModal(true);
    helpWind.exec();
}
