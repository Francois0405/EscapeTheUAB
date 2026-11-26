#include "SlidingPuzzleObj.h"

#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <cmath>


SlidingPuzzleObj::SlidingPuzzleObj(std::vector<SceneObject>& objetosEscena, physics::PhysicsScene& escenaFisica)
    : m_objetosEscena(objetosEscena), m_escenaFisica(escenaFisica) {
    for (int i = 0; i < 16; i++) {
        m_indicesObjetos[i] = -1;
    }
}


// ------------------------------------------------------------
// INICIAR
// ------------------------------------------------------------

bool SlidingPuzzleObj::iniciar()
{
    vaciar();
    m_puzzle.iniciar();
    if (!localizarTablero()) {
        std::cerr << "No se ha encontrado obj_tauler\n";
        return false;
    }

    if (!localizarPiezas()) {
        std::cerr << "No se han encontrado todas las piezas\n";
        return false;
    }

    // Colocar inicialmente las piezas según el estado lógico
    // del puzzle.
    for (int fila = 0; fila < 4; fila++) {
        for (int columna = 0; columna < 4; columna++) {
            int pieza = m_puzzle.getPieza(fila, columna);

            if (pieza == 0) {
                continue;
            }
            SceneObject* objeto = objetoDePieza(pieza);

            if (!objeto) {
                return false;
            }
            glm::vec3 posicionModelo = posicionCasilla(fila, columna);
            glm::vec3 centro = centroFisicoDePieza(*objeto, posicionModelo);
            int indiceObjeto = m_indicesObjetos[pieza];

            if (!m_escenaFisica.TeleportDynamicObject(indiceObjeto, centro)) {
                std::cerr << "No se ha podido colocar la pieza " << pieza << '\n';
                return false;
            }
        }
    }

    return true;
}


// ------------------------------------------------------------
// LOCALIZAR TABLERO
// ------------------------------------------------------------

bool SlidingPuzzleObj::localizarTablero()
{
    for (const SceneObject& objeto : m_objetosEscena) {
        if (objeto.id == "obj_tauler") {
            m_origen = objeto.position;

            glm::mat4 giro(1.0f);

            giro = glm::rotate(giro, glm::radians(objeto.eulerRotation.z), glm::vec3(0, 0, 1));
            giro = glm::rotate(giro, glm::radians(objeto.eulerRotation.y), glm::vec3(0, 1, 0) );
            giro = glm::rotate(giro, glm::radians(objeto.eulerRotation.x), glm::vec3(1, 0, 0));

            // Los ejes locales del tablero transformados
            // a coordenadas del mundo.
            m_ejeColumnas = glm::normalize(glm::vec3(giro * glm::vec4(1, 0, 0, 0)));
            m_ejeFilas = glm::normalize(glm::vec3(giro * glm::vec4(0, 0, 1, 0)));

            return true;
        }
    }

    return false;
}


// ------------------------------------------------------------
// LOCALIZAR PIEZAS
// ------------------------------------------------------------

bool SlidingPuzzleObj::localizarPiezas() {
    for (int pieza = 1; pieza <= 15; pieza++) {
        std::string id = "obj_pieza" + std::to_string(pieza);
        m_indicesObjetos[pieza] = -1;
        for (int i = 0; i < static_cast<int>(m_objetosEscena.size()); i++) {
            if (m_objetosEscena[i].id == id) {
                m_indicesObjetos[pieza] = i;
                break;
            }
        }

        if (m_indicesObjetos[pieza] == -1) {
            std::cerr << "No se ha encontrado " << id << '\n';
            return false;
        }
    }

    return true;
}


// ------------------------------------------------------------
// POSICIÓN DE UNA CASILLA
// ------------------------------------------------------------

glm::vec3 SlidingPuzzleObj::posicionCasilla(int fila, int columna) const {
    // Distribuir las cuatro casillas alrededor del centro.
    float desplazamientoColumnas = (columna - 1.5f) * m_separacion;

    float desplazamientoFilas = (fila - 1.5f) * m_separacion;

    return m_origen + (m_ejeColumnas * desplazamientoColumnas) + (m_ejeFilas * desplazamientoFilas);
}


