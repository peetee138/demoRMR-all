#include "lidarvisualizer.h"
#include <math.h>

LidarVisualizer::LidarVisualizer(QWidget *parent) : QWidget(parent)
{
    _robot = nullptr;
    hasData = false;

    // Zoznam sa inicializuje automaticky prazdny

    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    this->setAutoFillBackground(true);
    this->setPalette(pal);
}

void LidarVisualizer::setRobot(robot *robotPtr)
{
    _robot = robotPtr;
}

void LidarVisualizer::updateLidarData(const LaserMeasurement &data)
{
    memcpy(&copyOfLaserData, &data, sizeof(LaserMeasurement));
    hasData = true;
    update();
}

void LidarVisualizer::mousePressEvent(QMouseEvent *event)
{
    if(_robot == nullptr) return;

    // 1. Ziskame rozmery mapy
    int rows = static_cast<int>(_robot->getAmclMap().height);
    int cols = static_cast<int>(_robot->getAmclMap().width);
    if(rows == 0 || cols == 0) return;

    // --- NOVÉ: Získame vypočítaný obdĺžnik mapy (82:66) ---
    QRect mapRect = getMapRect();

    // Ak sme klikli mimo mapy, ignorujeme
    if (!mapRect.contains(event->pos())) return;

    // 2. Velkost bunky (vypocitana z mapRect, nie z celeho widgetu)
    double cellWidth  = static_cast<double>(mapRect.width()) / cols;
    double cellHeight = static_cast<double>(mapRect.height()) / rows;

    // 3. Prepocet kliknutia na mriezku (odcitame offset mapRect)
    int gridX = static_cast<int>((event->x() - mapRect.x()) / cellWidth);
    int gridY = static_cast<int>((event->y() - mapRect.y()) / cellHeight);

    // 4. Overenie hranic
    if(gridX >= 0 && gridX < cols && gridY >= 0 && gridY < rows)
    {
        // ... ZVYŠOK TVOJHO KÓDU PRE KLIKANIE (KONTROLA STENY, PRIDANIE BODU) ...
        // (Skopíruj si sem vnútro podmienky z tvojho pôvodného kódu)

        // Priklad pre istotu:
        int index = _robot->getAmclMap().index(gridX, gridY);
        unsigned int mapValue = _robot->getAmclMap().distanceField[index];
        if (mapValue >= 1 && mapValue <= 4) return;

        PointType clickedType;
        if (event->button() == Qt::LeftButton) clickedType = POINT_BLUE;
        else if (event->button() == Qt::RightButton) clickedType = POINT_PURPLE;
        else return;

        // ... Logika pridania do vectora points ...
        bool found = false;
        for (auto it = points.begin(); it != points.end(); ++it) {
            if (it->x == gridX && it->y == gridY) {
                if (it->type == clickedType) points.erase(it);
                else it->type = clickedType;
                found = true;
                break;
            }
        }
        if (!found) {
            MapPoint newPoint;
            newPoint.x = gridX;
            newPoint.y = gridY;
            newPoint.type = clickedType;
            points.push_back(newPoint);
        }
        emit pointsUpdated(points);
        update();
    }
}

void LidarVisualizer::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    // Vyplnime cele pozadie ciernou
    painter.fillRect(this->rect(), Qt::black);

    // --- Získame obdĺžnik pre mapu (82:66) ---
    QRect mapRect = getMapRect();

    // Zelený rámik okolo MAPY
    QPen pero;
    pero.setStyle(Qt::SolidLine);
    pero.setWidth(3);
    pero.setColor(Qt::green);
    painter.setPen(pero);
    painter.drawRect(mapRect.adjusted(0,0,-1,-1));

    if(!hasData && _robot == nullptr) return;

