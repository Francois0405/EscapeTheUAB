#pragma once

#include "SlidingPuzzle.h"
#include "SceneLoader.h"
#include "PhysicsScene.h"

#include <glm/glm.hpp>
#include <vector>
#include <string>

class SlidingPuzzleObj {
public:

    SlidingPuzzleObj(std::vector<SceneObject>& objetosEscena, physics::PhysicsScene& EscenaFisica);
    bool iniciar();
    bool intentarMover(int fila, int columna);
    bool intentarMoverObjeto(int indiceObjeto);
    bool estaResuelto() const;
    SceneObject* objetoDePieza(int numero);
    glm::vec3 posicionCasilla(int fila, int columna) const;
    void vaciar();

private:
    SlidingPuzzle m_puzzle;
    std::vector<SceneObject>& m_objetosEscena;
    physics::PhysicsScene& m_escenaFisica;
    int m_indicesObjetos[16];
    // Transformación del tablero.
    glm::vec3 m_origen{0.0f};
    glm::vec3 m_ejeColumnas{1.0f, 0.0f, 0.0f};
    glm::vec3 m_ejeFilas{0.0f, 1.0f, 0.0f};
    float m_separacion = 0.51f;
    bool localizarPiezas();
    bool localizarTablero();
    glm::vec3 centroFisicoDePieza(const SceneObject& objeto, const glm::vec3& posicionModelo) const;
};