// ------------------------------------------------------------
// CENTRO DEL COLLIDER
// ------------------------------------------------------------

glm::vec3 SlidingPuzzleObj::centroFisicoDePieza(const SceneObject& objeto, const glm::vec3& posicionModelo) const {
    glm::mat4 giro(1.0f);
    giro = glm::rotate( giro, glm::radians(objeto.eulerRotation.z), glm::vec3(0, 0, 1) );
    giro = glm::rotate( giro, glm::radians(objeto.eulerRotation.y), glm::vec3(0, 1, 0) );
    giro = glm::rotate( giro, glm::radians(objeto.eulerRotation.x), glm::vec3(1, 0, 0) );

    glm::vec3 desplazamiento = objeto.escala * objeto.fisica.desplazamiento;

    return posicionModelo + glm::vec3(giro * glm::vec4(desplazamiento, 0.0f));
}


// ------------------------------------------------------------
// INTENTAR MOVER UNA PIEZA
// ------------------------------------------------------------

bool SlidingPuzzleObj::intentarMover(int fila, int columna)
{
    // Comprobar si la pieza seleccionada puede moverse.
    if (!m_puzzle.puedeMover(fila, columna))
        return false;

    int pieza = m_puzzle.getPieza(fila, columna);

    if (pieza <= 0 || pieza > 15)
        return false;

    SceneObject* objeto = objetoDePieza(pieza);

    if (!objeto)
        return false;

    int indiceObjeto = m_indicesObjetos[pieza];

    // Calcular la posición de destino: la casilla vacía.
    int indiceHueco = m_puzzle.m_indiceHueco;

    int filaDestino = indiceHueco / 4;
    int columnaDestino = indiceHueco % 4;

    glm::vec3 posicionModelo = posicionCasilla(filaDestino, columnaDestino);

    // Convertir la posición del modelo al centro del collider.
    glm::vec3 nuevoCentro = centroFisicoDePieza(*objeto, posicionModelo);

    // Mover el cuerpo Bullet que ya existe.
    if (!m_escenaFisica.TeleportDynamicObject(indiceObjeto, nuevoCentro)) {
        std::cerr << "No se ha podido mover la pieza " << pieza << '\n';
        return false;
    }

    // Actualizar el tablero lógico.
    return m_puzzle.mover(fila, columna);
}

bool SlidingPuzzleObj::intentarMoverObjeto(int indiceObjeto) {
    // Buscar qué pieza corresponde al objeto seleccionado.
    for (int pieza = 1; pieza <= 15; pieza++) {
        if (m_indicesObjetos[pieza] != indiceObjeto)
            continue;

        // Buscar la posición actual de esa pieza en el tablero.
        for (int fila = 0; fila < 4; fila++) {
            for (int columna = 0; columna < 4; columna++) {
                if (m_puzzle.getPieza(fila, columna) == pieza) {
                    return intentarMover(fila, columna);
                }
            }
        }

        return false;
    }

    return false;
}

// ------------------------------------------------------------
// OBTENER OBJETO DE UNA PIEZA
// ------------------------------------------------------------

SceneObject* SlidingPuzzleObj::objetoDePieza(int numero) {
    if (numero < 1 || numero > 15)
        return nullptr;
    int indice = m_indicesObjetos[numero];

    if (indice < 0 || indice >= static_cast<int>(m_objetosEscena.size())) {
        return nullptr;
    }

    return &m_objetosEscena[indice];
}


// ------------------------------------------------------------
// COMPROBAR SI ESTÁ RESUELTO
// ------------------------------------------------------------

bool SlidingPuzzleObj::estaResuelto() const {
    return m_puzzle.estaResuelto();
}


// ------------------------------------------------------------
// LIMPIAR
// ------------------------------------------------------------

void SlidingPuzzleObj::vaciar() {
    for (int i = 0; i < 16; i++)
        m_indicesObjetos[i] = -1;
}