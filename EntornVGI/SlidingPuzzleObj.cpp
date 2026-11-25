#include "SlidingPuzzleObj.h"

SlidingPuzzleObj::SlidingPuzzleObj(fisica::MundoFisico& mundo) : m_mundo(mundo) {
    for (int i = 0; i < 16; i++) {
        cuerpos[i] = fisica::CuerpoInvalido;
    }
}

glm::vec3 SlidingPuzzleObj::posicionCasilla(int fila, int columna) const {
    float desplazamientoColumna = (columna - 1.5f) * m_separacion;
    float desplazamientoFila = (1.5f - fila) * m_separacion;
    return m_origen + (m_ejeColumnas * desplazamientoColumna) + (m_ejeFilas * desplazamientoFila);
}

bool SlidingPuzzleObj::iniciar() {
    vaciar();
    m_puzzle.iniciar();
    return crearCuerpos();
}

bool SlidingPuzzleObj::crearCuerpos() {
    const glm::vec3 mitadTamano(0.25f, 0.25f, 0.025f);

    for (int fila = 0; fila < 4; fila++) {
        for (int columna = 0; columna < 4; columna++) {
            int pieza = m_puzzle.getPieza(fila, columna);
            if (pieza == 0) {
                continue;
            }
            glm::vec3 posicion = posicionCasilla(fila, columna);

            cuerpos[pieza] = m_mundo.CrearCaja(fisica::TipoCuerpo::Cinematico, posicion, 
                m_rotacion, mitadTamano, 0.0f);
            if (cuerpos[pieza] == fisica::CuerpoInvalido) {
                vaciar();
                return false;
            }
        }
    }
    return true;
}

bool SlidingPuzzleObj::intentarMover(int fila, int columna) {
    if (!m_puzzle.puedeMover(fila, columna)) {
        return false;
    }
    int pieza = m_puzzle.getPieza(fila, columna);
    if (cuerpos[pieza] == fisica::CuerpoInvalido) {
        return false;
    }
    if (!m_puzzle.mover(fila, columna)) {
        return false;
    }
    // Busca nueva posición de la pieza
    int filaDestino = 0;
    int columnaDestino = 0;
    for (int fila = 0; fila < 4; fila++) {
        for (int col = 0; col < 4; col++) {
            if (m_puzzle.getPieza(fila, col) == pieza) {
                filaDestino = fila;
                columnaDestino = col;
            }
        }
    }
    m_mundo.MoverCinematico(cuerpos[pieza], posicionCasilla(filaDestino, columnaDestino), m_rotacion);
    return true;
}

bool SlidingPuzzleObj::estaResuelto() const {
    return m_puzzle.estaResuelto();
}

fisica::PoseCuerpo SlidingPuzzleObj::poseDePieza(int numero) const {
    if (cuerpos[numero] == fisica::CuerpoInvalido) {
        return {};
    }
    return m_mundo.PoseDe(cuerpos[numero]);
}

void SlidingPuzzleObj::vaciar() {
    for (fisica::IdCuerpo& cuerpo : cuerpos) {
        if (cuerpo != fisica::CuerpoInvalido) {
            m_mundo.EliminarCuerpo(cuerpo);
            cuerpo = fisica::CuerpoInvalido;
        }
    }
}