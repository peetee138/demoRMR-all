#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPainter>
#include <QDebug>
#include <math.h>
#include <QMessageBox>
#include <QKeyEvent>

///TOTO JE DEMO PROGRAM...AK SI HO NASIEL NA PC V LABAKU NEPREPISUJ NIC,ALE SKOPIRUJ SI MA NIEKAM DO INEHO FOLDERA
/// AK HO MAS Z GITU A ROBIS NA LABAKOVOM PC, TAK SI HO VLOZ DO FOLDERA KTORY JE JASNE ODLISITELNY OD TVOJICH KOLEGOV
/// NASLEDNE V POLOZKE Projects SKONTROLUJ CI JE VYPNUTY shadow build...
/// POTOM MIESTO TYCHTO PAR RIADKOV NAPIS SVOJE MENO ALEBO NEJAKY INY LUKRATIVNY IDENTIFIKATOR
/// KED SA NAJBLIZSIE PUSTIS DO PRACE, SKONTROLUJ CI JE MIESTO TOHTO TEXTU TVOJ IDENTIFIKATOR
/// AZ POTOM ZACNI ROBIT... AK TO NESPRAVIS, POJDU BODY DOLE... A NIE JEDEN,ALEBO DVA ALE BUDES RAD
/// AK SA DOSTANES NA SKUSKU


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{

    //tu je napevno nastavena ip. treba zmenit na to co ste si zadali do text boxu alebo nejaku inu pevnu. co bude spravna
    ipaddress="127.0.0.1";//192.168.1.11toto je na niektory realny robot.. na lokal budete davat "127.0.0.1"

    ui->setupUi(this);

    lidarVis = new LidarVisualizer(this);
    lidarVis->setRobot(&_robot); // Odovzdáme mu pointer na robota

    // 1. Pripravíme layout pre Startovaciu obrazovku (lidarEnterWidget)
    // Predpokladam, ze v .ui mas widget s nazvom "lidarEnterWidget"
    if(ui->lidarEnterWidget) {
        QVBoxLayout* l = new QVBoxLayout(ui->lidarEnterWidget);
        l->setContentsMargins(0, 0, 0, 0); // Aby bol roztiahnuty na cele okno
        l->addWidget(lidarVis); // Vlozime ho sem na zaciatku
    }

    // 2. Pripravíme layout aj pre Hlavnu obrazovku (lidarWidget), aby bol nachystany
    if(ui->lidarWidget && !ui->lidarWidget->layout()) {
        QVBoxLayout* l = new QVBoxLayout(ui->lidarWidget);
        l->setContentsMargins(0, 0, 0, 0);
    }

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

    qDebug() << "Video sa uloží sem:" << videoPath; qDebug() << "Fotka sa uloží sem:" << photoPath;

    datacounter=0;
#ifndef DISABLE_OPENCV
    actIndex=-1;
    useCamera1=false;

#endif


    datacounter=0;
    _robot.startLoging=false;

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updatePointsTable(const std::vector<MapPoint> &points)
{
    ui->tableWidgetPoints->setRowCount(0); // Vymazem stare

    for(const auto& p : points)
    {
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

// Slot na ulozenie do suboru
void MainWindow::on_pushButton_13_clicked()
{
    std::vector<MapPoint> points = lidarVis->getPoints();
    if(points.empty()) {
        QMessageBox::warning(this, "Pozor", "Ziadne body na ulozenie!");
        return;
    }

    QString filename = "trasa_bot.txt";
    QFile file(filename);
    if(file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
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

void MainWindow::on_pushButton_12_clicked()
{
    // Bezpecnostna kontrola
    if(!lidarVis || !ui->lidarWidget || !ui->lidarEnterWidget) return;

    // Zistime, kde sa lidarVis prave nachadza
    // parentWidget() vrati pointer na widget, v ktorom je lidarVis vlozeny
    QWidget *currentParent = lidarVis->parentWidget();

    if (currentParent == ui->lidarEnterWidget) {
        // --- PREPNUT NA HLAVNU OBRAZOVKU ---

        // Ziskame layout hlavneho widgetu a vlozime tam lidarVis
        // addWidget ho automaticky zoberie z enterWidgetu a da ho sem
        ui->lidarWidget->layout()->addWidget(lidarVis);

        qDebug() << "Lidar presunuty do HLAVNEHO okna (lidarWidget)";
    }
    else {
        // --- VRATIT NA STARTOVACIU OBRAZOVKU ---

        ui->lidarEnterWidget->layout()->addWidget(lidarVis);

        qDebug() << "Lidar presunuty do STARTOVACIEHO okna (lidarEnterWidget)";
    }
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    qDebug()<<"nahravaie: "<<recording;
    QPainter painter(this);
    ///prekreslujem obrazovku len vtedy, ked viem ze mam nove data. paintevent sa
    /// moze pochopitelne zavolat aj z inych dovodov, napriklad zmena velkosti okna
    painter.setBrush(Qt::black);//cierna farba pozadia(pouziva sa ako fill pre napriklad funkciu drawRect)
    QPen pero;
    pero.setStyle(Qt::SolidLine);//styl pera - plna ciara
    pero.setWidth(3);//hrubka pera -3pixely
    pero.setColor(Qt::green);//farba je zelena
    QRect rect;
    rect= ui->centralWidget->geometry();//ziskate porametre stvorca,do ktoreho chcete kreslit
    rect.translate(0,15);
    painter.drawRect(rect);
#ifndef DISABLE_OPENCV
    /*if(useCamera1==true && actIndex>-1)/// ak zobrazujem data z kamery a aspon niektory frame vo vectore je naplneny
    {*/
        std::cout<<actIndex<<std::endl;
        QImage image = QImage((uchar*)frame[actIndex].data, frame[actIndex].cols, frame[actIndex].rows, frame[actIndex].step, QImage::Format_RGB888  );//kopirovanie cvmat do qimage
        painter.drawImage(rect,image.rgbSwapped());
    //}
    //else
#endif
    /*{
        if(updateLaserPicture==1) ///ak mam nove data z lidaru
        {
            updateLaserPicture=0;


            pero.setColor(Qt::red);//farba je zelena
            painter.setPen(pero);
            painter.drawEllipse(QPoint(rect.width()/2+rect.topLeft().x(), rect.height()/2+rect.topLeft().y()),15,15);
            painter.drawLine(QPoint(rect.width()/2+rect.topLeft().x(), rect.height()/2+rect.topLeft().y()),QPoint(rect.width()/2+rect.topLeft().x(), rect.height()/2+rect.topLeft().y()-15));
            pero.setColor(Qt::green);//farba je zelena
            painter.setPen(pero);
            //teraz tu kreslime random udaje... vykreslite to co treba... t.j. data z lidaru
            //   std::cout<<copyOfLaserData.numberOfScans<<std::endl;
            #ifdef DISABLE_AMCL
            for(int k=0;k<copyOfLaserData.numberOfScans;k++)
            {
                int dist=copyOfLaserData.Data[k].scanDistance/20; ///vzdialenost nahodne predelena 20 aby to nejako vyzeralo v okne.. zmen podla uvazenia
                int xp=rect.width()-(rect.width()/2+dist*2*sin((360.0-copyOfLaserData.Data[k].scanAngle)*3.14159/180.0))+rect.topLeft().x(); //prepocet do obrazovky
                int yp=rect.height()-(rect.height()/2+dist*2*cos((360.0-copyOfLaserData.Data[k].scanAngle)*3.14159/180.0))+rect.topLeft().y();//prepocet do obrazovky
                if(rect.contains(xp,yp))//ak je bod vo vnutri nasho obdlznika tak iba vtedy budem chciet kreslit
                    painter.drawEllipse(QPoint(xp, yp),2,2);
            }
#else
           int rows = static_cast<int>(_robot.getAmclMap().height);
            int cols = static_cast<int>(_robot.getAmclMap().width);

            double cellWidth  = static_cast<double>(rect.width()) / cols;
            double cellHeight = static_cast<double>(rect.height()) / rows;

            for (int r = 0; r < rows; ++r) {
                for (int c = 0; c < cols; ++c) {
                    QRectF cellRect(c * cellWidth+rect.topLeft().x(), r * cellHeight+rect.topLeft().y(), cellWidth, cellHeight);

                    if (_robot.getAmclMap().distanceField[_robot.getAmclMap().index(c,r)]==1)
                        painter.setBrush( QColor(0, 200, 0));  // green
                    else if (_robot.getAmclMap().distanceField[_robot.getAmclMap().index(c,r)]>=2 && _robot.getAmclMap().distanceField[_robot.getAmclMap().index(c,r)] <=4)
                        painter.setBrush( QColor(100, 100, 0));  // green
                    else
                        painter.setBrush( Qt::black);

                    painter.setPen(QColor(100, 100, 100)); // light gray lines
                    painter.drawRect(cellRect);

                }
            }

            float robotXp=_robot.getBestParticle().x;
            float robotYp=_robot.getBestParticle().y;
            float robotThetaP=_robot.getBestParticle().theta;
            painter.setPen(QColor(200, 0, 0)); // light gray lines
            for(int k=0;k<copyOfLaserData.numberOfScans;k++)
            {
                float angleRad = robotThetaP -( copyOfLaserData.Data[k].scanAngle) * 3.14159 / 180.0f;
                float lx = robotXp + copyOfLaserData.Data[k].scanDistance * std::cos(angleRad);
                float ly = robotYp + copyOfLaserData.Data[k].scanDistance * std::sin(angleRad);
                int gx, gy;
                _robot.getGridCoordinates(lx, ly, gx, gy);
                //_robot.robotCom.amcld.worldToGrid(lx, ly, gx, gy, _robot.robotCom.amclmap);
                QRectF cellRect(gx * cellWidth+rect.topLeft().x(), gy * cellHeight+rect.topLeft().y(), cellWidth, cellHeight);
                 painter.drawRect(cellRect);

            }
#endif
        }
    }
#ifndef DISABLE_SKELETON
    if(updateSkeletonPicture==1 )
    {
        painter.setPen(Qt::red);
        for(int i=0;i<75;i++)
        {
            int xp=rect.width()-rect.width() * skeleJoints.joints[i].x+rect.topLeft().x();
            int yp= (rect.height() *skeleJoints.joints[i].y)+rect.topLeft().y();
            if(rect.contains(xp,yp))
                painter.drawEllipse(QPoint(xp, yp),2,2);
        }
    }
#endif*/
}


/// toto je slot. niekde v kode existuje signal, ktory je prepojeny. pouziva sa napriklad (v tomto pripade) ak chcete dostat data z jedneho vlakna (robot) do ineho (ui)
/// prepojenie signal slot je vo funkcii  on_pushButton_9_clicked
void  MainWindow::setUiValues(double robotX,double robotY,double robotFi)
{
    /*ui->lineEdit_2->setText(QString::number(robotX));
    ui->lineEdit_3->setText(QString::number(robotY));
    ui->lineEdit_4->setText(QString::number(robotFi));*/
}
#ifndef DISABLE_AMCL
void MainWindow::setUiAMCLValues(double robotX, double robotY, double robotFi)
{
    ui->lineEdit_2->setText("X = " + QString::number(robotX));
    ui->lineEdit_3->setText("Y = " + QString::number(robotY));
    ui->lineEdit_4->setText("Fi = " + QString::number(robotFi));

    //::cout<<"poloha z amcl dosla.. asi si to uprav"<<std::endl;
}
#endif

void MainWindow::on_pushButton_9_clicked() //start button
{
    //ziskanie joystickov

    QString zadany_text = ui->lineEdit->text();
    if (zadany_text.isEmpty()){
        this->ipaddress = "127.0.0.1";
        qDebug() << "IP adresa nastavená na" << QString::fromStdString(ipaddress);
    }
    else{
        this->ipaddress = zadany_text.toStdString();
        qDebug() << "IP adresa nastavená na" << QString::fromStdString(ipaddress);
    }
    //tu sa nastartuju vlakna ktore citaju data z lidaru a robota
    //recording = false; // reset

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
    /// prepojenie joysticku s jeho callbackom... zas cez lambdu. neviem ci som to niekde spominal,ale lambdy su super. okrem toho mam este rad ternarne operatory a spolocneske hry ale to tiez nikoho nezaujima
    /// co vas vlastne zaujima? citanie komentov asi nie, inak by ste citali toto a ze tu je blbosti
    connect(
        instance, &QJoysticks::axisChanged,
        [this]( const int js, const int axis, const qreal value) {
            double forw=0, rot=0;
            if(/*js==0 &&*/ axis==1){forw=-value*300;}
            if(/*js==0 &&*/ axis==0){rot=-value*(3.14159/2.0);}
            this->_robot.setSpeedVal(forw,rot);
        }
        );
#endif
}

void MainWindow::on_lineEdit_returnPressed()
{
    // Vykoná rovnakú akciu ako kliknutie na tlačidlo Štart
    on_pushButton_9_clicked();
}

void MainWindow::on_pushButton_2_clicked() //forward
{
    //pohyb dopredu
    _robot.setSpeedVal(200,0);

}

void MainWindow::on_pushButton_3_clicked() //back
{
    _robot.setSpeedVal(-100,0);

}

void MainWindow::on_pushButton_6_clicked() //left
{
    _robot.setSpeedVal(0,3.14159/8);

}

void MainWindow::on_pushButton_5_clicked()//right
{
    _robot.setSpeedVal(0,-3.14159/8);

}

void MainWindow::on_pushButton_4_clicked() //stop
{
    _robot.setSpeedVal(0,0);

}

/*void MainWindow::on_pushButton_12_clicked(){
    if (recording) {
        recording = false;          // zastaví nahrávanie
        videoWriter.release();      // uvoľní súbor
        qDebug() << "Nahrávanie zastavené pomocou tlačidla 8";
    }
}*/




void MainWindow::on_pushButton_clicked()
{
#ifndef DISABLE_OPENCV
    if(useCamera1==true)
    {
        useCamera1=false;

        ui->pushButton->setText("use camera");
    }
    else
    {
        useCamera1=true;

        ui->pushButton->setText("use laser");
    }
#endif
}


void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Bezpecnostna kontrola - ci mame vsetky widgety
    if(!lidarVis || !ui->lidarWidget || !ui->lidarEnterWidget) return;

    // --- STLACENIE "P" (Presun do lidarEnterWidget) ---
    if(event->key() == Qt::Key_P)
    {
        // Skontrolujeme ci ma widget layout, do ktoreho mozeme vlozit
        if(ui->lidarEnterWidget->layout()) {
            ui->lidarEnterWidget->layout()->addWidget(lidarVis);
            qDebug() << "Stlacene P -> Lidar presunuty do lidarEnterWidget";
        }
    }

    // --- STLACENIE "M" (Presun do lidarWidget) ---
    else if(event->key() == Qt::Key_M)
    {
        if(ui->lidarWidget->layout()) {
            ui->lidarWidget->layout()->addWidget(lidarVis);
            qDebug() << "Stlacene M -> Lidar presunuty do lidarWidget";
        }
    }

    // Zavolame povodnu funkcionalitu pre ostatne klavesy
    QMainWindow::keyPressEvent(event);
}


int MainWindow::paintThisLidar(const LaserMeasurement &laserData)
{
    /* memcpy( &copyOfLaserData,&laserData,sizeof(LaserMeasurement));
    updateLaserPicture=1;

    update();
    return 0;*/

    // Odstranujeme memcpy do lokalnej premennej MainWindow, posielame to rovno vizualizeru
    if(lidarVis) {
        lidarVis->updateLidarData(laserData);
    }

    // update(); <--- TOTO VYMAZ ALEBO ZAKOMENTUJ, aby sa neprekreslovalo cele okno kvoli lidaru
    return 0;
}

#ifndef DISABLE_OPENCV

///toto je calback na data z kamery, ktory ste podhodili robotu vo funkcii initAndStartRobot
/// vola sa ked dojdu nove data z kamery
int MainWindow::paintThisCamera(const cv::Mat &cameraData)
{
    cv::Mat frameCopy;
    cameraData.copyTo(frameCopy);

    frameCopy.copyTo(frame[(actIndex+1)%3]);
    actIndex = (actIndex+1)%3;

    // -----------------------------
    // 1) Spustenie nahrávania
    // -----------------------------
    if (!recording) {

        int fps = 20;
        cv::Size size(frameCopy.cols, frameCopy.rows);

        // MUSÍ byť .avi !!!
        if (!videoWriter.open(videoPath.toStdString(),
                              cv::VideoWriter::fourcc('M','J','P','G'),
                              fps,
                              size,
                              true))
        {
            qDebug() << "ERROR: VideoWriter could NOT open file:" << videoPath;
        }
        else {
            recording = true;
            qDebug() << "Recording started..." << videoPath;
        }
    }

    // -----------------------------
    // 2) Zapis videa
    // -----------------------------
    if (recording && videoWriter.isOpened()) {
        videoWriter.write(frameCopy);
    }

    // -----------------------------
    // 3) Detekcia lopty a uloženie fotky
    // -----------------------------
    if (!photoTaken && detectBall(frameCopy)) {

        qDebug() << "BALL DETECTED!";

        cv::imwrite(photoPath.toStdString(), frameCopy);
        photoTaken = true;

        // stop video
        if (recording) {
            recording = false;
            videoWriter.release();
            qDebug() << "Recording stopped.";
        }
    }

    updateLaserPicture = 1;

    this->update();
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

/*void MainWindow::on_pushButton_10_clicked()
{
    _robot.startLoging=!_robot.startLoging;
    if(_robot.startLoging==true)
        ui->pushButton_10->setText("Stop Logging");
    else
        ui->pushButton_10->setText("Start Logging");

}*/

bool MainWindow::detectBall(const cv::Mat &frame)
{
    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    // červená farba – dve masky
    cv::Mat lowerRed, upperRed;
    cv::inRange(hsv, cv::Scalar(0, 150, 80), cv::Scalar(10, 255, 255), lowerRed);
    cv::inRange(hsv, cv::Scalar(170, 150, 80), cv::Scalar(180, 255, 255), upperRed);

    cv::Mat redMask = lowerRed | upperRed;

    // žltá (ak máš žltú loptu)
    cv::Mat yellowMask;
    cv::inRange(hsv, cv::Scalar(15, 120, 120), cv::Scalar(35, 255, 255), yellowMask);

    cv::Mat mask = redMask | yellowMask;

    // najdi kontúry
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (auto &c : contours) {
        double area = cv::contourArea(c);

        if (area > 800) {   // prahovanie
            return true;
        }
    }

    return false;
}