#ifndef DISABLE_AMCL
    if(_robot == nullptr) return;

    int rows = static_cast<int>(_robot->getAmclMap().height);
    int cols = static_cast<int>(_robot->getAmclMap().width);

    double cellWidth  = static_cast<double>(mapRect.width()) / cols;
    double cellHeight = static_cast<double>(mapRect.height()) / rows;

    // --- 1. Kreslenie Mapy ---
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            unsigned int val = _robot->getAmclMap().distanceField[_robot->getAmclMap().index(c,r)];
            if (val == 1) painter.setBrush(QColor(0, 200, 0));
            else if (val >= 2 && val <= 4) painter.setBrush(QColor(100, 100, 0));
            else painter.setBrush(Qt::black);

            painter.setPen(Qt::NoPen);
            QRectF cellRect(mapRect.x() + c * cellWidth, mapRect.y() + r * cellHeight, cellWidth, cellHeight);
            painter.drawRect(cellRect);
        }
    }

    // Kreslenie bodov
    for(const auto& p : points)
    {
        painter.setBrush((p.type == POINT_BLUE) ? Qt::blue : Qt::magenta);
        painter.setPen(Qt::white);
        QRectF cellRect(mapRect.x() + p.x * cellWidth, mapRect.y() + p.y * cellHeight, cellWidth, cellHeight);
        painter.drawRect(cellRect);
    }

    // --- 2. Kreslenie robota (Laser data) ---
    float robotXp = _robot->getBestParticle().x;
    float robotYp = _robot->getBestParticle().y;
    float robotThetaP = _robot->getBestParticle().theta;

    painter.setPen(QColor(200, 0, 0));
    painter.setBrush(QColor(0, 0, 0));
    for(int k=0; k<copyOfLaserData.numberOfScans; k++)
    {
        float angleRad = robotThetaP - (copyOfLaserData.Data[k].scanAngle) * 3.14159 / 180.0f;
        float lx = robotXp + copyOfLaserData.Data[k].scanDistance * std::cos(angleRad);
        float ly = robotYp + copyOfLaserData.Data[k].scanDistance * std::sin(angleRad);
        int gx, gy;
        _robot->getGridCoordinates(lx, ly, gx, gy);

        QRectF cellRect(mapRect.x() + gx * cellWidth, mapRect.y() + gy * cellHeight, cellWidth, cellHeight);
        painter.drawRect(cellRect);
    }

    // --- 3. Kreslenie samotneho ROBOTA ---

    // A) Ziskame grid suradnice stredu robota
    int robotGx, robotGy;
    _robot->getGridCoordinates(robotXp, robotYp, robotGx, robotGy);

    // Prepocitame na pixely obrazovky
    double rCenterX = mapRect.x() + robotGx * cellWidth + cellWidth/2.0;
    double rCenterY = mapRect.y() + robotGy * cellHeight + cellHeight/2.0;

    QPoint center(static_cast<int>(rCenterX), static_cast<int>(rCenterY));

    // B) Nastavenie Pera - Kruhy
    QPen robotPen(Qt::red);
    robotPen.setWidth(3);
    painter.setBrush(Qt::NoBrush);

    // 1. Červený kruh (14)
    robotPen.setColor(Qt::red);
    painter.setPen(robotPen);
    painter.drawEllipse(center, 14, 14);

    // 2. Sivý kruh (17)
    robotPen.setColor(Qt::gray);
    painter.setPen(robotPen);
    painter.drawEllipse(center, 17, 17);

    // 3. Biely kruh vonkajší (21)
    robotPen.setColor(Qt::white);
    painter.setPen(robotPen);
    painter.drawEllipse(center, 21, 21);

    // 4. Biely kruh vnútorný (10)
    painter.drawEllipse(center, 10, 10);

    // 5. Čiara smeru (nos) - Plynulá verzia
    // Nepoužívame mriežku, ale priamu trigonometriu na pixeloch,
    // aby nos "netancoval" pri otáčaní.

    double noseLength = 25.0; // Dĺžka nosa v pixeloch

    // Výpočet koncového bodu čiary
    // X = stred + dĺžka * cos(uhol)
    // Y = stred - dĺžka * sin(uhol)  <-- MÍNUS, lebo Y os na obrazovke ide smerom dole!

    double nX = center.x() + noseLength * std::cos(robotThetaP);
    double nY = center.y() - noseLength * std::sin(robotThetaP);

    // Nastavíme bielu farbu pre nos (lebo predchádzajúci kruh bol biely)
    QPen nosePen(Qt::white);
    nosePen.setWidth(2);
    painter.setPen(nosePen);

    // Nakreslíme čiaru zo stredu robota
    painter.drawLine(center, QPointF(nX, nY));

#endif
}
