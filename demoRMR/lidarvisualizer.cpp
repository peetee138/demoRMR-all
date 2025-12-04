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

    // 2. Velkost bunky
    double cellWidth  = static_cast<double>(this->width()) / cols;
    double cellHeight = static_cast<double>(this->height()) / rows;

    // 3. Prepocet kliknutia na mriezku
    int gridX = static_cast<int>(event->x() / cellWidth);
    int gridY = static_cast<int>(event->y() / cellHeight);

    // 4. Overenie hranic
    if(gridX >= 0 && gridX < cols && gridY >= 0 && gridY < rows)
    {
        // --- NOVÁ ČASŤ: KONTROLA PREKÁŽKY ---

        // Získame index v poli mapy
        int index = _robot->getAmclMap().index(gridX, gridY);

        // Získame hodnotu z distanceField
        unsigned int mapValue = _robot->getAmclMap().distanceField[index];

        // Podmienka:
        // 1 = Zelená (Stena)
        // 2, 3, 4 = Žltá (Bezpečnostná zóna)
        if (mapValue >= 1 && mapValue <= 4)
        {
            qDebug() << "ZAKAZ: Klikol si na stenu alebo zltu zonu! Hodnota:" << mapValue;
            return; // Okamzite koncime funkciu, bod sa neprida
        }

        PointType clickedType;
        if (event->button() == Qt::LeftButton) {
            clickedType = POINT_BLUE;
        } else if (event->button() == Qt::RightButton) {
            clickedType = POINT_PURPLE;
        } else {
            return;
        }

        bool found = false;
        for (auto it = points.begin(); it != points.end(); ++it) {
            if (it->x == gridX && it->y == gridY) {
                if (it->type == clickedType) {
                    points.erase(it);
                } else {
                    it->type = clickedType;
                }
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

        qDebug() << "Pocet bodov:" << points.size();
        emit pointsUpdated(points);
        update();
    }
}

void LidarVisualizer::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    QRect rect = this->rect();

    // --- Zeleny ramik ---
    QPen pero;
    pero.setStyle(Qt::SolidLine);
    pero.setWidth(3);
    pero.setColor(Qt::green);
    painter.setPen(pero);
    painter.drawRect(rect.adjusted(0,0,-1,-1));

    if(!hasData && _robot == nullptr) return;

#ifndef DISABLE_AMCL
    if(_robot == nullptr) return;

    int rows = static_cast<int>(_robot->getAmclMap().height);
    int cols = static_cast<int>(_robot->getAmclMap().width);

    double cellWidth  = static_cast<double>(rect.width()) / cols;
    double cellHeight = static_cast<double>(rect.height()) / rows;

    // --- 1. Kreslenie Mapy ---
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            unsigned int val = _robot->getAmclMap().distanceField[_robot->getAmclMap().index(c,r)];
            if (val == 1) painter.setBrush(QColor(0, 200, 0));
            else if (val >= 2 && val <= 4) painter.setBrush(QColor(100, 100, 0));
            else painter.setBrush(Qt::black);

            painter.setPen(Qt::NoPen);
            painter.drawRect(QRectF(c * cellWidth, r * cellHeight, cellWidth, cellHeight));
        }
    }

    for(const auto& p : points)
    {
        painter.setBrush((p.type == POINT_BLUE) ? Qt::blue : Qt::magenta);
        painter.setPen(Qt::white);
        painter.drawRect(QRectF(p.x * cellWidth, p.y * cellHeight, cellWidth, cellHeight));
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

        QRectF cellRect(gx * cellWidth, gy * cellHeight, cellWidth, cellHeight);
        painter.drawRect(cellRect);
    }
#endif
}
