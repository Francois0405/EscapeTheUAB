#ifndef SCENELOADER_H
#define SCENELOADER_H

#include <string>
#include <vector>
#include <filesystem>
#include <glm/glm.hpp>
#include "MundoFisico.h"   // solo para fisica::TipoCuerpo (no arrastra Bullet)

// Configuración física OPCIONAL de un objeto (campo "physics" del JSON).
// Los tamaños se expresan en el espacio del MODELO, antes de aplicar la escala del objeto:
//   mundo.mitadTamano = |escala| * mitadTamano      (la escala se aplica UNA sola vez)
//   mundo.centro      = posicion + R * (escala * desplazamiento)
//   mundo.orientacion = R (la rotación del objeto)
struct SceneFisica
{
    bool presente = false;                                  // false = el objeto no tiene físicas
    fisica::TipoCuerpo tipo = fisica::TipoCuerpo::Estatico; // "static" o "dynamic"
    glm::vec3 mitadTamano = glm::vec3(0.5f);                // "halfSize": medias dimensiones de la caja
    glm::vec3 desplazamiento = glm::vec3(0.0f);             // "offset": centro de la caja respecto al origen del modelo
    float masa = 0.0f;                                      // "mass" en kg (solo dinámicos)
};

struct SceneObject
{
    std::string id;
    std::string categoria;
    std::string modelo;

    glm::vec3 posicion;
    glm::vec3 rotacion;
    glm::vec3 escala;

    SceneFisica fisica;   // opcional: sin "physics" en el JSON queda con presente = false
};

// Caja de colisión de UN nodo de un modelo animado (la hoja de la puerta, su marco...).
// Se expresa en el espacio local del nodo del GLB (el de glTF, con Y hacia arriba).
struct SceneColisionador
{
    std::string nodo;                                       // nombre del nodo en el GLB, p. ej. "Door"
    fisica::TipoCuerpo tipo = fisica::TipoCuerpo::Estatico; // "static" (marco) o "kinematic" (hoja que se mueve)
    glm::vec3 mitadTamano = glm::vec3(0.5f);
    glm::vec3 desplazamiento = glm::vec3(0.0f);
};

// Instancia de un modelo animado GLB declarada en la lista opcional "animated".
struct SceneObjetoAnimado
{
    std::string id;
    std::string modelo;                  // ruta del .glb relativa al JSON
    glm::vec3 posicion = glm::vec3(0.0f);
    float giroZGrados = 0.0f;            // de momento los modelos animados solo giran sobre el eje vertical
    float escala = 1.0f;                 // escala uniforme
    std::string animacion;               // nombre de la animación; vacío = la primera del archivo
    std::vector<SceneColisionador> colisionadores;
};

// Todo lo que describe un archivo de escena.
struct DatosEscena
{
    std::string nombre;
    std::vector<SceneObject> objetos;            // arquitectura + objetos OBJ
    std::vector<SceneObjetoAnimado> animados;    // objetos GLB animados (lista opcional)
};

// Lee la escena completa. Es estricto: si hay un error devuelve false, deja 'datos' como estaba
// y 'error' explica qué campo falla.
bool CargarDatosEscenaJSON(const std::filesystem::path& ruta, DatosEscena& datos, std::string& error);

// Versión antigua (solo objetos OBJ): sigue funcionando igual. Valida también los objetos animados.
bool CargarEscenaJSON(
    const std::filesystem::path& ruta,
    std::vector<SceneObject>& objetos,
    std::string& error,
    std::string& nombre
);

#endif
