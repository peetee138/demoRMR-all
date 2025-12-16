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

    // Inicializácia premenných
    recording = false;
    koniecMisie = false;

    // Časovač pre nahrávanie Lidaru (napr. 20 FPS = 50ms)
    lidarRecordTimer = new QTimer(this);
    connect(lidarRecordTimer, &QTimer::timeout, this, &MainWindow::recordLidarFrame);

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

    startRecording(); //nahravie spustene

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
    // OCHRANA: Ak sú dáta prázdne, nerob nič
    if (cameraData.empty() || cameraData.cols <= 0 || cameraData.rows <= 0) return 0;

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
        QPixmap scaledPix = pix.scaled(cameraLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        cameraLabel->setPixmap(scaledPix);
    }

    // --- 3. Nahrávanie videa ---
    if (recording && videoWriterCamera.isOpened()) {
        videoWriterCamera.write(frameCopy);
    }

    // --- 4. DETEKCIA LOPTY A ULOŽENIE FOTKY ---
    // Kontrolujeme !photoTaken, aby sme uložili fotku a zastavili misiu iba RAZ
    if (!photoTaken && detectBall(frameCopy)) {

        qDebug() << "Lopta nájdená! Začínam proces ukončenia.";

        // A) Príprava cesty a názvu súboru
        QString saveDir = "C:/Users/petri/Downloads/kamera_kobuki/Fotka/";
        QDir dir(saveDir);
        if (!dir.exists()) {
            dir.mkpath("."); // Vytvorí priečinok ak neexistuje
        }

        QDateTime now = QDateTime::currentDateTime();
        QString timestamp = now.toString("yyyy_MM_dd_hh_mm_ss");
        QString finalPhotoPath = saveDir + "fotka_lopty_" + timestamp + ".jpg";

        // B) Uloženie fotografie
        bool saved = cv::imwrite(finalPhotoPath.toStdString(), frameCopy);
        if(saved) {
            qDebug() << "Fotografia úspešne uložená:" << finalPhotoPath;
        } else {
            qDebug() << "CHYBA: Fotografia sa nepodarila uložiť!";
        }

        // C) Nastavenie príznaku, že už sme loptu našli
        photoTaken = true;

        // D) Zastavenie nahrávania a robota
        if (recording) {
            stopRecording(); // Toto zavrie video súbory
        }

        state = IDLE;             // Zastaví navigačnú slučku
        _robot.setSpeedVal(0, 0); // Zastaví fyzicky robota

        // E) Informácia pre užívateľa
        QMessageBox::information(this, "Misia Úspešná", "Lopta bola nájdená!\nFotografia uložená.\nMisia ukončená.");
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
    cv::Mat lowerRed1, upperRed1, lowerRed2, upperRed2, redMask;

    cv::inRange(hsv, cv::Scalar(0, 150, 80), cv::Scalar(10, 255, 255), lowerRed1);
    cv::inRange(hsv, cv::Scalar(170, 150, 80), cv::Scalar(180, 255, 255), lowerRed2);

    redMask = lowerRed1 | lowerRed2;

    //hough transformacia
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(5, 5),2,2);

    //detegovanie kruhov
    std::vector<cv::Vec3f> circles;

    //parametre
    // dp = 1 (rozlíšenie), minDist = frame.rows/8 (min. vzdialenosť medzi kruhmi)
    // param1 = 100 (Canny edge threshold), param2 = 30 (Accumulator threshold - čím menšie, tým viac falošných kruhov)
    // minRadius = 10, maxRadius = 400 (rozsah veľkosti lopty)
    cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, 1, gray.rows / 8, 100, 20, 25, 500);

    for (size_t i = 0; i < circles.size(); i++)
    {
        cv::Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
        int radius = cvRound(circles[i][2]);

        // Vytvoríme ROI (Region of Interest) okolo kruhu
        // Musíme dávať pozor, aby sme nevyšli z obrazu (Boundary check)
        int x = std::max(0, center.x - radius);
        int y = std::max(0, center.y - radius);
        int w = std::min(frame.cols - x, 2 * radius);
        int h = std::min(frame.rows - y, 2 * radius);

        if (w <= 0 || h <= 0) continue;

        cv::Rect roiRect(x, y, w, h);

        cv::Mat roiMask = redMask(roiRect);

        //pocet pixelov v Roi
        int redPixelCount = cv::countNonZero(roiMask);

        int totalPixels = w * h;

        if (totalPixels > 0 && (double)redPixelCount / totalPixels > 0.4)
        {
            // Našli sme červený kruh!
            qDebug()<<"LOPTAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
            return true;
        }
    }

    return false;
}

