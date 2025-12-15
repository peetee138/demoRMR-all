#ifndef BATTERYINDICATOR_H
#define BATTERYINDICATOR_H

#include <QWidget>
#include <QPainter>

class BatteryIndicator : public QWidget
{
    Q_OBJECT
public:
    explicit BatteryIndicator(QWidget *parent = nullptr);

public slots:
    void setBatteryLevel(double level);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double m_level; // 0 až 100
};

#endif // BATTERYINDICATOR_H
