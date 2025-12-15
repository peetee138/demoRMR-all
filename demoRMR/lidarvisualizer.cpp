#include "lidarvisualizer.h"
#include <math.h>

LidarVisualizer::LidarVisualizer(QWidget *parent) : QWidget(parent)
{
    _robot = nullptr;
    hasData = false;
    drawPath = false;
    pozorStena = false;
    lastCollisionState = false; // Inicializácia
    // Zoznam sa inicializuje automaticky prazdny
    m_highlightWalls = false;

    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    this->setAutoFillBackground(true);
    this->setPalette(pal);
}

void LidarVisualizer::toggleWallHighlight(bool enable)
{
    m_highlightWalls = enable;
    update(); // Vynúti prekreslenie
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
        // Priklad pre istotu:
        int index = _robot->getAmclMap().index(gridX, gridY);
        unsigned int mapValue = _robot->getAmclMap().distanceField[index];

        if (mapValue >= 1 && mapValue <= 3) {
            qDebug() << "Klik do zakazanej zony!";
            emit forbiddenZoneClicked(); // Vysleme signal
            return; // A skoncime, nepridavame bod
        }

        PointType clickedType;
        if (event->button() == Qt::LeftButton) clickedType = POINT_BLUE;
        else if (event->button() == Qt::RightButton) clickedType = POINT_PURPLE;
        else return;


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

            // Logika farieb
            if (val == 1) {
                // Stena
                if(m_highlightWalls) painter.setBrush(Qt::yellow);
                else painter.setBrush(QColor(255, 255, 255));
            }
            else if (val >= 2 && val <= 3) {
                // Inflacna zona (okolo steny)
                if(m_highlightWalls) painter.setBrush(QColor(255, 255, 0)); // Tmavsia zlta
                else painter.setBrush(QColor(0, 0, 0));
            }
            else {
                // Volny priestor
                painter.setBrush(Qt::black);
            }

            painter.setPen(Qt::NoPen);
            QRectF cellRect(mapRect.x() + c * cellWidth, mapRect.y() + r * cellHeight, cellWidth, cellHeight);
            painter.drawRect(cellRect);
        }
    }
    // --- 2. Kreslenie bodov (UPRAVENÉ: Zelené pre prejdené, Modré/Fialové pre budúce) ---
    for(size_t i = 0; i < points.size(); ++i)
    {
        const auto& p = points[i];

        // Rozhodovanie o farbe podľa indexu
        if (static_cast<int>(i) < m_currentIndex) {
            // Bod už bol dosiahnutý -> ZELENÁ
            painter.setBrush(Qt::green);
        } else {
            // Bod ešte nebol dosiahnutý -> MODRÁ (Waypoint) alebo FIALOVÁ (Task)
            painter.setBrush((p.type == POINT_BLUE) ? Qt::blue : Qt::magenta);
        }

        painter.setPen(Qt::white);
        QRectF cellRect(mapRect.x() + p.x * cellWidth, mapRect.y() + p.y * cellHeight, cellWidth, cellHeight);
        painter.drawRect(cellRect);
    }

    // --- 3. KRESLENIE TRASY (UPRAVENÉ: Dynamické prepojenie) ---
    if (drawPath) {

        // Na začiatku povieme, že všetko je OK.
        pozorStena = false;

        // Pripravíme si perá
        QPen okPen(Qt::yellow);       okPen.setWidth(2);
        QPen robotPenLine(Qt::green); robotPenLine.setWidth(2);
        QPen badPen(Qt::red);         badPen.setWidth(2);

        // A) Čiara od ROBOTA k AKTUÁLNEMU bodu
        // Skontrolujeme, či sme už neprešli všetky body (či index nie je mimo rozsahu)
        if (m_currentIndex < static_cast<int>(points.size())) {

            float rx = _robot->getBestParticle().x;
            float ry = _robot->getBestParticle().y;
            int rgx, rgy;
            _robot->getGridCoordinates(rx, ry, rgx, rgy);

            // Cieľom je bod na aktuálnom indexe
            MapPoint targetP = points[m_currentIndex];

            // 1. Skontrolujeme kolíziu
            bool crash = checkLineCollision(rgx, rgy, targetP.x, targetP.y);

            // 2. Nastavíme farbu a premennú
            if (crash) {
                painter.setPen(badPen);
                pozorStena = true;
            } else {
                painter.setPen(robotPenLine);
            }

            // 3. Vykreslíme
            double rScreenX = mapRect.x() + rgx * cellWidth + cellWidth / 2.0;
            double rScreenY = mapRect.y() + rgy * cellHeight + cellHeight / 2.0;
            double tx = mapRect.x() + targetP.x * cellWidth + cellWidth / 2.0;
            double ty = mapRect.y() + targetP.y * cellHeight + cellHeight / 2.0;

            painter.drawLine(QPointF(rScreenX, rScreenY), QPointF(tx, ty));
        }

        // B) Čiary medzi OSTATNÝMI BODMI (len tie, ktoré ešte neboli prejdené)
        if (points.size() > 1) {
            // Cyklus začína od m_currentIndex, aby sme nekreslili čiary medzi už prejdenými bodmi
            for (size_t i = m_currentIndex; i < points.size() - 1; ++i) {
                MapPoint p1 = points[i];
                MapPoint p2 = points[i+1];

                // 1. Skontrolujeme kolíziu
                bool crash = checkLineCollision(p1.x, p1.y, p2.x, p2.y);

                // 2. Nastavíme farbu a premennú
                if (crash) {
                    painter.setPen(badPen);
                    pozorStena = true;
                } else {
                    painter.setPen(okPen);
                }

                // 3. Vykreslíme
                double x1 = mapRect.x() + p1.x * cellWidth + cellWidth / 2.0;
                double y1 = mapRect.y() + p1.y * cellHeight + cellHeight / 2.0;
                double x2 = mapRect.x() + p2.x * cellWidth + cellWidth / 2.0;
                double y2 = mapRect.y() + p2.y * cellHeight + cellHeight / 2.0;

                painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
            }
        }
    }

    // --- 4. Kreslenie robota (Laser data) ---
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

    // --- 5. Kreslenie samotneho ROBOTA ---
    int robotGx, robotGy;
    _robot->getGridCoordinates(robotXp, robotYp, robotGx, robotGy);

    double rCenterX = mapRect.x() + robotGx * cellWidth + cellWidth/2.0;
    double rCenterY = mapRect.y() + robotGy * cellHeight + cellHeight/2.0;

    QPoint center(static_cast<int>(rCenterX), static_cast<int>(rCenterY));

    QPen robotPen(Qt::red);
    robotPen.setWidth(3);
    painter.setBrush(Qt::NoBrush);

    robotPen.setColor(Qt::red);
    painter.setPen(robotPen);
    painter.drawEllipse(center, 14, 14);

    robotPen.setColor(Qt::gray);
    painter.setPen(robotPen);
    painter.drawEllipse(center, 17, 17);

    robotPen.setColor(Qt::white);
    painter.setPen(robotPen);
    painter.drawEllipse(center, 21, 21);
    painter.drawEllipse(center, 10, 10);

    double noseLength = 25.0;
    double nX = center.x() + noseLength * std::cos(robotThetaP);
    double nY = center.y() - noseLength * std::sin(robotThetaP);

    QPen nosePen(Qt::white);
    nosePen.setWidth(2);
    painter.setPen(nosePen);
    painter.drawLine(center, QPointF(nX, nY));
