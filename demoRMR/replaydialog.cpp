#include "replaydialog.h"
#include "ui_replaydialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QVBoxLayout> // Potrebné pre layout

ReplayDialog::ReplayDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ReplayDialog)
{
    ui->setupUi(this);

    // 1. Vytvoríme Labely pre obraz (Kamera a Lidar)
    labelCamera = new QLabel(this);
    labelCamera->setAlignment(Qt::AlignCenter);
    labelCamera->setStyleSheet("background-color: black; color: white;");
    labelCamera->setScaledContents(false);

    labelLidar = new QLabel(this);
    labelLidar->setAlignment(Qt::AlignCenter);
    labelLidar->setStyleSheet("background-color: black; color: white;");
    labelLidar->setScaledContents(false);

    // --- 2. NOVÉ: Label pre Info/Stats (widget_2) ---
    labelInfo = new QLabel(this);
    labelInfo->setAlignment(Qt::AlignCenter);
    labelInfo->setStyleSheet("background-color: black; color: white;");
    labelInfo->setScaledContents(true); // Zachovať pomer strán

    // Vloženie do widget_2
    // Skontrolujeme, či widget_2 už má layout (ak nie, vytvoríme ho)
    if (!ui->widget_2->layout()) {
        QVBoxLayout *layout = new QVBoxLayout(ui->widget_2);
        layout->setContentsMargins(0, 0, 0, 0);
        ui->widget_2->setLayout(layout);
    }
    ui->widget_2->layout()->addWidget(labelInfo);


    // 3. Časovač
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ReplayDialog::playNextFrame);

    // 4. Počiatočný stav
    isCameraBig = false;
    on_pushButton_3_clicked();

    mediaLoaded = false;
    ui->pushButton_2->setEnabled(false);
}

ReplayDialog::~ReplayDialog()
{
    if(capCamera.isOpened()) capCamera.release();
    if(capLidar.isOpened()) capLidar.release();
    // Uvoľniť aj info
    if(capInfo.isOpened()) capInfo.release();
    delete ui;
}

// --- VYBRAŤ PRIEČINOK ---
void ReplayDialog::on_pushButton_clicked()
{
    QString rootPath = "C:/Users/petri/Downloads/kamera_kobuki/Zaznamy";
    QString dir = QFileDialog::getExistingDirectory(this, "Vyber priečinok misie", rootPath, QFileDialog::ShowDirsOnly);

    if (dir.isEmpty()) return;

    // Cesty k súborom
    std::string camFile = (dir + "/kamera.avi").toStdString();
    std::string lidarFile = (dir + "/lidar.avi").toStdString();
    std::string infoFile = (dir + "/info.avi").toStdString(); // <--- NOVÉ

    // Otvorenie videí
    // Info video je voliteľné (staršie nahrávky ho nemusia mať), takže nezlyháme ak chýba
    bool cameraOk = capCamera.open(camFile);
    bool lidarOk = capLidar.open(lidarFile);
    bool infoOk = capInfo.open(infoFile);

    if (!cameraOk || !lidarOk) {
        QMessageBox::warning(this, "Chyba", "Nedajú sa otvoriť základné .avi súbory (kamera/lidar)!");
        return;
    }

    // --- ZÍSKANIE POČTU SNÍMKOV ---
    fpsCamera = capCamera.get(cv::CAP_PROP_FPS);
    totalFramesCamera = (long)capCamera.get(cv::CAP_PROP_FRAME_COUNT);

    fpsLidar = capLidar.get(cv::CAP_PROP_FPS);
    totalFramesLidar = (long)capLidar.get(cv::CAP_PROP_FRAME_COUNT);

    // Info frames
    totalFramesInfo = infoOk ? (long)capInfo.get(cv::CAP_PROP_FRAME_COUNT) : 0;

    // Ochrana proti chybám
    if (fpsCamera <= 0) fpsCamera = 8;
    if (fpsLidar <= 0) fpsLidar = 21;
    if (totalFramesCamera <= 0) totalFramesCamera = 100;
    if (totalFramesLidar <= 0) totalFramesLidar = 100;

    double durCam = totalFramesCamera / fpsCamera;
    double durLidar = totalFramesLidar / fpsLidar;

    // --- LOGIKA MASTER / SLAVE ---
    if (durCam < durLidar) {
        masterVideo = 0; // Kamera je Master
        speedRatio = (double)totalFramesLidar / (double)totalFramesCamera;

        // Výpočet pomeru pre Info video
        if (totalFramesInfo > 0)
            speedRatioInfo = (double)totalFramesInfo / (double)totalFramesCamera;
        else
            speedRatioInfo = 0;

        ui->label->setText(QString("Kamera Master | Lidar x%1").arg(speedRatio, 0, 'f', 2));
    }
    else {
        masterVideo = 1; // Lidar je Master
        speedRatio = (double)totalFramesCamera / (double)totalFramesLidar;

        // Výpočet pomeru pre Info video
        if (totalFramesInfo > 0)
            speedRatioInfo = (double)totalFramesInfo / (double)totalFramesLidar;
        else
            speedRatioInfo = 0;

        ui->label->setText(QString("Lidar Master | Kamera x%1").arg(speedRatio, 0, 'f', 2));
    }

    // Reset počítadiel
    currentFrameMaster = 0;
    currentFrameSlave = 0;
    currentFrameInfo = 0; // <--- NOVÉ

    mediaLoaded = true;
    ui->pushButton_2->setEnabled(true);
}

