#ifndef LIDARVISUALIZER_H
#define LIDARVISUALIZER_H

#include <QWidget>
#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <vector> // Potrebujeme pre zoznam bodov
#include "robot.h"

// Definujeme si typ bodu
enum PointType {
    POINT_BLUE,   // Lave tlacidlo
    POINT_PURPLE  // Prave tlacidlo
};

// Struktura pre jeden bod na mape
struct MapPoint {
    int x;
    int y;
    PointType type;
};

class LidarVisualizer : public QWidget
{
    Q_OBJECT
public:
    explicit LidarVisualizer(QWidget *parent = nullptr);
    void togglePathDrawing(bool enable) { drawPath = enable; update(); }
    void setCurrentIndex(int index);

    void setRobot(robot* robotPtr);
    void updateLidarData(const LaserMeasurement &data);

    void toggleWallHighlight(bool enable);

    // Funkcia na vratenie vsetkych bodov (ak by si ich chcel poslat robotovi)
    std::vector<MapPoint> getPoints() const { return points; }


    bool pozorStena;
    // Funkcia na vycistenie vsetkych bodov
    void clearPoints() { points.clear(); update(); }

signals:
    void pointsUpdated(const std::vector<MapPoint> &points);
    void collisionDetected();
    void forbiddenZoneClicked();
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    robot* _robot;
    LaserMeasurement copyOfLaserData;
    bool hasData;
    bool drawPath = false; // Premenná, či kresliť čiary (na začiatku false)
    bool checkLineCollision(int x1, int y1, int x2, int y2);
    bool lastCollisionState = false;
    // ZOZNAM BODOV namiesto jedneho bodu
    std::vector<MapPoint> points;
    int m_currentIndex = 0;
    bool m_highlightWalls = false; // Premenna pre stav zlteho zobrazenia
    // Pomocná funkcia na výpočet obdĺžnika s pomerom 82:66
    QRect getMapRect() const {
        if (width() == 0 || height() == 0) return QRect();

        double targetRatio = 82.0 / 66.0;
        double currentRatio = (double)width() / (double)height();

        int drawW, drawH;
        int offX, offY;

        if (currentRatio > targetRatio) {
            // Okno je príliš široké -> výška určuje veľkosť
            drawH = height();
            drawW = static_cast<int>(drawH * targetRatio);
            offY = 0;
            offX = (width() - drawW) / 2;
        } else {
            // Okno je príliš vysoké -> šírka určuje veľkosť
            drawW = width();
            drawH = static_cast<int>(drawW / targetRatio);
            offX = 0;
            offY = (height() - drawH) / 2;
        }
        return QRect(offX, offY, drawW, drawH);
    }
};

#endif // LIDARVISUALIZER_H
