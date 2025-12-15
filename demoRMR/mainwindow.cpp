#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPainter>
#include <QDebug>
#include <math.h>
#include <QMessageBox>
#include <QKeyEvent>
#include "helpwindow.h"
#include <QHeaderView>
#include "errordialog.h"
#include "wallerrordialog.h"

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

    connect(lidarVis, &LidarVisualizer::collisionDetected, this, &MainWindow::showCollisionError);

    // Kamera (QLabel namiesto paintEvent)
    cameraLabel = new QLabel(this);
    cameraLabel->setAlignment(Qt::AlignCenter);
    cameraLabel->setStyleSheet("background-color: black; color: white;");
    cameraLabel->setText("Čakám na kameru...");
    cameraLabel->setScaledContents(false);

    // --- 2. Vytvorenie layoutov pre vsetky schranky (podla tvojho screenshotu) ---

    if (ui->widget_2->layout() == nullptr) {
        QVBoxLayout *layout = new QVBoxLayout(ui->widget_2);
        ui->widget_2->setLayout(layout);
    }

    // 2. Vytvoríme indikátor
    batteryVis = new BatteryIndicator(this);

    // 3. Pridáme ho do layoutu
    ui->widget_2->layout()->addWidget(batteryVis);


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

    // --- INICIALIZÁCIA NAVIGÁCIE ---
    state = IDLE;
    navTimer = new QTimer(this);
    // Prepojíme časovač s funkciou navigationLoop
    connect(navTimer, &QTimer::timeout, this, &MainWindow::navigationLoop);

    // --- Ostatne prepojenia ---
    connect(lidarVis, &LidarVisualizer::pointsUpdated, this, &MainWindow::updatePointsTable);
    connect(ui->pushButton_13, &QPushButton::clicked, this, &MainWindow::on_pushButton_13_clicked);
    connect(lidarVis, &LidarVisualizer::forbiddenZoneClicked, this, &MainWindow::showForbiddenError);
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
    isDarkMode = false; // Začíname v Light Mode
    updateTheme();
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
/*void MainWindow::on_pushButton_13_clicked()
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
}*/

void MainWindow::on_pushButton_13_clicked()
{
    // 1. Zoberieme body, ktoré si naklikal
    navigationPoints = lidarVis->getPoints();

    if(navigationPoints.empty()) {
        QMessageBox::warning(this, "Pozor", "Žiadne body!");
        return;
    }

    // --- NOVÁ KONTROLA: Sú body prepojené? ---
    if (!lidarVis->isPathDrawActive()) {
        QMessageBox::warning(this, "Pozor", "Trasa nie je skontrolovaná!\nStlačte najprv tlačidlo 'Kontrola'");
        return;
    }

    if (lidarVis->getLastCheckedCount() == 0) {
        QMessageBox::warning(this, "Pozor", "Žiadne body!");
        return;
    }
    // -----------------------------------------

    // 2. Nastavíme počiatočný stav
    currentPointIndex = 0;
    if(lidarVis) {
        lidarVis->setCurrentIndex(0);
    }

    state = MOVING;

    // 3. Spustíme časovač (cyklus pobeží každých 50ms)
    if(!navTimer->isActive()) {
        navTimer->start(50);
    }

    qDebug() << "Startujem navigaciu. Pocet bodov:" << navigationPoints.size()<< " Povolene len po index:" << lidarVis->getLastCheckedCount();
}