void MainWindow::navigationLoop()
{
    static double current_linear_speed = 0.0;
    double angular_speed = 0;

    // --- 0. NOTAUS ---
    if (notaus == true) {
        _robot.setSpeedVal(0, 0);
        current_linear_speed = 0.0;
        return;
    }

    if(state == IDLE) {
        navTimer->stop();
        _robot.setSpeedVal(0, 0);
        current_linear_speed = 0.0;
        return;
    }

    // --- 1. DYNAMICKÁ AKTUALIZÁCIA ---
    int allowedPoints = 0;
    if (lidarVis) {
        allowedPoints = lidarVis->getLastCheckedCount();
        int crashIndex = lidarVis->getFirstCollisionIndex();
        if (crashIndex != -2) {
            int safeLimit = crashIndex + 1;
            if (safeLimit < allowedPoints) allowedPoints = safeLimit;
        }
        std::vector<MapPoint> freshPoints = lidarVis->getPoints();
        if (freshPoints.size() != navigationPoints.size() || freshPoints.size() > 0) {
            navigationPoints = freshPoints;
        }
    }

    // --- 2. KONTROLA KONCA TRASY (Pre Waypointy) ---
    // Táto kontrola funguje hlavne pre modré body, keď cez ne prejdeme.
    if (currentPointIndex >= navigationPoints.size() || currentPointIndex >= allowedPoints) {
        qDebug() << "Koniec trasy (Waypoint).";
        state = IDLE;
        _robot.setSpeedVal(0,0);
        current_linear_speed = 0.0;

        if (recording) {
            stopRecording();
            QMessageBox::information(this, "Misia", "Misia ukončená (Waypoint). Záznam uložený.");
        }
        return;
    }

    MapPoint pt = navigationPoints[currentPointIndex];

    double targetX = pt.x * 100.0;
    double targetY = pt.y * 100.0;
    double dy = robot_X - targetX;
    double dx = robot_Y - targetY;
    double distance = sqrt(dx*dx + dy*dy);
    double targetAngle = atan2(dy, dx);
    double errorAngle = targetAngle - robot_Fi;

    while (errorAngle > M_PI) errorAngle -= 2 * M_PI;
    while (errorAngle < -M_PI) errorAngle += 2 * M_PI;

    const double tolerance_pos = 50.0;
    const double Kp_angle = 1.8;
    const double Kp_dist = 1.0;
    const double RAMP_STEP = 10.0;

    // A) SME V CIELI?
    if (distance < tolerance_pos && state != ROTATING)
    {
        if (pt.type == POINT_PURPLE) {
            _robot.setSpeedVal(0, 0);
            current_linear_speed = 0.0;
            state = ROTATING;
            totalRotatedAngle = 0.0;
            lastRobotTheta = robot_Fi;
            qDebug() << "Bod dosiahnuty (TASK). Zacinam rotovat.";
            return;
        }
        else {
            currentPointIndex++;
            if(lidarVis) lidarVis->setCurrentIndex(currentPointIndex);

            // Ak bol toto posledný bod (Waypoint), v ďalšom cykle to zachytí kontrola na začiatku
            if (currentPointIndex >= navigationPoints.size()) {
                // Tu ešte nezastavujeme, necháme prebehnúť ďalší cyklus, ktorý to korektne ukončí
            }
            return;
        }
    }

    // B) MOVING
    if (state == MOVING)
    {
        double required_linear = 0.0;
        if (fabs(errorAngle) > M_PI / 18.0) {
            required_linear = 0;
            angular_speed = Kp_angle * errorAngle;
        } else {
            required_linear = Kp_dist * distance * cos(errorAngle);
            angular_speed = Kp_angle * errorAngle;
        }

        if (required_linear > 300) required_linear = 300;
        if (required_linear < 0) required_linear = 0;

        if (current_linear_speed < required_linear) {
            current_linear_speed += RAMP_STEP;
            if (current_linear_speed > required_linear) current_linear_speed = required_linear;
        } else if (current_linear_speed > required_linear) {
            current_linear_speed -= RAMP_STEP;
            if (current_linear_speed < required_linear) current_linear_speed = required_linear;
        }

        if (current_linear_speed < 0) current_linear_speed = 0;
        if (angular_speed > 3.1415/4) angular_speed = 3.1415/4;
        if (angular_speed < -3.1415/4) angular_speed = -3.1415/4;

        _robot.setSpeedVal(current_linear_speed, angular_speed);
    }

    // C) ROTATING (Task)
    else if (state == ROTATING)
    {
        current_linear_speed = 0.0;
        double delta = robot_Fi - lastRobotTheta;
        while (delta > M_PI) delta -= 2 * M_PI;
        while (delta < -M_PI) delta += 2 * M_PI;

        totalRotatedAngle += fabs(delta);
        lastRobotTheta = robot_Fi;

        if (totalRotatedAngle >= 2 * M_PI - 0.2) {

            // Rotácia hotová
            state = MOVING;
            currentPointIndex++;
            if(lidarVis) lidarVis->setCurrentIndex(currentPointIndex);

            _robot.setSpeedVal(0, 0);
            qDebug() << "Rotacia dokoncena.";

            // --- !!! TOTO JE OPRAVA PRE POSLEDNÝ TASK BOD !!! ---
            // Skontrolujeme, či sme po rotácii už na konci zoznamu
            if (currentPointIndex >= navigationPoints.size() || currentPointIndex >= allowedPoints) {
                qDebug() << "Koniec trasy (Task).";
                state = IDLE;
                _robot.setSpeedVal(0, 0);

                if (recording) {
                    stopRecording();
                    QMessageBox::information(this, "Misia", "Misia ukončená (Task). Záznam uložený.");
                }
            }
            // ----------------------------------------------------

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

void MainWindow::startRecording()
{
    if (recording) return;

    // 1. Vytvorenie priečinka a cesty (tvoj kód ostáva)
    QDateTime now = QDateTime::currentDateTime();
    QString folderName = "Misia_" + now.toString("yyyy_MM_dd_hh_mm_ss");
    QString basePath = "C:/Users/petri/Downloads/kamera_kobuki/Zaznamy/"; // Uprav si cestu ak treba
    QString fullPath = basePath + folderName;

    QDir dir;
    if (!dir.exists(fullPath)) {
        dir.mkpath(fullPath);
    }

    QString camFile = fullPath + "/kamera.avi";
    QString lidarFile = fullPath + "/lidar.avi";

    int fps_1 = 8;
    int fps_2 = 21;

    // --- OTVORENIE KAMERY ---
    videoWriterCamera.open(camFile.toStdString(), cv::VideoWriter::fourcc('M','J','P','G'), fps_1, cv::Size(640, 360), true);

    // --- OTVORENIE LIDARU (OPRAVENÉ) ---
    if(lidarVis) {
        // Zistíme aktuálnu veľkosť widgetu
        int w = lidarVis->width();
        int h = lidarVis->height();

        // Ochrana pred nulovými rozmermi
        if (w <= 0) w = 640;
        if (h <= 0) h = 480;

        // !!! DÔLEŽITÉ: Rozmery musia byť párne (násobky 2) !!!
        if (w % 2 != 0) w--;
        if (h % 2 != 0) h--;

        // Uložíme si tento rozmer do premennej v triede
        lidarVideoSize = cv::Size(w, h);

        videoWriterLidar.open(lidarFile.toStdString(), cv::VideoWriter::fourcc('M','J','P','G'), fps_2, lidarVideoSize, true);
    }

    if (videoWriterCamera.isOpened() && videoWriterLidar.isOpened()) {
        recording = true;
        //koniecMisie = false; // ak použivaš tuto premennu
        lidarRecordTimer->start(1000 / fps_2);
        qDebug() << "Nahravanie spustene. Lidar rozmer:" << lidarVideoSize.width << "x" << lidarVideoSize.height;
    } else {
        qDebug() << "CHYBA: Nepodarilo sa otvorit video subory!";
        // Pre istotu skúsime zavrieť, ak sa jeden otvoril a druhý nie
        if (videoWriterCamera.isOpened()) videoWriterCamera.release();
        if (videoWriterLidar.isOpened()) videoWriterLidar.release();
    }
}

void MainWindow::stopRecording()
{
    if (!recording) return;

    recording = false;
    koniecMisie = true; // Splnenie požiadavky

    lidarRecordTimer->stop();

    if (videoWriterCamera.isOpened()) videoWriterCamera.release();
    if (videoWriterLidar.isOpened()) videoWriterLidar.release();

    qDebug() << "Nahravanie ukoncene. Koniec misie.";
}

// Funkcia, ktorá "odfotí" Lidar okno a uloží do videa
void MainWindow::recordLidarFrame()
{
    // 1. Základná kontrola
    if (!recording || !lidarVis || !videoWriterLidar.isOpened()) return;

    // 2. Získame obrázok widgetu
    QPixmap pix = lidarVis->grab();
    if (pix.isNull() || pix.width() <= 0 || pix.height() <= 0) {
        return;
    }

    QImage img = pix.toImage().convertToFormat(QImage::Format_RGB888);

    // 3. Konverzia QImage na cv::Mat
    cv::Mat mat(img.height(), img.width(), CV_8UC3, (uchar*)img.bits(), img.bytesPerLine());

    // Vytvoríme čistú kópiu pre OpenCV (BGR)
    cv::Mat matBGR;
    cv::cvtColor(mat, matBGR, cv::COLOR_RGB2BGR);

    // 4. !!! KRITICKÁ ČASŤ !!!
    // Musíme zmeniť veľkosť obrázka presne na to, s čím sme otvorili VideoWriter.
    // Aj keď sa veľkosť líši len o 1 pixel, musíme spraviť resize.

    if (matBGR.size() != lidarVideoSize) {
        try {
            cv::resize(matBGR, matBGR, lidarVideoSize);
        } catch (...) {
            qDebug() << "Chyba pri resize Lidaru!";
            return;
        }
    }

    // 5. Zápis
    videoWriterLidar.write(matBGR);

    // Debug výpis (môžeš po čase vymazať, ak to bude fungovať)
    // static int frameCounter = 0;
    // if (frameCounter++ % 20 == 0) qDebug() << "Zapisujem Lidar frame...";
}
