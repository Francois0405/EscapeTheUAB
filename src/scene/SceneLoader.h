#ifndef SCENELOADER_H
#define SCENELOADER_H

#include <string>
#include <vector>
#include <filesystem>
#include <glm/glm.hpp>
#include "src/physics/MundoFisico.h"   // solo para fisica::TipoCuerpo (no arrastra Bullet)

// Configuración física OPCIONAL de un objeto (campo "physics" del JSON).
// Los tamaños se expresan en el espacio del MODELO, antes de aplicar la escala del objeto:
//   mundo.mitadTamano = |escala| * mitadTamano      (la escala se aplica UNA sola vez)
//   mundo.centro      = posicion + R * (escala * desplazamiento)
//   mundo.orientacion = R (la rotación del objeto)
struct SceneFisica
{
    bool presente = false;                                  // false = el objeto no tiene físicas
    fisica::TipoCuerpo tipo = fisica::TipoCuerpo::Estatico; // static, dynamic o bouncy
    fisica::FormaCuerpo forma = fisica::FormaCuerpo::Caja;
    glm::vec3 mitadTamano = glm::vec3(0.5f);                // "halfSize": medias dimensiones; radio para esfera
    glm::vec3 desplazamiento = glm::vec3(0.0f);             // "offset": centro de la caja respecto al origen del modelo
    float masa = 0.0f;                                      // "mass" en kg (solo dinámicos)
};

// Fuente de luz ligada a la transformacion de una instancia OBJ.
// El motor limita estas fuentes a los slots disponibles del shader Phong.
struct SceneLuz
{
    bool presente = false;
    bool encendida = true;
    glm::vec3 desplazamiento = glm::vec3(0.0f);
    glm::vec3 color = glm::vec3(0.82f, 0.94f, 1.0f);
    float intensidad = 1.0f;
	glm::vec2 areaSize = glm::vec2(0.0f); // dimensiones completas locales; cero = foco puntual
};

// Sobrescritura de material por instancia. tileSize expresa metros por repeticion
// de textura en los ejes del plano; roughness se traduce a shininess Phong.
struct SceneMaterial
{
    bool presente = false;
    std::string textura; // relativa al JSON de escena
    glm::vec2 tileSize = glm::vec2(1.0f);
    float roughness = 0.85f;
    // Máscara procedural determinista en coordenadas de mundo. strength=0 conserva escenas antiguas.
    float dirtStrength = 0.0f;
    float dirtScale = 0.45f; // frecuencia en celdas por metro
    float dirtSeed = 0.0f;
};

// Apertura rectangular en una pared paramétrica. centerU y base se expresan en metros
// desde el centro de pared y el pie inferior, respectivamente.
struct SceneAperturaPared
{
    std::string idPuerta;
    float centerU = 0.0f;
    float ancho = 0.9f;
    float alto = 2.1f;
    float base = 0.0f;
	std::string id;
};

struct ScenePared
{
    bool presente = false;
    std::vector<SceneAperturaPared> aperturas;
};

struct SceneSeccionPared
{
    glm::vec3 centroLocal{ 0.0f };
    glm::vec3 mitad{ 0.0f };
};

// Particiona una caja de pared en paños laterales, dinteles y posibles zócalos.
// Rechaza aperturas solapadas, demasiado próximas a extremos o fuera de altura.
bool GenerarSeccionesPared(const glm::vec3& mitadPared, const std::vector<SceneAperturaPared>& aperturas,
    std::vector<SceneSeccionPared>& secciones, std::string& error);

struct SceneObject
{
    std::string id;
    std::string categoria;
    std::string modelo;

    glm::vec3 posicion;
    glm::vec3 rotacion;
    glm::vec3 escala;

    SceneFisica fisica;   // opcional: sin "physics" en el JSON queda con presente = false
    SceneLuz luz;         // opcional: fuente Phong atada al objeto, sin colision extra
    SceneMaterial material; // opcional: material configurable por instancia
    ScenePared pared;       // opcional: definición lógica con aperturas, no altera el OBJ compartido
    bool permitirPenetracionArquitectura = false; // excepción deliberada, desactivada por defecto
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
    bool permitirPenetracionArquitectura = false; // excepción deliberada, desactivada por defecto
    std::string paredId;
    std::string aperturaId;
};

// Punto inicial opcional de la escena. position son los pies del jugador en metros;
// lookDirection es un vector del mundo y no contiene la altura de ojos.
struct SceneSpawn
{
    bool presente = false;
    glm::vec3 pies{ 0.0f };
    glm::vec3 direccion{ 1.0f, 0.0f, 0.0f };
};

// Todo lo que describe un archivo de escena.
struct DatosEscena
{
    std::string nombre;
    SceneSpawn spawn;
    float indirectIntensity = 0.16f; // aproximación hemisférica basada en luminarias visibles
    float indirectAmbientBase = 0.025f;
    float directSceneLightScale = 1.0f;
    glm::vec3 indirectFloorColor{ 0.42f, 0.46f, 0.43f };
    glm::vec3 indirectSkyColor{ 0.24f, 0.28f, 0.34f };
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
