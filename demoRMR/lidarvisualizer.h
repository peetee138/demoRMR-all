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

    void setRobot(robot* robotPtr);
    void updateLidarData(const LaserMeasurement &data);

    // Funkcia na vratenie vsetkych bodov (ak by si ich chcel poslat robotovi)
    std::vector<MapPoint> getPoints() const { return points; }

    // Funkcia na vycistenie vsetkych bodov
    void clearPoints() { points.clear(); update(); }

signals:
    void pointsUpdated(const std::vector<MapPoint> &points);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    robot* _robot;
    LaserMeasurement copyOfLaserData;
    bool hasData;

    // ZOZNAM BODOV namiesto jedneho bodu
    std::vector<MapPoint> points;
};

#endif // LIDARVISUALIZER_H
