#include "batteryindicator.h"

BatteryIndicator::BatteryIndicator(QWidget *parent) : QWidget(parent)
{
    m_level = 50.0; // Štartovacia hodnota
    // Nastavíme minimálnu veľkosť, aby to bolo vidno
    setMinimumSize(50, 80);
}

void BatteryIndicator::setBatteryLevel(double level)
{
    m_level = level;
    // Orezanie hodnôt
    if(m_level > 100) m_level = 100;
    if(m_level < 0) m_level = 0;

    update(); // Vynúti prekreslenie
}

void BatteryIndicator::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Rozmery widgetu
    int w = width();
    int h = height();

    // Okraje
    int margin = 5;

    // 1. Kreslenie "čiapočky" batérie (plusový pól)
    int tipWidth = w / 3;
    int tipHeight = 10;
    QRect tipRect((w - tipWidth)/2, margin, tipWidth, tipHeight);

    painter.setBrush(Qt::black);
    painter.setPen(Qt::black);
    painter.drawRect(tipRect);

    // 2. Kreslenie tela batérie (obrys)
    QRect bodyRect(margin, margin + tipHeight, w - 2*margin, h - 2*margin - tipHeight);

    painter.setBrush(Qt::NoBrush); // Priehľadné vnútro
    QPen pen(Qt::black);
    pen.setWidth(3);
    painter.setPen(pen);
    painter.drawRect(bodyRect);

    // 3. Kreslenie výplne (podľa percent)
    // Vyberieme farbu
    QColor fillColor;
    if (m_level > 50) fillColor = QColor(0,160,0);
    else if (m_level > 20) fillColor = QColor("orange");
    else fillColor = Qt::red;

    // Výpočet výšky výplne (bodyRect.height() je 100%)
    // Odpočítame padding 4px aby to neprekrývalo hrubý obrys
    int padding = 4;
    int maxFillHeight = bodyRect.height() - 2*padding;
    int currentFillHeight = (int)(maxFillHeight * (m_level / 100.0));

    // Kreslíme odspodu
    QRect fillRect(
        bodyRect.left() + padding,
        bodyRect.bottom() - padding - currentFillHeight,
        bodyRect.width() - 2*padding,
        currentFillHeight
        );

    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);
    painter.drawRect(fillRect);

    // 4. (Voliteľné) Text s percentami
    painter.setPen(Qt::black);
    painter.drawText(bodyRect, Qt::AlignCenter, QString::number((int)m_level) + "%");
}
