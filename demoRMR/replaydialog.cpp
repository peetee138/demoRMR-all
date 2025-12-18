#include "replaydialog.h"
#include "ui_replaydialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>

ReplayDialog::ReplayDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ReplayDialog)
{
    ui->setupUi(this);

    // 1. Vytvoríme Labely pre obraz
    labelCamera = new QLabel(this);
    labelCamera->setAlignment(Qt::AlignCenter);
    labelCamera->setStyleSheet("background-color: black; color: white;");
    labelCamera->setScaledContents(false);

    labelLidar = new QLabel(this);
    labelLidar->setAlignment(Qt::AlignCenter);
    labelLidar->setStyleSheet("background-color: black; color: white;");
    labelLidar->setScaledContents(false);

    // 2. Časovač
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ReplayDialog::playNextFrame);

    // 3. Počiatočný stav
    isCameraBig = false;
    on_pushButton_3_clicked(); // Usporiadať okná

    mediaLoaded = false;
    ui->pushButton_2->setEnabled(false);
}

ReplayDialog::~ReplayDialog()
{
    if(capCamera.isOpened()) capCamera.release();
    if(capLidar.isOpened()) capLidar.release();
    delete ui;
}

// --- VYBRAŤ PRIEČINOK ---
/*void ReplayDialog::on_pushButton_clicked()
{
    QString rootPath = "C:/Users/petri/Downloads/kamera_kobuki/Zaznamy";
    QString dir = QFileDialog::getExistingDirectory(this, "Vyber priečinok misie", rootPath, QFileDialog::ShowDirsOnly);

    if (dir.isEmpty()) return;

    std::string camFile = (dir + "/kamera.avi").toStdString();
    std::string lidarFile = (dir + "/lidar.avi").toStdString();

    // Otvorenie OpenCV
    if (!capCamera.open(camFile) || !capLidar.open(lidarFile)) {
        QMessageBox::warning(this, "Chyba", "Nedajú sa otvoriť .avi súbory!");
        return;
    }

    // Získanie informácií o videu
    fpsCamera = capCamera.get(cv::CAP_PROP_FPS);
    totalFramesCamera = (long)capCamera.get(cv::CAP_PROP_FRAME_COUNT);

    fpsLidar = capLidar.get(cv::CAP_PROP_FPS);
    totalFramesLidar = (long)capLidar.get(cv::CAP_PROP_FRAME_COUNT);

    if (fpsCamera <= 0) fpsCamera = 8;   // Fallback ak zlyha detekcia
    if (fpsLidar <= 0) fpsLidar = 21;

    // Výpočet dĺžky v sekundách
    double durCam = totalFramesCamera / fpsCamera;
    double durLidar = totalFramesLidar / fpsLidar;

    // --- LOGIKA ZRÝCHLENIA ---
    // Určíme, ktoré video je kratšie (Master) a musíme prispôsobiť to dlhšie (Slave)

    if (durCam < durLidar) {
        masterVideo = 0; // Kamera je master (kratšia)
        speedRatio = durLidar / durCam;
        ui->label->setText(QString("Kamera: Master | Lidar zrýchlený (x%1)").arg(speedRatio, 0, 'f', 2));
    }
    else {
        masterVideo = 1; // Lidar je master (kratší)
        speedRatio = durCam / durLidar;
        ui->label->setText(QString("Lidar: Master | Kamera zrýchlená (x%1)").arg(speedRatio, 0, 'f', 2));
    }

    // Niekedy je pomer 1.0, ale počet FPS je iný.
    // V tomto zjednodušenom režime budeme časovač riadiť podľa FPS Mastra.

    accumulator = 0.0;
    mediaLoaded = true;
    ui->pushButton_2->setEnabled(true);
}*/

// --- PLAY ---
void ReplayDialog::on_pushButton_2_clicked()
{
    if (!mediaLoaded) return;

    // Nastavíme interval časovača podľa FPS Mastra
    // Napr. ak má kamera 8 FPS, timer tikne každých 125ms (1000/8)
    double masterFps = (masterVideo == 0) ? fpsCamera : fpsLidar;
    int interval = 1000 / masterFps;

    if (timer->isActive()) timer->stop();
    timer->start(interval);

    ui->label->setText("Prehrávam...");
}