#endif
    if (pozorStena == true && lastCollisionState == false) {
        // Pošleme signál do MainWindow
        emit collisionDetected();
    }

    // Uložíme si aktuálny stav pre ďalšie kolo
    lastCollisionState = pozorStena;
}
bool LidarVisualizer::checkLineCollision(int x1, int y1, int x2, int y2)
{
    if(!_robot) return false;

    // Rozmery mapy
    int mapW = _robot->getAmclMap().width;
    int mapH = _robot->getAmclMap().height;

    // Bresenhamov algoritmus
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    int cx = x1;
    int cy = y1;

    while (true) {
        // --- OPRAVA: Ignorujeme štartovací bod ---
        // Ak stojíme na mieste, ktoré je "trochu" nebezpečné (hodnota 2),
        // nechceme, aby to hneď vyhlásilo chybu. Chceme vedieť, či NARAŹÍME do steny cestou.
        if (cx == x1 && cy == y1) {
            // Nerob nic, len chod dalej
        }
        else {
            // Kontrola hraníc
            if (cx >= 0 && cx < mapW && cy >= 0 && cy < mapH) {
                int index = _robot->getAmclMap().index(cx, cy);
                // Pretypovanie na int pre istotu
                int val = (int)_robot->getAmclMap().distanceField[index];

                // Ak je to stena alebo inflačná zóna (>= 2)
                if (val >= 1 && val < 3) {
                    qDebug() << "KOLIZIA na [" << cx << "," << cy << "] hodnota:" << val;
                    return true;
                }
            }
        }

        if (cx == x2 && cy == y2) break;

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; cx += sx; }
        if (e2 < dx) { err += dx; cy += sy; }
    }
    return false;
}