// --- SLUČKA PREHRÁVANIA ---
void ReplayDialog::playNextFrame()
{
    cv::Mat frameCam, frameLidar, frameInfo;
    bool retMaster = true;
    bool retSlave = true;
    bool retInfo = true;

    // Pointery pre Master/Slave logiku (Kamera vs Lidar)
    cv::VideoCapture *capMaster = (masterVideo == 0) ? &capCamera : &capLidar;
    cv::VideoCapture *capSlave  = (masterVideo == 0) ? &capLidar  : &capCamera;

    cv::Mat *matMaster = (masterVideo == 0) ? &frameCam : &frameLidar;
    cv::Mat *matSlave  = (masterVideo == 0) ? &frameLidar : &frameCam;

    long *cntMaster = &currentFrameMaster;
    long *cntSlave  = &currentFrameSlave;

    // 1. MASTER VIDEO: Prečítame 1 snímok
    if (capMaster->read(*matMaster)) {
        (*cntMaster)++;
    } else {
        retMaster = false;
    }

    // 2. SLAVE VIDEO (Kamera alebo Lidar): Synchronizácia
    long targetSlaveFrame = (long)((double)(*cntMaster) * speedRatio);
    while (*cntSlave < targetSlaveFrame) {
        if (capSlave->grab()) (*cntSlave)++;
        else { retSlave = false; break; }
    }
    if (retSlave) {
        if (!capSlave->retrieve(*matSlave)) retSlave = false;
    }

    // 3. INFO VIDEO (widget_2): Synchronizácia
    // Používame rovnakú logiku: Vypočítame cieľový snímok podľa Mastra
    if (capInfo.isOpened()) {
        long targetInfoFrame = (long)((double)(*cntMaster) * speedRatioInfo);

        while (currentFrameInfo < targetInfoFrame) {
            if (capInfo.grab()) currentFrameInfo++;
            else { retInfo = false; break; }
        }

        if (retInfo) {
            if(!capInfo.retrieve(frameInfo)) retInfo = false;
        }
    }

    // --- KONTROLA KONCA ---
    if (!retMaster || matMaster->empty()) {
        timer->stop();
        ui->label->setText("Koniec prehrávania.");
        return;
    }

    // --- ZOBRAZENIE ---

    // Kamera
    if (labelCamera->isVisible() && !frameCam.empty()) {
        QPixmap pix = QPixmap::fromImage(matToQImage(frameCam));
        labelCamera->setPixmap(pix.scaled(labelCamera->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    // Lidar
    if (labelLidar->isVisible() && !frameLidar.empty()) {
        QPixmap pix = QPixmap::fromImage(matToQImage(frameLidar));
        labelLidar->setPixmap(pix.scaled(labelLidar->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    // Info / Stats (widget_2) <--- NOVÉ
    if (capInfo.isOpened() && !frameInfo.empty()) {
        QPixmap pix = QPixmap::fromImage(matToQImage(frameInfo));
        // Zobrazíme do labelInfo
        labelInfo->setPixmap(pix.scaled(labelInfo->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

// --- TLAČIDLO PLAY (on_pushButton_2_clicked) ---
void ReplayDialog::on_pushButton_2_clicked()
{
    if (!mediaLoaded) return;

    // Nastavíme interval časovača podľa FPS Mastra
    double masterFps = (masterVideo == 0) ? fpsCamera : fpsLidar;
    int interval = 1000 / masterFps;

    if (timer->isActive()) timer->stop();
    timer->start(interval);

    ui->label->setText("Prehrávam...");
}

// --- TLAČIDLO SWAP (on_pushButton_3_clicked) ---
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
        // Cam -> Big, Lidar -> Small
        ui->stackedWidget->setCurrentWidget(ui->bigCamera);
        ui->stackedWidget_2->setCurrentWidget(ui->smallLidar);
        targetBig = ui->bigCamera;
        targetSmall = ui->smallLidar;

        ui->pushButton_3->setText("Swap: Cam->Small");
    } else {
        // Lidar -> Big, Cam -> Small
        ui->stackedWidget->setCurrentWidget(ui->bigLidar);
        ui->stackedWidget_2->setCurrentWidget(ui->smallCamera);
        targetBig = ui->bigLidar;
        targetSmall = ui->smallCamera;

        ui->pushButton_3->setText("Swap: Lidar->Small");
    }

    // 3. VLOŽENIE: Ak layout neexistuje, vytvoríme ho

    // --- Veľké okno ---
    if (!targetBig->layout()) {
        QVBoxLayout *l = new QVBoxLayout(targetBig);
        l->setContentsMargins(0,0,0,0);
    }
    // Vložíme správny label
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

// --- POMOCNÁ FUNKCIA: matToQImage ---
QImage ReplayDialog::matToQImage(const cv::Mat &mat)
{
    if(mat.empty()) return QImage();

    // OpenCV je BGR, Qt chce RGB
    if(mat.type() == CV_8UC3) {
        // Vytvoríme kópiu s prehodenými kanálmi
        QImage img(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_BGR888);
        return img.copy(); // Copy vynúti hlbokú kópiu dát
    }
    else if(mat.type() == CV_8UC1) {
        QImage img(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
        return img.copy();
    }
    return QImage();
}