// --- SLUČKA (Kľúčová funkcia) ---
/*void ReplayDialog::playNextFrame()
{
    cv::Mat frameCam, frameLidar;
    bool retCam = true;
    bool retLidar = true;

    if (masterVideo == 0) {
        // --- KAMERA JE MASTER (Kratšia) ---

        // 1. Prečítame 1 snímok z Kamery
        retCam = capCamera.read(frameCam);

        // 2. Prečítame "X" snímkov z Lidaru (podľa speedRatio)
        accumulator += speedRatio;
        int framesToSkip = (int)accumulator;
        accumulator -= framesToSkip;

        // "Preskočíme" framesToSkip-1 snímkov a prečítame ten posledný
        for (int i = 0; i < framesToSkip; i++) {
            retLidar = capLidar.read(frameLidar);
            if (!retLidar) break; // Koniec videa
        }
    }
    else {
        // --- LIDAR JE MASTER (Kratší) ---

        // 1. Prečítame 1 snímok z Lidaru
        retLidar = capLidar.read(frameLidar);

        // 2. Prečítame "X" snímkov z Kamery
        accumulator += speedRatio;
        int framesToSkip = (int)accumulator;
        accumulator -= framesToSkip;

        for (int i = 0; i < framesToSkip; i++) {
            retCam = capCamera.read(frameCam);
            if (!retCam) break;
        }
    }

    // Kontrola konca
    if (!retCam || !retLidar || frameCam.empty() || frameLidar.empty()) {
        timer->stop();
        ui->label->setText("Koniec videa.");
        return;
    }

    // Zobrazenie
    if (labelCamera->isVisible())
        labelCamera->setPixmap(QPixmap::fromImage(matToQImage(frameCam)));

    if (labelLidar->isVisible())
        labelLidar->setPixmap(QPixmap::fromImage(matToQImage(frameLidar)));
}*/
void ReplayDialog::on_pushButton_clicked()
{
    QString rootPath = "C:/Users/petri/Downloads/kamera_kobuki/Zaznamy";
    QString dir = QFileDialog::getExistingDirectory(this, "Vyber priečinok misie", rootPath, QFileDialog::ShowDirsOnly);

    if (dir.isEmpty()) return;

    std::string camFile = (dir + "/kamera.avi").toStdString();
    std::string lidarFile = (dir + "/lidar.avi").toStdString();

    if (!capCamera.open(camFile) || !capLidar.open(lidarFile)) {
        QMessageBox::warning(this, "Chyba", "Nedajú sa otvoriť .avi súbory!");
        return;
    }

    // --- ZÍSKANIE POČTU SNÍMKOV ---
    fpsCamera = capCamera.get(cv::CAP_PROP_FPS);
    totalFramesCamera = (long)capCamera.get(cv::CAP_PROP_FRAME_COUNT);

    fpsLidar = capLidar.get(cv::CAP_PROP_FPS);
    totalFramesLidar = (long)capLidar.get(cv::CAP_PROP_FRAME_COUNT);

    // Ochrana proti chybám v AVI hlavičke (ak vráti 0)
    if (fpsCamera <= 0) fpsCamera = 8;
    if (fpsLidar <= 0) fpsLidar = 21;
    if (totalFramesCamera <= 0) totalFramesCamera = 100; // Fallback
    if (totalFramesLidar <= 0) totalFramesLidar = 100;

    // Výpočet dĺžok
    double durCam = totalFramesCamera / fpsCamera;
    double durLidar = totalFramesLidar / fpsLidar;

    // --- LOGIKA MASTER / SLAVE ---
    // Master je ten KRATŠÍ (podľa času). Ten určuje tempo.
    if (durCam < durLidar) {
        masterVideo = 0; // Kamera je Master
        // Pomer: Koľko snímkov Lidaru pripadá na 1 snímok Kamery?
        // Používame pomer celkových počtov snímkov pre presnosť
        speedRatio = (double)totalFramesLidar / (double)totalFramesCamera;
        ui->label->setText(QString("Kamera Master (%1s) | Lidar zrýchlený (x%2)").arg(durCam, 0, 'f', 1).arg(durLidar/durCam, 0, 'f', 2));
    }
    else {
        masterVideo = 1; // Lidar je Master
        speedRatio = (double)totalFramesCamera / (double)totalFramesLidar;
        ui->label->setText(QString("Lidar Master (%1s) | Kamera zrýchlená (x%2)").arg(durLidar, 0, 'f', 1).arg(durCam/durLidar, 0, 'f', 2));
    }

    // Reset počítadiel
    currentFrameMaster = 0;
    currentFrameSlave = 0;

    mediaLoaded = true;
    ui->pushButton_2->setEnabled(true);
}

