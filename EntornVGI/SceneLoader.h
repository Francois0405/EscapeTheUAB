#ifndef SCENELOADER_H
#define SCENELOADER_H

#include <string>
#include <vector>
#include <filesystem>
#include <glm/glm.hpp>

struct SceneObject
{
    std::string modelo;

    glm::vec3 posicion;
    glm::vec3 rotacion;
    glm::vec3 escala;
};

std::vector<SceneObject> CargarEscena(
    const std::filesystem::path& ruta
);

#endif