#include "src/gameplay/slidingPuzzle/slidingPuzzle.h"
#include <cstdlib>

bool SlidingPuzzle::puedeMover(int fila, int columna) const {
    if (fila < 0 || fila >= m_filas || columna < 0 || columna >= m_columnas)
        return false;

    int filaHueco = m_indiceHueco / m_columnas;
    int columnaHueco = m_indiceHueco % m_columnas;

    if ((fila == filaHueco && std::abs(columna - columnaHueco) == 1) ||
        (columna == columnaHueco && std::abs(fila - filaHueco) == 1)) {
        return true;
    }
    return false;
}

void SlidingPuzzle::iniciar() {
    m_filas = 4;
    m_columnas = 4;
    m_size = 16;
    m_indiceHueco = 9;
    int tablero[16] = {4, 6, 3, 5, 1, 7, 8, 10, 9, 0, 2, 12, 15, 11, 13, 14};
    for (int i = 0; i < m_size; i++) {
        m_tablero[i] = tablero[i];
        if (m_tablero[i] == 0) {
            m_indiceHueco = i;
        }
    }
}

bool SlidingPuzzle::mover(int fila, int columna) {
    if (!puedeMover(fila, columna))
        return false;
    int indice = fila * m_columnas + columna;
    m_tablero[m_indiceHueco] = m_tablero[indice];
    m_tablero[indice] = 0;
    m_indiceHueco = indice;
    return true;
}

bool SlidingPuzzle::estaResuelto() const {
    for (int i = 0; i < m_size - 1; i++) {
        if (m_tablero[i] != i + 1)
            return false;
    }
    if (m_tablero[15] != 0) {
        return false;
    }
    return true;
}

int SlidingPuzzle::getPieza(int fila, int columna) const {
    if (fila < 0 || fila >= m_filas || columna < 0 || columna >= m_columnas)
        return -1;
    return m_tablero[fila * m_columnas + columna];
}