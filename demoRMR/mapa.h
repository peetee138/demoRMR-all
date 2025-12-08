#ifndef MAPA_H
#define MAPA_H

#include <vector>
#include "robot.h" // Potrebujeme poznať štruktúru GridMap

class Mapa
{
public:
    // Statická funkcia - nepotrebujeme vytvárať inštanciu triedy Mapa,
    // stačí zavolať Mapa::spracuj(...)
    static std::vector<int> spracujVonkajsok(const GridMap& amclMap);
};

#endif // MAPA_H
