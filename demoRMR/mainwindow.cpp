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
#include "replaydialog.h"

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

    connect(lidarRecordTimer, &QTimer::timeout, this, &MainWindow::recordStatsFrame);

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

    batteryVis = new BatteryIndicator(this);

    ui->widget_2->layout()->addWidget(batteryVis);

    if (ui->widget_3->layout() == nullptr) {
        QVBoxLayout *layout = new QVBoxLayout(ui->widget_3);
        ui->widget_3->setLayout(layout);
    }

    QFont font("Arial", 10, QFont::Bold);

    labelX = new QLabel("X: 0.00 m", this);
    labelY = new QLabel("Y: 0.00 m", this);
    labelFi = new QLabel("Fi: 0.00 rad", this);

    labelX->setFont(font);
    labelY->setFont(font);
    labelFi->setFont(font);

    labelX->setAlignment(Qt::AlignCenter);
    labelY->setAlignment(Qt::AlignCenter);
    labelFi->setAlignment(Qt::AlignCenter);

    ui->widget_3->layout()->addWidget(labelX);
    ui->widget_3->layout()->addWidget(labelY);
    ui->widget_3->layout()->addWidget(labelFi);

    if(ui->lidar && !ui->lidar->layout()) {
        QVBoxLayout* l = new QVBoxLayout(ui->lidar);
        l->setContentsMargins(0,0,0,0);
    }
    if(ui->camera && !ui->camera->layout()) {
        QVBoxLayout* l = new QVBoxLayout(ui->camera);
        l->setContentsMargins(0,0,0,0);
    }

    if(ui->smallLidar && !ui->smallLidar->layout()) {
        QVBoxLayout* l = new QVBoxLayout(ui->smallLidar);
        l->setContentsMargins(0,0,0,0);
    }
    if(ui->smallCamera && !ui->smallCamera->layout()) {
        QVBoxLayout* l = new QVBoxLayout(ui->smallCamera);
        l->setContentsMargins(0,0,0,0);
    }

    // --- 3. Nastavenie pociatocneho stavu ---
    isLidarBig = true;

    ui->lidar->layout()->addWidget(lidarVis);

    ui->smallCamera->layout()->addWidget(cameraLabel);

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

    connect(ui->tableWidgetPoints, &QTableWidget::cellClicked,
            this, &MainWindow::onCellClicked);

    connect(ui->tableWidgetPoints->verticalHeader(), &QHeaderView::sectionClicked,
            this, &MainWindow::onRowHeaderClicked);

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
    if(!lidarVis || !cameraLabel) return;

    lidarVis->setParent(nullptr);
    cameraLabel->setParent(nullptr);

    isLidarBig = !isLidarBig;

    if(isLidarBig)
    {

        ui->lidar->layout()->addWidget(lidarVis);
        ui->smallCamera->layout()->addWidget(cameraLabel);
        ui->stackedWidget->setCurrentWidget(ui->lidar);
        if(ui->stackedWidget_2) ui->stackedWidget_2->setCurrentWidget(ui->smallCamera);
        
        ui->pushButton->setIcon(QIcon(":/ikonky/laser.png"));
    }
    else
    {
        ui->camera->layout()->addWidget(cameraLabel);
        ui->smallLidar->layout()->addWidget(lidarVis);
        ui->stackedWidget->setCurrentWidget(ui->camera);
        if(ui->stackedWidget_2) ui->stackedWidget_2->setCurrentWidget(ui->smallLidar);

        ui->pushButton->setIcon(QIcon(":/ikonky/camera.png"));
    }

    // Prekreslenie
    lidarVis->show();
    cameraLabel->show();
}