void ReplayDialog::playNextFrame()
{
    cv::Mat frameCam, frameLidar;
    bool retMaster = true;
    bool retSlave = true;

    // Pointery na to, kto je kto, aby sme nemuseli písať duplicitný kód
    cv::VideoCapture *capMaster = (masterVideo == 0) ? &capCamera : &capLidar;
    cv::VideoCapture *capSlave  = (masterVideo == 0) ? &capLidar  : &capCamera;

    cv::Mat *matMaster = (masterVideo == 0) ? &frameCam : &frameLidar;
    cv::Mat *matSlave  = (masterVideo == 0) ? &frameLidar : &frameCam;

    long *cntMaster = &currentFrameMaster;
    long *cntSlave  = &currentFrameSlave;

    // 1. Prečítame 1 snímok z MASTER videa
    if (capMaster->read(*matMaster)) {
        (*cntMaster)++;
    } else {
        retMaster = false; // Koniec master videa
    }

    // 2. Vypočítame, kde by mal byť SLAVE
    // Vzorec: (AktuálnyMaster / CelkovýMaster) * CelkovýSlave
    // Toto zaručí, že keď Master bude na 100%, Slave bude tiež presne na 100%
    long targetSlaveFrame = (long)((double)(*cntMaster) * speedRatio);

    // 3. Dobehneme Slave video (preskočíme snímky pomocou grab())
    // Používame while cyklus, kým nedosiahneme cieľový snímok
    while (*cntSlave < targetSlaveFrame) {
        // grab() je rýchlejšie ako read(), lebo len posunie hlavičku, nedekóduje obraz
        if (capSlave->grab()) {
            (*cntSlave)++;
        } else {
            retSlave = false;
            break; // Slave došiel na koniec skôr (nemalo by sa stať, ale pre istotu)
        }
    }

    // 4. Dekódujeme aktuálny snímok Slave videa
    // retrieve() dekóduje ten snímok, na ktorom sme zastavili po grab()
    if (retSlave) {
        if (!capSlave->retrieve(*matSlave)) {
            retSlave = false;
        }
    }

    // --- KONTROLA KONCA ---
    // Skončíme, len ak Master už nemá ďalšie snímky
    if (!retMaster || matMaster->empty()) {
        timer->stop();
        ui->label->setText("Koniec prehrávania.");
        return;
    }

    // Zobrazenie do Labelov
    if (labelCamera->isVisible()) {
        QPixmap pix = QPixmap::fromImage(matToQImage(frameCam));
        // Scaled s KeepAspectRatio zabezpečí, že obraz sa nedeformuje
        // SmoothTransformation zabezpečí, že čiary nebudú "zubaté" pri zmenšovaní
        labelCamera->setPixmap(pix.scaled(labelCamera->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    if (labelLidar->isVisible()) {
        QPixmap pix = QPixmap::fromImage(matToQImage(frameLidar));
        labelLidar->setPixmap(pix.scaled(labelLidar->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

// --- SWAP ---
void ReplayDialog::on_pushButton_3_clicked()
{
    isCameraBig = !isCameraBig;

    // 1. Odpojíme labely z aktuálneho miesta
    labelCamera->setParent(nullptr);
    labelLidar->setParent(nullptr);

    // 2. Pripravíme pointery na cieľové widgety podľa stavu
    QWidget *targetBig = nullptr;
    QWidget *targetSmall = nullptr;

    if (isCameraBig) {
        ui->stackedWidget->setCurrentWidget(ui->bigCamera);
        ui->stackedWidget_2->setCurrentWidget(ui->smallLidar);
        targetBig = ui->bigCamera;
        targetSmall = ui->smallLidar;

        ui->pushButton_3->setText("Swap: Cam->Small");
    } else {
        ui->stackedWidget->setCurrentWidget(ui->bigLidar);
        ui->stackedWidget_2->setCurrentWidget(ui->smallCamera);
        targetBig = ui->bigLidar;
        targetSmall = ui->smallCamera;

        ui->pushButton_3->setText("Swap: Lidar->Small");
    }

    // 3. VLOŽENIE: Ak layout neexistuje, vytvoríme ho! (Toto opraví biely obraz)

    // --- Veľké okno ---
    if (!targetBig->layout()) {
        QVBoxLayout *l = new QVBoxLayout(targetBig);
        l->setContentsMargins(0,0,0,0); // Aby bolo video roztiahnuté na celú plochu
    }
    // Vložíme správny label (ak je CameraBig, tak do Big ide kamera, inak lidar)
    targetBig->layout()->addWidget(isCameraBig ? labelCamera : labelLidar);

    // --- Malé okno ---
    if (!targetSmall->layout()) {
        QVBoxLayout *l = new QVBoxLayout(targetSmall);
        l->setContentsMargins(0,0,0,0);
    }
    targetSmall->layout()->addWidget(isCameraBig ? labelLidar : labelCamera);

    // 4. Zobraziť
    labelCamera->show();
    labelLidar->show();
}

// --- POMOCNÁ: OpenCV Mat -> Qt QImage ---
QImage ReplayDialog::matToQImage(const cv::Mat &mat)
{
    if(mat.empty()) return QImage();

    // OpenCV je BGR, Qt chce RGB
    if(mat.type() == CV_8UC3) {
        // Vytvoríme kópiu s prehodenými kanálmi
        QImage img(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_BGR888);
        return img.copy(); // Copy vynúti hlbokú kópiu dát, čo je bezpečnejšie pre GUI
    }
    else if(mat.type() == CV_8UC1) {
        QImage img(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
        return img.copy();
    }
    return QImage();
}
