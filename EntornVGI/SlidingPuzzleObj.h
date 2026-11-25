#pragma once
#include "MundoFisico.h"
#include "SlidingPuzzle.h"

class SlidingPuzzleObj {
public:
    SlidingPuzzleObj(fisica::MundoFisico& mundo);

    bool iniciar();

    bool interactuar();
    bool seleccionarPieza(int fila, int columna);
    // void actualizar();
    bool intentarMover(int fila, int columna);
    bool estaResuelto() const;


    fisica::PoseCuerpo poseDePieza(int numero) const;
    void vaciar();

private:
    fisica::MundoFisico& m_mundo;
    SlidingPuzzle m_puzzle;
    // La posición 0 no se utiliza porque es el hueco.
    fisica::IdCuerpo cuerpos[16];
    // Posición y orientación del tablero en el mundo.
    glm::vec3 m_origen{0.0f, 0.0f, -3.0f};
    glm::vec3 m_ejeColumnas{1.0f, 0.0f, 0.0f};
    glm::vec3 m_ejeFilas{0.0f, 1.0f, 0.0f};
    glm::quat m_rotacion{1.0f, 0.0f, 0.0f, 0.0f};
    float m_separacion = 0.50f;

    glm::vec3 posicionCasilla(int fila, int columna) const;
    bool crearCuerpos();
};