void MainWindow::on_pushButton_8_clicked()
{
    if(lidarVis) {
        // Zapneme kreslenie čiar (prepne sa to na true)
        // Ak by si to chcel ako "toggle" (zapnúť/vypnúť), musel by si si v lidarvisualizer urobiť get metódu,
        // ale pre začiatok stačí natvrdo zapnúť.
        lidarVis->togglePathDrawing(true);
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

    robot_X = robotX;
    robot_Y = robotY;
    robot_Fi = robotFi;

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

    connect(&_robot, &robot::publishBattery, batteryVis, &BatteryIndicator::setBatteryLevel);
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
void MainWindow::navigationLoop()
{
    // Statická premenná si pamätá hodnotu medzi volaniami funkcie (plynulosť)
    static double current_linear_speed = 0.0;

    // Lokálna premenná pre otáčanie (tu rampu nechceme)
    double angular_speed = 0;

    // --- 0. NOTAUS ---
    if (notaus == true) {
        // Okamžité zastavenie
        _robot.setSpeedVal(0, 0);

        // Resetujeme pamäť rýchlosti -> po odbrzdení pôjde od nuly
        current_linear_speed = 0.0;

        return;
    }

    if(state == IDLE) {
        navTimer->stop();
        _robot.setSpeedVal(0, 0);
        current_linear_speed = 0.0;
        return;
    }

    // Zistíme, koľko bodov bolo skutočne prepojených čiarou
    int allowedPoints = 0;
    if (lidarVis) {
        allowedPoints = lidarVis->getLastCheckedCount();
        int crashIndex = lidarVis->getFirstCollisionIndex();
        if (crashIndex != -2) {
            int safeLimit = crashIndex + 1;
            if (safeLimit < allowedPoints) {
                allowedPoints = safeLimit;
            }
        }
        std::vector<MapPoint> freshPoints = lidarVis->getPoints();
        if (freshPoints.size() != navigationPoints.size() || freshPoints.size() > 0) { // aktualizacia bodov
            navigationPoints = freshPoints;
        }
    }

    // --- 1. Kontrola konca trasy ---
    if (currentPointIndex >= navigationPoints.size()||currentPointIndex>=allowedPoints) {
        qDebug() << "Koniec trasy.";
        state = IDLE;
        _robot.setSpeedVal(0,0);
        current_linear_speed = 0.0;
        return;
    }

    MapPoint pt = navigationPoints[currentPointIndex];

    // Prevod bodu z mapy na mm
    double targetX = pt.x * 100.0;
    double targetY = pt.y * 100.0;

    // --- 2. Výpočet chýb ---
    double dy = robot_X - targetX;
    double dx = robot_Y - targetY;
    double distance = sqrt(dx*dx + dy*dy);

    double targetAngle = atan2(dy, dx);
    double errorAngle = targetAngle - robot_Fi;

    while (errorAngle > M_PI) errorAngle -= 2 * M_PI;
    while (errorAngle < -M_PI) errorAngle += 2 * M_PI;

    // --- 3. Konštanty ---
    const double tolerance_pos = 50.0;
    const double Kp_angle = 1.8;
    const double Kp_dist = 1.0;

    // RAMPA: O koľko zrýchliť/spomaliť za 50ms
    // Ak dáš 10, tak z 0 na 300 sa dostane za 1.5 sekundy (30 krokov)
    // Ak dáš 25, bude to ostrejšie (cca 0.6 sekundy)
    const double RAMP_STEP = 10.0;

    // --- 4. Rozhodovací strom ---

    // A) Sme v cieli?
    if (distance < tolerance_pos && state != ROTATING)
    {
        if (pt.type == POINT_PURPLE) {
            _robot.setSpeedVal(0, 0);
            current_linear_speed = 0.0; // Reset rýchlosti pred rotáciou
            state = ROTATING;
            totalRotatedAngle = 0.0;
            lastRobotTheta = robot_Fi;
            qDebug() << "Bod dosiahnuty (TASK). Zacinam rotovat.";
            return;
        }
        else {
            currentPointIndex++;
            if(lidarVis) lidarVis->setCurrentIndex(currentPointIndex);

            if (currentPointIndex >= navigationPoints.size()) {
                state = IDLE;
                _robot.setSpeedVal(0, 0);
                current_linear_speed = 0.0;
                qDebug() << "Ciel dosiahnuty. Koniec.";
            } else {
                qDebug() << "Bod dosiahnuty. Pokracujem plynule na dalsi.";
            }
            return;
        }
    }

    // B) Sme vo fáze pohybu (MOVING)
    if (state == MOVING)
    {
        double required_linear = 0.0;

        // Ak je uhol veľký, stojíme a len točíme
        if (fabs(errorAngle) > M_PI / 18.0) // 10 stupňov
        {
            required_linear = 0;
            angular_speed = Kp_angle * errorAngle;
        }
        else
        {
            // Vypočítame koľko by sme CHCELI ísť
            required_linear = Kp_dist * distance * cos(errorAngle);
            angular_speed = Kp_angle * errorAngle;
        }

        // --- TU JE RAMPOVANIE ---

        // 1. Orezanie žiadaného maxima (aby sme neakcelerovali k 1000ke)
        if (required_linear > 300) required_linear = 300;
        if (required_linear < 0) required_linear = 0; // Žiadne cúvanie!

        // 2. Postupné pridávanie/uberanie (Rampa)
        if (current_linear_speed < required_linear) {
            current_linear_speed += RAMP_STEP;
            // Aby sme neprestrelili
            if (current_linear_speed > required_linear) current_linear_speed = required_linear;
        }
        else if (current_linear_speed > required_linear) {
            current_linear_speed -= RAMP_STEP;
            // Aby sme nepodstrelili
            if (current_linear_speed < required_linear) current_linear_speed = required_linear;
        }

        // 3. Poistka (pre istotu)
        if (current_linear_speed < 0) current_linear_speed = 0;

        // Limity uhlovej rýchlosti (bez rampy)
        if (angular_speed > 3.1415/4) angular_speed = 3.1415/4;
        if (angular_speed < -3.1415/4) angular_speed = -3.1415/4;

        _robot.setSpeedVal(current_linear_speed, angular_speed);
    }

    // C) Sme vo fáze otáčania na mieste (ROTATING)
    else if (state == ROTATING)
    {
        // Tu sa uistíme, že lineárna je 0
        current_linear_speed = 0.0;

        double delta = robot_Fi - lastRobotTheta;
        while (delta > M_PI) delta -= 2 * M_PI;
        while (delta < -M_PI) delta += 2 * M_PI;

        totalRotatedAngle += fabs(delta);
        lastRobotTheta = robot_Fi;

        if (totalRotatedAngle >= 2 * M_PI - 0.2) {
            state = MOVING;
            currentPointIndex++;
            if(lidarVis) lidarVis->setCurrentIndex(currentPointIndex);

            _robot.setSpeedVal(0, 0);
            qDebug() << "Rotacia dokoncena. Idem na dalsi bod.";
        } else {
            _robot.setSpeedVal(0, 0.5);
        }
    }
}

void MainWindow::on_pushButton_7_clicked()
{
    HelpWindow helpWind;
    helpWind.setModal(true);
    helpWind.exec();
}

void MainWindow::on_pushButton_15_clicked(){
    notaus = !notaus;

    if(notaus){
        _robot.setSpeedVal(0,0);
    }else
        notaus = false;
}

void MainWindow::showForbiddenError()
{
    errorDialog dlg(this);

    // Prepojíme signál z Dialogu (tlačidlo Pomocka) priamo na slot Visualizera (toggleWallHighlight)
    // Použijeme lambdu alebo priame prepojenie, ak je visualizer dostupný
    connect(&dlg, &errorDialog::requestZoneHighlight, lidarVis, &LidarVisualizer::toggleWallHighlight);

    dlg.exec(); // Zobrazí sa modálne (čaká)
}
/*void MainWindow::showCollisionError()
{
    // Vytvoríme a zobrazíme dialóg
    errorDialog dlg;
    dlg.setModal(true); // Aby sa nedalo klikať inde kým nezavrieš okno
    dlg.exec(); // Zobrazí okno a čaká na zavretie
}*/
void MainWindow::showCollisionError()
{
    // Použijeme správny dialog pre stenu
    wallErrorDialog dlg(this);
    dlg.setModal(true);

    // PREPOJENIE:
    // Keď v dialogu klikneš DELETE -> zavolá sa vo Visualizeri funkcia na mazanie
    connect(&dlg, &wallErrorDialog::deleteRequested, lidarVis, &LidarVisualizer::removeInvalidPoints);

    dlg.exec();
}

void MainWindow::on_pushButton_10_clicked()
{
    // Prepnutie stavu
    isDarkMode = !isDarkMode;

    // Zmena textu na tlacidle podla stavu
    if(isDarkMode) ui->pushButton_10->setText("Light Mode");
    else ui->pushButton_10->setText("Dark Mode");

    // Aplikovanie stylu
    updateTheme();
}

void MainWindow::updateTheme()
{
    QString style;

    if (isDarkMode) {
        // --- DARK MODE ---
        style = R"(
            QMainWindow, QWidget {
                background-color: #2b2b2b;
                color: #ffffff;
            }
            QPushButton {
                background-color: #404040;
                border: 1px solid #555;
                border-radius: 5px;
                padding: 5px;
                color: #ffffff;
            }
            QPushButton:hover {
                background-color: #505050;
            }
            QPushButton:pressed {
                background-color: #252525;
            }
            QLineEdit {
                background-color: #1e1e1e;
                border: 1px solid #555;
                color: #ffffff;
            }
            QTableWidget {
                background-color: #1e1e1e;
                color: #ffffff;
                gridline-color: #444;
            }
            QHeaderView::section {
                background-color: #404040;
                color: #ffffff;
                border: 1px solid #555;
            }
            QLabel {
                color: #ffffff;
            }
        )";
    } else {
        // --- LIGHT MODE ---
        style = R"(
            QMainWindow, QWidget {
                background-color: #f0f0f0;
                color: #000000;
            }
            QPushButton {
                background-color: #e0e0e0;
                border: 1px solid #a0a0a0;
                border-radius: 5px;
                padding: 5px;
                color: #000000;
            }
            QPushButton:hover {
                background-color: #d0d0d0;
            }
            QPushButton:pressed {
                background-color: #b0b0b0;
            }
            QLineEdit {
                background-color: #ffffff;
                border: 1px solid #ccc;
                color: #000000;
            }
            QTableWidget {
                background-color: #ffffff;
                color: #000000;
                gridline-color: #ccc;
            }
            QHeaderView::section {
                background-color: #e0e0e0;
                color: #000000;
                border: 1px solid #ccc;
            }
            QLabel {
                color: #000000;
            }
        )";
    }

    // Aplikujeme štýl na celé okno
    this->setStyleSheet(style);

    // --- ŠPECIÁLNE VÝNIMKY ---

    // Kamera musí ostať čierna, inak by biele pozadie rušilo obraz
    if(cameraLabel) {
        cameraLabel->setStyleSheet("background-color: black; color: white; border: 2px solid gray;");
    }

    // Lidar visualizer má vlastné kreslenie (čierne pozadie v paintEvent),
    // takže stylesheet ho neovplyvní negatívne, ale môžeme mu nastaviť border
    if(lidarVis) {
        // Lidar si pozadie riesi sam, tu len resetneme dedicnost ak treba
        lidarVis->setStyleSheet("background-color: black;");
    }
}
