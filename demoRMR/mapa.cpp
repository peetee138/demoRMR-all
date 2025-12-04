#include "mapa.h"
#include <queue>
#include <QDebug> // Ak chceme vypisovat debug info

std::vector<int> Mapa::spracujVonkajsok(const GridMap &amclMap)
{
    int rows = static_cast<int>(amclMap.height);
    int cols = static_cast<int>(amclMap.width);
    int size = rows * cols;

    // Pripravíme prázdnu masku (samé nuly)
    std::vector<int> maska(size, 0);

    // Ak je mapa prázdna, vrátime prázdnu masku
    if (rows == 0 || cols == 0) return maska;

    // Fronta pre Flood Fill
    std::queue<int> q;

    // 1. Pridáme okraje do fronty a označíme ako 5
    for (int x = 0; x < cols; x++) {
        // Horny okraj
        q.push(0 * cols + x);
        maska[0 * cols + x] = 5;

        // Dolny okraj
        q.push((rows - 1) * cols + x);
        maska[(rows - 1) * cols + x] = 5;
    }
    for (int y = 0; y < rows; y++) {
        // Lavy okraj
        q.push(y * cols + 0);
        maska[y * cols + 0] = 5;

        // Pravy okraj
        q.push(y * cols + (cols - 1));
        maska[y * cols + (cols - 1)] = 5;
    }

    // 2. Spustíme vylievanie
    while(!q.empty())
    {
        int curr = q.front();
        q.pop();

        int cx = curr % cols;
        int cy = curr / cols;

        int neighborsX[] = {0, 0, -1, 1};
        int neighborsY[] = {-1, 1, 0, 0};

        for(int i=0; i<4; i++)
        {
            int nx = cx + neighborsX[i];
            int ny = cy + neighborsY[i];

            if(nx >= 0 && nx < cols && ny >= 0 && ny < rows)
            {
                int nIdx = ny * cols + nx;
                int mapVal = static_cast<int>(amclMap.distanceField[nIdx]);

                // Ak to nie je stena (mapVal == 0) a ešte to nie je označené (maska == 0)
                if(maska[nIdx] == 0 && mapVal == 0)
                {
                    maska[nIdx] = 5; // Označíme ako vonkajšok
                    q.push(nIdx);
                }
            }
        }
    }

    qDebug() << "Mapa bola úspešne spracovaná v súbore mapa.cpp";
    return maska;
}
