#pragma once

class SlidingPuzzle {
public:
    int m_filas;
    int m_columnas;
    int m_tablero[16];
    int m_size;
    int m_indiceHueco;

    SlidingPuzzle(): m_filas(4), m_columnas(4), m_size(m_filas*m_columnas), m_indiceHueco(m_size-1) { 
        for (int i = 0; i < m_indiceHueco; i++) m_tablero[i] = i + 1; m_tablero[m_indiceHueco] = 0;} 

    void iniciar();
    bool puedeMover(int fila, int columna) const;
    bool mover(int fila, int columna);
    bool estaResuelto() const;
    int getPieza(int fila, int columna) const;
private:
};