void MainWindow::updatePointsTable(const std::vector<MapPoint> &points)
{
    ui->tableWidgetPoints->setRowCount(0);
    for(int i = 0; i < (int)points.size(); ++i) {
        const auto& p = points[i];
        int row = ui->tableWidgetPoints->rowCount();
        ui->tableWidgetPoints->insertRow(row);
        ui->tableWidgetPoints->setItem(row, 0, new QTableWidgetItem(QString::number(p.x)));
        ui->tableWidgetPoints->setItem(row, 1, new QTableWidgetItem(QString::number(p.y)));
        QString typeStr = (p.type == POINT_BLUE) ? "Waypoint" : "Task";
        QTableWidgetItem *itemType = new QTableWidgetItem(typeStr);

        if (state != IDLE && i < currentPointIndex) {
            itemType->setForeground(Qt::gray); // Už sme tam boli -> Zelená
            itemType->setFont(QFont("Arial", 9, QFont::Bold)); // Voliteľné: Tučné písmo
        }
        else {
            // Ešte sme tam neboli -> Pôvodné farby
            if(p.type == POINT_BLUE) itemType->setForeground(Qt::blue);
            else itemType->setForeground(Qt::magenta);
        }

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
    navigationPoints = lidarVis->getPoints();

    if(navigationPoints.empty()) {
        QMessageBox::warning(this, "Pozor", "Žiadne body!");
        return;
    }

    // --- KONTROLA: Sú body prepojené? ---
    if (!lidarVis->isPathDrawActive()) {
        QMessageBox::warning(this, "Pozor", "Trasa nie je skontrolovaná!\nStlačte najprv tlačidlo 'Kontrola'");
        return;
    }

    if (lidarVis->getLastCheckedCount() == 0) {
        QMessageBox::warning(this, "Pozor", "Žiadne body!");
        return;
    }
    // -----------------------------------------

    currentPointIndex = 0;
    if(lidarVis) {
        lidarVis->setCurrentIndex(0);
    }

    updatePointsTable(navigationPoints);

    state = MOVING;

    startRecording(); //nahravie spustene

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


void MainWindow::setUiValues(double robotX,double robotY,double robotFi) {}

#ifndef DISABLE_AMCL
/*void MainWindow::setUiAMCLValues(double robotX, double robotY, double robotFi)
{
    //ui->lineEdit_2->setText("X = " + QString::number(robotX/100));
    //ui->lineEdit_3->setText("Y = " + QString::number(robotY/100));
    double normalizedFi = std::fmod(robotFi, 2.0 * M_PI);
    if (normalizedFi > M_PI) normalizedFi -= 2.0 * M_PI;
    if (normalizedFi <= -M_PI) normalizedFi += 2.0 * M_PI;
    //ui->lineEdit_4->setText("Fi = " + QString::number(normalizedFi));

    labelX->setText(QString("X: %1 mm").arg(robotX, 0, 'f', 0));
    labelY->setText(QString("Y: %1 mm").arg(robotY, 0, 'f', 0));
    labelFi->setText(QString("Fi: %1 rad").arg(normalizedFi, 0, 'f', 3));

    robot_X = robotX;
    robot_Y = robotY;
    robot_Fi = normalizedFi;
    //qDebug()<<"uhol robota: "<<robot_Fi;

}*/
void MainWindow::setUiAMCLValues(double robotX, double robotY, double robotFi)
{
    static std::vector<std::pair<double, double>> history;

    static int badPointsCounter = 0;       // Koľko zlých bodov prišlo za sebou
    static int stablePointsCounter = 0;    // Koľko dobrých bodov prišlo za sebou
    static bool warningActive = false;     // Či sme už zobrazili varovanie a čakáme na ustálenie

    const size_t HISTORY_SIZE = 4;
    const double MAX_JUMP_MM = 500.0;
    const int STABILITY_REQUIRED = 20;     // Musí prísť 20 dobrých bodov, aby sa resetovalo varovanie

    if (!history.empty()) {
        double sumX = 0, sumY = 0;
        for (const auto& bod : history) { sumX += bod.first; sumY += bod.second; }
        double avgX = sumX / history.size();
        double avgY = sumY / history.size();

        double dist = std::sqrt(std::pow(robotX - avgX, 2) + std::pow(robotY - avgY, 2));

        if (dist > MAX_JUMP_MM) {
            // --- DETEGOVANÁ CHYBA ---
            badPointsCounter++;
            stablePointsCounter = 0; // Prerušili sme sériu dobrých bodov

            if (!warningActive) {
                warningActive = true; // Zamkneme, aby nevyskakovalo ďalšie
                QMessageBox::warning(this, "Pozor", "Strata polohy!");
            }

            // Ak je to krátkodobý úlet, ignorujeme ho
            if (badPointsCounter < 5) {
                return; // Ukončíme funkciu, neprekreslíme UI
            }

            // Ak úlet trvá dlho, resetujeme filter (robot sa asi naozaj premiestnil)
            history.clear();
            badPointsCounter = 0;
        }
        else {
            // --- BOD JE V PORIADKU ---
            badPointsCounter = 0;
            stablePointsCounter++;

            // Varovanie odomkneme až vtedy, keď je signál dlhšie stabilný
            if (stablePointsCounter > STABILITY_REQUIRED) {
                warningActive = false;
            }
        }
    }

    history.push_back({robotX, robotY});
    if (history.size() > HISTORY_SIZE) history.erase(history.begin());
    // -------------------------------------------------------

    // --- 2. PÔVODNÝ KÓD NA VYKRESLENIE ---
    double normalizedFi = std::fmod(robotFi, 2.0 * M_PI);
    if (normalizedFi > M_PI) normalizedFi -= 2.0 * M_PI;
    if (normalizedFi <= -M_PI) normalizedFi += 2.0 * M_PI;

    labelX->setText(QString("X: %1 mm").arg(robotX, 0, 'f', 0));
    labelY->setText(QString("Y: %1 mm").arg(robotY, 0, 'f', 0));
    labelFi->setText(QString("Fi: %1 rad").arg(normalizedFi, 0, 'f', 3));

    robot_X = robotX;
    robot_Y = robotY;
    robot_Fi = normalizedFi;
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

    connect(&_robot, SIGNAL(publishFrontLidarPoints(std::vector<double>, std::vector<double>)), this, SLOT(receiveFrontLidarPoints(std::vector<double>, std::vector<double>)));

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
/*int MainWindow::paintThisCamera(const cv::Mat &cameraData)
{
    // 1. OCHRANA
    if (cameraData.empty() || cameraData.cols <= 0 || cameraData.rows <= 0) return 0;

    // 2. NORMALIZÁCIA OBRAZU
    cv::Mat frameCopy;
    if (cameraData.channels() == 4) cv::cvtColor(cameraData, frameCopy, cv::COLOR_BGRA2BGR);
    else if (cameraData.channels() == 1) cv::cvtColor(cameraData, frameCopy, cv::COLOR_GRAY2BGR);
    else cameraData.copyTo(frameCopy);

    frameCopy.copyTo(frame[(actIndex+1)%3]);
    actIndex = (actIndex+1)%3;

    // 3. RESIZE
    cv::resize(frameCopy, frameCopy, cv::Size(640, 360));

    // 4. PRÍPRAVA NA KRESLENIE
    cv::Mat rgbFrame;
    cv::cvtColor(frameCopy, rgbFrame, cv::COLOR_BGR2RGB);

    QImage qimg((uchar*)rgbFrame.data, rgbFrame.cols, rgbFrame.rows, rgbFrame.step, QImage::Format_RGB888);
    QImage drawingImage = qimg.copy();
    QPainter painter(&drawingImage);

    // --- 5. DETEKCIA LOPTY ---
    float ballRadius = 0;
    cv::Point ballCenter;
    bool ballFound = false;

    if (!photoTaken) {
        ballFound = detectBall(frameCopy, ballRadius, ballCenter);
    }

    // Premenné na nájdenie najlepšieho bodu
    double bestLidarDistance = -1.0;
    double minDiffX = 10000.0; // Inicializujeme na veľké číslo

    // --- 6. FÚZIA LIDARU A KAMERY ---
    double width_I = 640.0;
    double height_I = 360.0;
    double f = 934.962;
    double Z = -210;
    double Z_D = -145;
    double Y_D = -115;
    double f_new = f * (width_I / 960.0);

    //double bestLidarDistance = -1.0;
    double minDistanceFound = 100000.0; // Inicializujeme na velke cislo

    for (size_t var = 0; var < uhol_update.size(); var++) {
        double dist = vzdialenost_update[var];
        if(dist <= 0) continue;

        double uhol_rad = uhol_update[var] * (M_PI/180.0);
        double sinus = std::sin(uhol_rad);
        double cosinus = std::cos(uhol_rad);

        // Projekcia bodu na obrazovku
        double X_obr = width_I / 2.0 - (f_new * (dist * sinus)) / (dist * cosinus + Z_D);
        double Y_obr = height_I / 2.0 + (f_new * (-Z + Y_D)) / (dist * cosinus + Z_D);

        if(X_obr >= 0 && X_obr < width_I && Y_obr >= 0 && Y_obr < height_I) {

            // Základné vykreslenie bodu (červená/modrá)
            QColor color;
            int kanalAlfa = static_cast<int>((250.0 / dist) * 255);
            if (kanalAlfa > 255) kanalAlfa = 255; if (kanalAlfa < 50) kanalAlfa = 50;

            if (dist > 185 && dist <= 350) {
                painter.setBrush(QColor(255, 0, 0, 255)); painter.setPen(Qt::NoPen);
                painter.drawRect(QRectF(X_obr - 5, Y_obr - 5, 10, 10));
            } else if (dist > 350) {
                painter.setBrush(QColor(0, 0, 255, kanalAlfa)); painter.setPen(Qt::NoPen);
                painter.drawEllipse(QPointF(X_obr, Y_obr), 3, 3);
            } else {
                painter.setBrush(QColor(0, 0, 0, 255)); painter.setPen(Qt::NoPen);
                painter.drawEllipse(QPointF(X_obr, Y_obr), 3, 3);
            }

            // --- HĽADANIE NAJBLIŽŠIEHO BODU K LOPTE (Iba X os) ---
            if (ballFound) {
                // Sme v rámci šírky lopty?
                if (std::abs(X_obr - ballCenter.x) < ballRadius) {

                    painter.setBrush(Qt::green);
                    painter.drawEllipse(QPointF(X_obr, Y_obr), 6, 6);

                    // Ak je tento bod BLIŽŠIE než tie, čo sme našli doteraz, berieme ho!
                    // Tým pádom ignorujeme stenu (3500mm) a vezmeme loptu (napr. 500mm)
                    if (dist < minDistanceFound) {
                        minDistanceFound = dist;
                        bestLidarDistance = dist;
                    }
                }
            }
        }
    }
    painter.end();

    // 7. ZOBRAZENIE
    if(cameraLabel) {
        QPixmap pix = QPixmap::fromImage(drawingImage);
        cameraLabel->setPixmap(pix.scaled(cameraLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    // 8. NAHRÁVANIE
    if (recording && videoWriterCamera.isOpened()) {
        videoWriterCamera.write(frameCopy);
    }

    // 9. LOGIKA UKONČENIA
    if (ballFound) {

        qDebug() << "Lopta detegovaná. Radius:" << ballRadius;

        // --- Získanie vzdialenosti ---
        double distanceMm = bestLidarDistance;

        // Ak lidar loptu netrafil (distance je -1 alebo 0), musíme to ošetriť,
        // inak by podmienka < 1000 prešla (lebo -1 < 1000).
        if (distanceMm <= 0) {
            // Fallback na kameru ak lidar zlyhal, alebo ignorovanie
            // distanceMm = 40000.0 / ballRadius;
            // Alebo len nastavíme veľké číslo, aby sa to nespustilo
            distanceMm = 9999.0;
        }

        if (distanceMm > 5000) distanceMm = 5000;

        // --- Výpočet uhla ---
        double fovRad = 1.05;
        double angleOffset = ((320.0 - ballCenter.x) / 320.0) * (fovRad / 2.0);

        // --- Výpočet polohy na mape ---
        double ballGlobalAngle = robot_Fi + angleOffset;

        // Normalizácia výsledného uhla (-PI do +PI)
        if (ballGlobalAngle > M_PI) ballGlobalAngle -= 2.0 * M_PI;
        if (ballGlobalAngle <= -M_PI) ballGlobalAngle += 2.0 * M_PI;

        double ballX = robot_X - distanceMm * sin(ballGlobalAngle);
        double ballY = robot_Y - distanceMm * cos(ballGlobalAngle);

        // --- Zobrazenie na mape (Vizualizácia) ---
        // Toto necháme bežať vždy, aby si videl loptu na mape aj z diaľky
        if (lidarVis) {
            lidarVis->setDetectedBall(true, (int)(ballX / 100.0), (int)(ballY / 100.0));
        }

        // --- DEBUG VÝPIS ---
        qDebug() << "Lopta dist:" << distanceMm << "mm | Uhol:" << angleOffset;

        // ====================================================================
        // >>> TU JE ZMENA: KONTROLA VZDIALENOSTI (MENEJ AKO 1 METER) <<<
        // ====================================================================
        // Vykoná sa len ak je vzdialenosť platná (> 10mm) a menšia ako 1000mm
        if (distanceMm > 10.0 && distanceMm < 1000.0)
        {
            qDebug() << "Lopta je blizko (< 1m)! Zastavujem a fotim.";

            // --- Uloženie a Koniec ---
            QString saveDir = "C:/Users/petri/Downloads/kamera_kobuki/Fotka/";
            QDir dir(saveDir); if (!dir.exists()) dir.mkpath(".");
            QDateTime now = QDateTime::currentDateTime();
            QString timestamp = now.toString("yyyy_MM_dd_hh_mm_ss");
            QString finalPhotoPath = saveDir + "fotka_lopty_" + timestamp + ".jpg";

            cv::imwrite(finalPhotoPath.toStdString(), frameCopy);

            photoTaken = true;
            if (recording) stopRecording();
            state = IDLE;

            // Okamžité zastavenie
            _robot.setSpeedVal(0, 0);

            QMessageBox::information(this, "Misia Úspešná",
                                     "Lopta nájdená a dosiahnutá!\n"
                                     "Vzdialenosť: " + QString::number((int)distanceMm) + " mm\n"
                                                                               "Súradnice: [" + QString::number((int)ballX) + ", " + QString::number((int)ballY) + "]");
        }
        else {
            // Ak je lopta ďalej ako 1m, len vypíšeme info, ale nezastavujeme
            // Robot pokračuje v navigácii (state ostáva MOVING alebo ROTATING)
            qDebug() << "Vidim loptu, ale je este daleko (" << distanceMm << " mm). Pokracujem.";
        }
    }

    return 0;
}*/
int MainWindow::paintThisCamera(const cv::Mat &cameraData)
{
    // 1. OCHRANA
    if (cameraData.empty() || cameraData.cols <= 0 || cameraData.rows <= 0) return 0;

    // 2. NORMALIZÁCIA OBRAZU
    cv::Mat frameCopy;
    if (cameraData.channels() == 4) cv::cvtColor(cameraData, frameCopy, cv::COLOR_BGRA2BGR);
    else if (cameraData.channels() == 1) cv::cvtColor(cameraData, frameCopy, cv::COLOR_GRAY2BGR);
    else cameraData.copyTo(frameCopy);

    frameCopy.copyTo(frame[(actIndex+1)%3]);
    actIndex = (actIndex+1)%3;

    // 3. RESIZE
    cv::resize(frameCopy, frameCopy, cv::Size(640, 360));

    // 4. PRÍPRAVA NA KRESLENIE
    cv::Mat rgbFrame;
    cv::cvtColor(frameCopy, rgbFrame, cv::COLOR_BGR2RGB);

    bool vidimPrekazkuTeraz = false;

    QImage qimg((uchar*)rgbFrame.data, rgbFrame.cols, rgbFrame.rows, rgbFrame.step, QImage::Format_RGB888);
    QImage drawingImage = qimg.copy();
    QPainter painter(&drawingImage);

    // --- 5. DETEKCIA LOPTY ---
    float ballRadius = 0;
    cv::Point ballCenter;
    bool ballFound = false;

    // Detekujeme len ak sme ešte neodfotili
    if (!photoTaken) {
        ballFound = detectBall(frameCopy, ballRadius, ballCenter);
    }

    // Premenné na nájdenie najlepšieho bodu
    double bestLidarDistance = -1.0;
    double minDistanceFound = 100000.0;

    // --- 6. FÚZIA LIDARU A KAMERY ---
    double width_I = 640.0;
    double height_I = 360.0;
    double f = 934.962;
    double Z = -210;
    double Z_D = -145;
    double Y_D = -115;
    double f_new = f * (width_I / 960.0);

    for (size_t var = 0; var < uhol_update.size(); var++) {
        double dist = vzdialenost_update[var];
        if(dist <= 0) continue;

        double uhol_rad = uhol_update[var] * (M_PI/180.0);
        double sinus = std::sin(uhol_rad);
        double cosinus = std::cos(uhol_rad);

        double X_obr = width_I / 2.0 - (f_new * (dist * sinus)) / (dist * cosinus + Z_D);
        double Y_obr = height_I / 2.0 + (f_new * (-Z + Y_D)) / (dist * cosinus + Z_D);

        if(X_obr >= 0 && X_obr < width_I && Y_obr >= 0 && Y_obr < height_I) {

            // Základné vykreslenie bodu
            int kanalAlfa = static_cast<int>((250.0 / dist) * 255);
            if (kanalAlfa > 255) kanalAlfa = 255; if (kanalAlfa < 50) kanalAlfa = 50;

            if (dist >= 270 && dist <= 350) {
                painter.setBrush(QColor(255, 0, 0, 255)); painter.setPen(Qt::NoPen);
                painter.drawRect(QRectF(X_obr - 5, Y_obr - 5, 10, 10));
            }else if (dist < 270 && dist > 50){
                    vidimPrekazkuTeraz = true;
                    qDebug()<<"prekazka do paze "<< vidimPrekazkuTeraz;
            } else if (dist > 350) {
                painter.setBrush(QColor(0, 0, 255, kanalAlfa)); painter.setPen(Qt::NoPen);
                painter.drawEllipse(QPointF(X_obr, Y_obr), 3, 3);
            } else {
                painter.setBrush(QColor(0, 0, 0, 255)); painter.setPen(Qt::NoPen);
                painter.drawEllipse(QPointF(X_obr, Y_obr), 3, 3);
            }

            // Hľadanie najbližšieho bodu k lopte
            if (ballFound) {
                if (std::abs(X_obr - ballCenter.x) < ballRadius) {
                    painter.setBrush(Qt::green);
                    painter.drawEllipse(QPointF(X_obr, Y_obr), 6, 6);

                    if (dist < minDistanceFound) {
                        minDistanceFound = dist;
                        bestLidarDistance = dist;
                    }
                }
            }
        }
    }
    painter.end();

    // 7. ZOBRAZENIE
    if(cameraLabel) {
        QPixmap pix = QPixmap::fromImage(drawingImage);
        cameraLabel->setPixmap(pix.scaled(cameraLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    // 8. NAHRÁVANIE VIDEO
    if (recording && videoWriterCamera.isOpened()) {
        videoWriterCamera.write(frameCopy);
    }

    // 9. LOGIKA UKONČENIA
    if (ballFound) {

        qDebug() << "Lopta detegovaná. Radius:" << ballRadius;

        double distanceMm = bestLidarDistance;
        if (distanceMm <= 0) distanceMm = 9999.0;
        if (distanceMm > 5000) distanceMm = 5000;

        // Výpočet polohy lopty pre zobrazenie
        double fovRad = 1.05;
        double angleOffset = ((320.0 - ballCenter.x) / 320.0) * (fovRad / 2.0);
        double ballGlobalAngle = robot_Fi + angleOffset;
        if (ballGlobalAngle > M_PI) ballGlobalAngle -= 2.0 * M_PI;
        if (ballGlobalAngle <= -M_PI) ballGlobalAngle += 2.0 * M_PI;
        double ballX = robot_X - distanceMm * sin(ballGlobalAngle);
        double ballY = robot_Y - distanceMm * cos(ballGlobalAngle);

        if (lidarVis) {
            lidarVis->setDetectedBall(true, (int)(ballX / 100.0), (int)(ballY / 100.0));
        }

        // ====================================================================
        // >>> LOGIKA ZASTAVENIA A ONESKORENIA <<<
        // ====================================================================
        if (distanceMm > 10.0 && distanceMm < 1000.0)
        {
            qDebug() << "Lopta je blizko! Zastavujem, fotim a spustam casovac pre koniec.";

            // 1. OKAMŽITE ZASTAVIŤ ROBOTA
            state = IDLE; // Vypneme navigáciu
            _robot.setSpeedVal(0, 0); // Zastavíme motory

            // 2. OKAMŽITE ODFOTIŤ
            QString saveDir = "C:/Users/petri/Downloads/kamera_kobuki/Fotka/";
            QDir dir(saveDir); if (!dir.exists()) dir.mkpath(".");
            QDateTime now = QDateTime::currentDateTime();
            QString timestamp = now.toString("yyyy_MM_dd_hh_mm_ss");
            QString finalPhotoPath = saveDir + "fotka_lopty_" + timestamp + ".jpg";

            cv::imwrite(finalPhotoPath.toStdString(), frameCopy);

            photoTaken = true;

            QTimer::singleShot(2000, this, [this, distanceMm, ballX, ballY]() {
                if (recording) stopRecording();
                
                QMessageBox::information(this, "Misia Úspešná",
                                         "Lopta nájdená a dosiahnutá!\n"
                                         "Vzdialenosť: " + QString::number((int)distanceMm) + " mm\n"
                                                                                   "Súradnice: [" + QString::number((int)ballX) + ", " + QString::number((int)ballY) + "]");
            });
        }
        else {
            qDebug() << "Vidim loptu, ale je este daleko (" << distanceMm << " mm). Pokracujem.";
        }
    }
    prekazkaActive = vidimPrekazkuTeraz;
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

/*bool MainWindow::detectBall(const cv::Mat &frame, float &outRadius, cv::Point &outCenter)
{
    // 1. Ochrana
    if (frame.empty()) return false;

    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
    cv::Mat lowerRed1, upperRed1, lowerRed2, upperRed2, redMask;

    cv::inRange(hsv, cv::Scalar(0, 150, 80), cv::Scalar(10, 255, 255), lowerRed1);
    cv::inRange(hsv, cv::Scalar(170, 150, 80), cv::Scalar(180, 255, 255), lowerRed2);
    redMask = lowerRed1 | lowerRed2;

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(5, 5), 2, 2);

    std::vector<cv::Vec3f> circles;
    // Parametre: minRadius 5, maxRadius 400
    cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, 1, gray.rows / 8, 100, 25, 5, 400);

    for (size_t i = 0; i < circles.size(); i++)
    {
        cv::Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
        int radius = cvRound(circles[i][2]);

        int x = std::max(0, center.x - radius);
        int y = std::max(0, center.y - radius);
        int w = std::min(frame.cols - x, 2 * radius);
        int h = std::min(frame.rows - y, 2 * radius);

        if (w <= 0 || h <= 0) continue;

        cv::Rect roiRect(x, y, w, h);
        cv::Mat roiMask = redMask(roiRect);

        int redPixelCount = cv::countNonZero(roiMask);
        int totalPixels = w * h;

        if (totalPixels > 0 && (double)redPixelCount / totalPixels > 0.4)
        {
            // !!! Zapíšeme vysledky do premenných !!!
            outRadius = (float)radius;
            outCenter = center;
            return true;
        }
    }
    return false;
}*/
/*bool MainWindow::detectBall(const cv::Mat &frame, float &outRadius, cv::Point &outCenter)
{
    if (frame.empty()) return false;

    // 1. Prevod na HSV
    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    // 2. Maska pre červenú farbu
    cv::Mat lowerRed1, upperRed1, lowerRed2, upperRed2, redMask;
    cv::inRange(hsv, cv::Scalar(0, 130, 80), cv::Scalar(10, 255, 255), lowerRed1);
    cv::inRange(hsv, cv::Scalar(170, 130, 80), cv::Scalar(180, 255, 255), lowerRed2);
    redMask = lowerRed1 | lowerRed2;

    // 3. Odstránenie šumu (morfologické operácie)
    // Toto spojí rozbité červené fľaky do jedného
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::erode(redMask, redMask, kernel);
    cv::dilate(redMask, redMask, kernel);

    // 4. Nájdenie kontúr (obrysov červených fľakov)
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(redMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    double maxArea = 0;
    int maxIndex = -1;

    // 5. Hľadáme najväčší červený objekt
    for (size_t i = 0; i < contours.size(); i++) {
        double area = cv::contourArea(contours[i]);

        // Ignorujeme malé šumy (filtrovanie podľa veľkosti)
        if (area > 500) {
            if (area > maxArea) {
                maxArea = area;
                maxIndex = (int)i;
            }
        }
    }

    // 6. Ak sme našli veľký objekt
    if (maxIndex != -1) {
        // Vypočítame kruh, ktorý tento objekt obaluje
        cv::Point2f center;
        float radius;
        cv::minEnclosingCircle(contours[maxIndex], center, radius);

        // Zapíšeme výsledky
        outCenter = center;
        outRadius = radius;

        // Debug výpis
        qDebug() << "Nasiel som loptu cez KONTURY! Radius:" << radius << " Area:" << maxArea;

        return true;
    }

    return false;
}
*/
/*bool MainWindow::detectBall(const cv::Mat &frame, float &outRadius, cv::Point &outCenter)
{
    if (frame.empty()) return false;

    // 1. Prevod na HSV
    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    // 2. Maska pre červenú farbu
    // Rozsahy ostavaju rovnake
    cv::Mat lowerRed1, upperRed1, lowerRed2, upperRed2, redMask;
    cv::inRange(hsv, cv::Scalar(0, 130, 80), cv::Scalar(10, 255, 255), lowerRed1);
    cv::inRange(hsv, cv::Scalar(170, 130, 80), cv::Scalar(180, 255, 255), lowerRed2);
    redMask = lowerRed1 | lowerRed2;

    // 3. Odstránenie šumu
    // Dôležité: Nepreháňať to s dilatáciou, aby sa z kríža nestala machuľa
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::erode(redMask, redMask, kernel);
    cv::dilate(redMask, redMask, kernel);

    // 4. Nájdenie kontúr
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(redMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    double maxArea = 0;
    int bestIndex = -1;
    float bestRadius = 0;
    cv::Point2f bestCenter;

    // 5. Prechádzame všetky kontúry a hľadáme tú, ktorá je červená A ZÁROVEŇ guľatá
    for (size_t i = 0; i < contours.size(); i++) {
        double area = cv::contourArea(contours[i]);

        // Ignorujeme malé šumy
        if (area < 600) continue;

        // --- NOVÁ KONTROLA TVARU ---

        // A) Vypočítame obvod
        double perimeter = cv::arcLength(contours[i], true);
        if (perimeter == 0) continue;

        // B) Vypočítame "Cirkularitu" (Kruhovitosť)
        // Vzorec: 4 * PI * Area / (Perimeter^2)
        // Perfektný kruh má hodnotu 1.0. Štvorec cca 0.78.
        // Kríž (dlhé tenké čiary) bude mať veľmi nízku hodnotu (napr. 0.2 - 0.4).
        double circularity = (4 * M_PI * area) / (perimeter * perimeter);

        // C) Vypočítame "Plnosť" (Solidity) voči opísanej kružnici
        cv::Point2f center;
        float radius;
        cv::minEnclosingCircle(contours[i], center, radius);
        double circleArea = M_PI * radius * radius;

        // Pomer plochy objektu k ploche kruhu, v ktorom sa nachádza.
        // Lopta vyplní kruh takmer celý (cca 0.8 - 0.9).
        // Kríž vyplní len malú časť kruhu (má veľa prázdneho miesta okolo ramien).
        double solidity = area / circleArea;

        // --- DEBUG VÝPIS (aby si videl hodnoty v konzole) ---
        // Ak ti to nenájde loptu, pozri si v konzole, aké má hodnoty a uprav podmienku nižšie
        // qDebug() << "Objekt" << i << "Area:" << area << "Circularity:" << circularity << "Solidity:" << solidity;

        // D) Podmienka pre LOPTU
        // Cirkularita > 0.6 (aby sme vylúčili čiari a kríže)
        // Solidity > 0.6 (aby sme vylúčili prstence alebo C-tvary)
        qDebug()<<"CIRC: "<< circularity << "Solid: " << solidity;
        if (circularity > 0.1 && solidity > 0.3) {

            // Hľadáme najväčšiu loptu (ak by ich bolo viac)
            if (area > maxArea) {
                maxArea = area;
                bestIndex = (int)i;
                bestCenter = center;
                bestRadius = radius;
            }
        }
    }

    // 6. Ak sme našli vyhovujúci objekt
    if (bestIndex != -1) {
        outCenter = bestCenter;
        outRadius = bestRadius;

        qDebug() << "Lopta najdena! Area:" << maxArea << "Radius:" << bestRadius;
        return true;
    }

    return false;
}*/

/*bool MainWindow::detectBall(const cv::Mat &frame, float &outRadius, cv::Point &outCenter)
{
    if (frame.empty()) return false;

    // 1. Príprava pre Hough (Grayscale + Blur)
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    // MedianBlur je najlepší pre Hough, zachová hrany ale odstráni šum
    cv::medianBlur(gray, gray, 5);

    // 2. Príprava pre kontrolu farby (HSV Maska)
    cv::Mat hsv, redMask;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
    cv::Mat lowerRed1, upperRed1, lowerRed2, upperRed2;
    cv::inRange(hsv, cv::Scalar(0, 130, 80), cv::Scalar(10, 255, 255), lowerRed1);
    cv::inRange(hsv, cv::Scalar(170, 130, 80), cv::Scalar(180, 255, 255), lowerRed2);
    redMask = lowerRed1 | lowerRed2;
    // Jemne vycistime masku
    cv::dilate(redMask, redMask, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5)));

    // 3. Hough Circles
    std::vector<cv::Vec3f> circles;
    // Parametre si možno budeš musieť doladiť:
    // param1 (100) = Canny threshold (hrany)
    // param2 (30)  = Accumulator threshold (nižšie číslo = viac kruhov, aj falošných)
    // minRadius, maxRadius = nastav podľa vzdialenosti robota
    cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, 1, gray.rows/8, 100, 25, 10, 400);

    float bestRadius = 0;
    cv::Point bestCenter;
    bool found = false;

    // 4. VALIDÁCIA: Je ten kruh červený?
    for(size_t i = 0; i < circles.size(); i++)
    {
        cv::Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
        int radius = cvRound(circles[i][2]);

        // Vytvoríme ROI (výrez) okolo kruhu, aby sme neprechádzali celý obrázok
        // Ošetríme hranice obrazu
        int x = std::max(0, center.x - radius);
        int y = std::max(0, center.y - radius);
        int w = std::min(frame.cols - x, 2 * radius);
        int h = std::min(frame.rows - y, 2 * radius);

        if (w <= 0 || h <= 0) continue;

        cv::Rect roi(x, y, w, h);
        cv::Mat maskROI = redMask(roi);

        // Spočítame, koľko pixelov v tomto výreze je červených
        int redPixels = cv::countNonZero(maskROI);
        int totalPixels = w * h;

        // Ak je aspoň 40% plochy štvorca okolo kruhu červených, je to lopta.
        // (Kruh zaberá cca 78% štvorca, ak je lopta fľakatá, 40% je safe hranica)
        double ratio = (double)redPixels / totalPixels;

        if (ratio > 0.4)
        {
            // Našli sme červený kruh!
            // Ak nájdeme viac, berieme ten najväčší alebo najbližší
            if (radius > bestRadius) {
                bestRadius = (float)radius;
                bestCenter = center;
                found = true;
            }
        }
    }

    if (found) {
        outRadius = bestRadius;
        outCenter = bestCenter;
        qDebug() << "Hough nasiel cervenu loptu! Radius:" << bestRadius;
        return true;
    }

    return false;
}*/

bool MainWindow::detectBall(const cv::Mat &frame, float &outRadius, cv::Point &outCenter)
{
    if (frame.empty()) return false;

    // 1. Blur na odstránenie šumu a vyhladenie fľakov
    // Používame GaussianBlur, ktorý trochu rozmaže hrany fľakov
    cv::Mat blurred;
    cv::GaussianBlur(frame, blurred, cv::Size(9, 9), 2, 2);

    // 2. Prevod na HSV
    cv::Mat hsv;
    cv::cvtColor(blurred, hsv, cv::COLOR_BGR2HSV);

    // 3. Maska pre červenú farbu
    // Rozšíril som trochu dolné hranice (S=100, V=60), aby to chytilo aj tmavšie časti lopty v tieni
    cv::Mat lowerRed1, upperRed1, lowerRed2, upperRed2, redMask;
    cv::inRange(hsv, cv::Scalar(0, 100, 60), cv::Scalar(10, 255, 255), lowerRed1);
    cv::inRange(hsv, cv::Scalar(160, 100, 60), cv::Scalar(180, 255, 255), lowerRed2);
    redMask = lowerRed1 | lowerRed2;

    // 4. Morfológia - CLOSE spojí diery vnútri objektu
    // Toto je dôležité pre fľakatú loptu - "zaleje" diery
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
    cv::morphologyEx(redMask, redMask, cv::MORPH_CLOSE, kernel);

    // 5. Nájdenie kontúr
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(redMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    double maxArea = 0;
    int bestIndex = -1;
    float bestRadius = 0;
    cv::Point2f bestCenter;

    for (size_t i = 0; i < contours.size(); i++) {
        // Zoberieme plochu surovej kontúry (môže byť deravá kvôli fľakom)
        double rawArea = cv::contourArea(contours[i]);

        // Ignorujeme malé šumy
        if (rawArea < 500) continue;

        std::vector<cv::Point> hull;
        cv::convexHull(contours[i], hull);

        double hullArea = cv::contourArea(hull);
        double hullPerimeter = cv::arcLength(hull, true);

        if (hullPerimeter == 0) continue;

        // Výpočet kruhovitosti na základe OBALU 
        // Perfektný kruh = 1.0
        double hullCircularity = (4 * M_PI * hullArea) / (hullPerimeter * hullPerimeter);

        // Debug výpis (ak potrebuješ vidieť hodnoty)
        // qDebug() << "Objekt" << i << "HullCircularity:" << hullCircularity << "HullArea:" << hullArea;

        // --- FILTROVANIE ---
        // Lopta (aj fľakatá) bude mať HullCircularity blízko 1.0 (určite nad 0.75)
        // Kríž alebo čiara bude mať výrazne menej (pod 0.5), pretože majú veľký obvod a malú plochu
        if (hullCircularity > 0.75) {

            // Pre istotu skontrolujeme, či tento obal vypĺňa kružnicu (Solidity)
            cv::Point2f center;
            float radius;
            cv::minEnclosingCircle(hull, center, radius);

            double circleArea = M_PI * radius * radius;
            double fillRatio = hullArea / circleArea;

            // Lopta vyplní opísanú kružnicu aspoň na 70%
            if (fillRatio > 0.7) {
                // Berieme najväčší takýto objekt
                if (hullArea > maxArea) {
                    maxArea = hullArea;
                    bestIndex = (int)i;
                    bestCenter = center;
                    bestRadius = radius;
                }
            }
        }
    }

    if (bestIndex != -1) {
        outCenter = bestCenter;
        outRadius = bestRadius;

        // qDebug() << "Nasiel som loptu (Convex Hull)! Radius:" << bestRadius << "Area:" << maxArea;
        return true;
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

    if (prekazkaActive == true) {
        // Ak vidíme prekážku, okamžite stojíme
        _robot.setSpeedVal(0, 0);
        current_linear_speed = 0.0;
        qDebug() << "Prekazka detegovana! Stojim.";
        return; // Nepokračujeme v navigácii, kým prekážka nezmizne
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

            updatePointsTable(navigationPoints);
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
        if (angular_speed > 3.1415/8) angular_speed = 3.1415/8;
        if (angular_speed < -3.1415/8) angular_speed = -3.1415/8;

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

            state = MOVING;
            currentPointIndex++;
            if(lidarVis) lidarVis->setCurrentIndex(currentPointIndex);

            updatePointsTable(navigationPoints);

            _robot.setSpeedVal(0, 0);
            qDebug() << "Rotacia dokoncena.";

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

/*void MainWindow::on_pushButton_15_clicked(){
    notaus = !notaus;

    if(notaus){
        _robot.setSpeedVal(0,0);
    }else
        notaus = false;
}*/

void MainWindow::on_pushButton_15_clicked(){
    notaus = !notaus;

    if(notaus){
        // 1. Zastaviť robota
        _robot.setSpeedVal(0,0);

        // 2. Zmeniť farbu hlavného tlačidla
        ui->pushButton_15->setStyleSheet(
            "background-color: red; "
            "color: white; "
            "font-weight: bold; "
            "border: 3px solid darkred; "
            "text-align: center;"
            );

        // 3. VYTVORENIE VLASTNÉHO OKNA
        // Toto zaručí, že text bude presne v strede
        QDialog dialog(this);
        dialog.setWindowTitle("EMERGENCY STOP");
        dialog.setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint); // Odstráni ? tlačidlo
        dialog.setStyleSheet("background-color: #ffcccc;"); // Pozadie okna

        // Layout (rozloženie prvkov pod seba)
        QVBoxLayout *layout = new QVBoxLayout(&dialog);
        layout->setSpacing(20); // Medzera medzi textom a tlačidlom
        layout->setContentsMargins(20, 20, 20, 20); // Okraje okna

        // A) TEXT
        QLabel *label = new QLabel("⚠️ EMERGENCY STOP STLAČENÝ! ⚠️\n\nRobot bol okamžite zastavený.", &dialog);
        label->setAlignment(Qt::AlignCenter); // Zarovnanie na stred
        label->setStyleSheet("color: darkred; font-weight: bold; font-size: 14px; background: transparent;");
        layout->addWidget(label);

        // B) TLAČIDLO OK
        QPushButton *okButton = new QPushButton("OK", &dialog);
        okButton->setFixedWidth(100); // Pevná šírka tlačidla
        okButton->setStyleSheet(
            "QPushButton { background-color: red; color: white; border: 1px solid darkred; padding: 6px; font-weight: bold; border-radius: 4px; }"
            "QPushButton:hover { background-color: darkred; }"
            );
        // Prepojíme tlačidlo so zatvorením okna
        connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);

        // Pridáme tlačidlo do layoutu a zarovnáme ho na stred (alebo Qt::AlignRight ak chceš vpravo)
        layout->addWidget(okButton, 0, Qt::AlignCenter);

        // 4. Zobraziť okno
        dialog.exec();

    } else {
        notaus = false;
        ui->pushButton_15->setStyleSheet("");
        updateTheme();
    }
}

void MainWindow::showForbiddenError()
{
    errorDialog dlg(this);
    connect(&dlg, &errorDialog::requestZoneHighlight, lidarVis, &LidarVisualizer::toggleWallHighlight);
    dlg.exec();
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
    wallErrorDialog dlg(this);
    dlg.setModal(true);

    connect(&dlg, &wallErrorDialog::deleteRequested, lidarVis, &LidarVisualizer::removeInvalidPoints);

    dlg.exec();
}

void MainWindow::on_pushButton_10_clicked()
{
    isDarkMode = !isDarkMode;
    updateTheme();
}

void MainWindow::updateTheme()
{
    QString style;

    if (isDarkMode) {
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
        ui->pushButton_10->setIcon(QIcon(":/ikonky/9937122.png"));
    } else {
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
        ui->pushButton_10->setIcon(QIcon(":/ikonky/1664849-200.png"));
    }

    this->setStyleSheet(style);

    // --- VÝNIMKY ---
    if(cameraLabel) {
        cameraLabel->setStyleSheet("background-color: black; color: white; border: 2px solid gray;");
    }
    if(lidarVis) {
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
    QString statsFile = fullPath + "/info.avi";

    int fps_1 = 8;
    int fps_2 = 21;

    // --- OTVORENIE KAMERY ---
    videoWriterCamera.open(camFile.toStdString(), cv::VideoWriter::fourcc('M','J','P','G'), fps_1, cv::Size(640, 360), true);

    // --- OTVORENIE LIDARU  ---
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

    if(ui->widget_3) {
        int w = ui->widget_3->width();
        int h = ui->widget_3->height();

        // Ochrana: Rozmery musia byť párne (pre kodek MJPG)
        if (w % 2 != 0) w--;
        if (h % 2 != 0) h--;

        statsVideoSize = cv::Size(w, h);

        // Otvoríme video
        videoWriterStats.open(statsFile.toStdString(), cv::VideoWriter::fourcc('M','J','P','G'), fps_2, statsVideoSize, true);
    }
    
    if (videoWriterCamera.isOpened() && videoWriterLidar.isOpened() && videoWriterStats.isOpened()) {
        recording = true;
        lidarRecordTimer->start(1000 / fps_2);
        qDebug() << "Nahravanie spustene (Kamera, Lidar, Info).";
    } else {
        qDebug() << "CHYBA: Nepodarilo sa otvorit vsetky videa!";
        if (videoWriterCamera.isOpened()) videoWriterCamera.release();
        if (videoWriterLidar.isOpened()) videoWriterLidar.release();
        if (videoWriterStats.isOpened()) videoWriterStats.release();
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
    if (videoWriterStats.isOpened()) videoWriterStats.release();

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
    // Musíme zmeniť veľkosť obrázka..

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
}

void MainWindow::receiveFrontLidarPoints(const std::vector<double> &uhol, const std::vector<double> &vzdialenost)
{
    this->uhol_update = uhol;
    this->vzdialenost_update = vzdialenost;
}

/*double MainWindow::getDistanceToBall(double targetAngleRad)
{
    // Tolerancia +/- 5 stupňov (v radiánoch cca 0.08)
    const double ANGLE_TOLERANCE = 10.0 * (M_PI / 180.0);

    double minDistance = 10000.0; // Inicializujeme na "nekonečno"
    bool found = false;

    // Prejdeme všetky body z posledného scanu lidaru
    for (int i = 0; i < copyOfLaserData.numberOfScans; i++)
    {
        double dist = copyOfLaserData.Data[i].scanDistance;
        double angleDeg = copyOfLaserData.Data[i].scanAngle; // Uhol v stupňoch (0..360)

        // Ignorujeme chybné merania (príliš blízko alebo ďaleko)
        if (dist < 200  || dist > 6000) continue;

        // Prevod uhla Lidaru na systém kamery (-PI až +PI, kde 0 je vpredu)
        // Predpokladáme, že Lidar má 0 vpredu. Ak má 0 vzadu, treba pridať 180°.
        // V robot.cpp si mal logiku s negáciou, tu použijeme štandardnú normalizáciu:

        double lidarAngleRad = angleDeg * (M_PI / 180.0);

        // Normalizácia na rozsah [-PI, PI]
        while (lidarAngleRad > M_PI) lidarAngleRad -= 2.0 * M_PI;
        while (lidarAngleRad < -M_PI) lidarAngleRad += 2.0 * M_PI;

        // Pozor: Kamera má (Vľavo +, Vpravo -). Lidar to môže mať naopak.
        // Ak ti to nebude merať presne, skús odkomentovať tento riadok:
        // lidarAngleRad = -lidarAngleRad;

        // Ak je tento bod v smere lopty (v rámci tolerancie)
        if (fabs(lidarAngleRad - targetAngleRad) < ANGLE_TOLERANCE) {
            // Hľadáme najbližší bod v tomto výseku (lebo lopta je prekážka vpredu)
            if (dist < minDistance) {
                minDistance = dist;
                found = true;
            }
        }
    }

    if (found) return minDistance;
    return -1.0; // Lidar v danom smere nič nevidel (alebo je lopta príliš nízko)
}*/
double MainWindow::getDistanceToBall(double targetAngleRad)
{
    // Konvertujeme uhel z kamery (radiany) na stupne, lebo uhol_update je v stupnoch
    double targetAngleDeg = targetAngleRad * (180.0 / M_PI);

    double minDiff = 1000.0;
    double bestDistance = -1.0;

    // Prechádzame len tie body, ktoré sa vykresľujú do kamery (tie modré)
    // Tieto sú už správne otočené a vyfiltrované v robot.cpp
    for (size_t i = 0; i < uhol_update.size(); i++) {

        double currentAngle = uhol_update[i];
        double currentDist = vzdialenost_update[i];

        if (currentDist <= 0) continue;

        // Hľadáme bod, ktorý má uhol najbližšie k uhlu lopty
        double diff = std::abs(currentAngle - targetAngleDeg);

        if (diff < minDiff) {
            minDiff = diff;
            bestDistance = currentDist;
        }
    }

    // Ak sme našli bod, ktorý je uhlovo blízko (napr. do 5 stupňov od stredu lopty)
    if (minDiff < 10.0) {
        return bestDistance;
    }

    return -1.0; // Nenašli sme žiadny lidar bod v smere lopty
}

void MainWindow::on_pushButton_11_clicked()
{
    // Zastavíme robota a logiku navigácie, ak náhodou beží,
    // aby nám to nerobilo šarapatu počas pozerania replayu
    if (state != IDLE) {
        state = IDLE;
        _robot.setSpeedVal(0,0);
        navTimer->stop();
    }

    // Vytvoríme a zobrazíme dialóg
    ReplayDialog dlg(this);
    dlg.exec(); // Modálne okno - hlavné okno bude blokované kým sa replay nezavrie
}

void MainWindow::recordStatsFrame()
{
    // 1. Kontrola: nahrávame? existuje widget_3? je súbor otvorený?
    if (!recording || !ui->widget_3 || !videoWriterStats.isOpened()) return;

    // 2. Grabovanie (odfotenie) widgetu_3
    QPixmap pix = ui->widget_3->grab();
    if (pix.isNull() || pix.width() <= 0 || pix.height() <= 0) return;

    QImage img = pix.toImage().convertToFormat(QImage::Format_RGB888);

    // 3. Konverzia na OpenCV Mat
    cv::Mat mat(img.height(), img.width(), CV_8UC3, (uchar*)img.bits(), img.bytesPerLine());
    cv::Mat matBGR;
    cv::cvtColor(mat, matBGR, cv::COLOR_RGB2BGR);

    // 4. Resize (ak sa veľkosť okna zmenila)
    if (matBGR.size() != statsVideoSize) {
        try {
            cv::resize(matBGR, matBGR, statsVideoSize);
        } catch (...) {
            return;
        }
    }

    // 5. Zápis
    videoWriterStats.write(matBGR);
}

void MainWindow::onRowHeaderClicked(int index)
{
    // Index je číslo riadku (začína od 0)

    // Voliteľné: Dialóg na potvrdenie (aby si to nezmazal omylom)
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Zmazať bod",
                                  "Naozaj chcete zmazať bod č. " + QString::number(index + 1) + "?",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // Zavoláme funkciu vo vizualizéri
        if(lidarVis) {
            lidarVis->removePointAtIndex(index);
        }
    }
}

void MainWindow::onCellClicked(int row, int column)
{
    // Stĺpec 0 je X, 1 je Y, 2 je Typ
    // My chceme reagovať len na kliknutie do stĺpca "Typ" (index 2)
    if (column == 2) {
        if (lidarVis) {
            lidarVis->togglePointType(row);
        }
    }
}

void MainWindow::on_pushButton_12_clicked()
{
    if(lidarVis) {
        // 1. Zistíme aktuálny stav (či svieti alebo nie)
        bool aktualnyStav = lidarVis->getWallHighlight();

        // 2. Pošleme mu opačný stav (ak je true -> pošleme false, a naopak)
        lidarVis->toggleWallHighlight(!aktualnyStav);

        // Voliteľné: Výpis do konzoly pre kontrolu
        // qDebug() << "Zvyraznenie stien prepnute na:" << !aktualnyStav;
    }
}

void MainWindow::on_pushButton_14_clicked()
{
    // 1. OKAMZITE ZASTAVENIE
    _robot.setSpeedVal(0, 0);
    navTimer->stop();    

    // 2. UKONCENIE nahravania 
    if (recording) {
        stopRecording();
    }
    if (lidarRecordTimer->isActive()) {
        lidarRecordTimer->stop();
    }

    // 3. RESET premennych
    state = IDLE;
    currentPointIndex = 0;

    koniecMisie = false;
    notaus = false;   

    photoTaken = false;     
    prekazkaActive = false; 

    // 4. Vymazanie dat
    navigationPoints.clear();

    // 5. RESET mapy
    if (lidarVis) {
        lidarVis->reset();
    }

    // 6. Vycistenie tabulky
    ui->tableWidgetPoints->setRowCount(0);

    qDebug() << "--- SYSTEM KOMPLETNE RESETOVANY ---";
    QMessageBox::information(this, "Reset", "Misia bola resetovaná.\nMôžete zadať nové body a začať odznova.");
}
