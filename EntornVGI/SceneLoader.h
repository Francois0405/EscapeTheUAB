#ifndef SCENELOADER_H
#define SCENELOADER_H

#include <string>
#include <vector>
#include <filesystem>
#include <glm/glm.hpp>

struct SceneObject
{
    std::string id;
    std::string categoria;
    std::string modelo;

    glm::vec3 posicion;
    glm::vec3 rotacion;
    glm::vec3 escala;
};

// JSON es estricto: si hay un error no publica objetos parciales.
bool CargarEscenaJSON(
    const std::filesystem::path& ruta,
    std::vector<SceneObject>& objetos,
    std::string& error,
    std::string& nombre
);

#endif
