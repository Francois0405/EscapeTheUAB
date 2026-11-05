#include "SceneLoader.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    int failures = 0;

    void comprobar(bool condicion, const char* caso)
    {
        if (!condicion)
        {
            std::cerr << "FALLO: " << caso << '\n';
            ++failures;
        }
    }

    void escribir(const fs::path& ruta, const std::string& contenido)
    {
        fs::create_directories(ruta.parent_path());
        std::ofstream archivo(ruta, std::ios::binary);
        archivo << contenido;
    }

    std::string escena(const std::string& objects)
    {
        return "{\"version\":1,\"name\":\"test\",\"architecture\":[],\"objects\":" + objects + "}";
    }

    bool cargar(const fs::path& ruta, std::vector<SceneObject>& objetos,
        std::string& error, std::string& nombre)
    {
        return CargarEscenaJSON(ruta, objetos, error, nombre);
    }
}

int main()
{
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path temp = fs::temp_directory_path() / ("EscapeTheUAB-SceneLoaderTests-" + std::to_string(unique));
    fs::create_directories(temp / "models");
    escribir(temp / "models" / "cube.obj", "# OBJ fixture\n");

    std::vector<SceneObject> objetos;
    std::string error;
    std::string nombre;

    const fs::path habitacion = fs::path(ESCAPETHEUAB_SOURCE_DIR) /
        "EntornVGI/assets/scenes/habitacion_beta.json";
    comprobar(cargar(habitacion, objetos, error, nombre), "habitacion_beta.json debe cargar");
    comprobar(objetos.size() == 12, "habitacion_beta debe contener 12 instancias");
    comprobar(nombre == "habitacion_beta", "nombre de habitacion_beta");

    escribir(temp / "malformed.json", "{no-json");
    comprobar(!cargar(temp / "malformed.json", objetos, error, nombre), "JSON malformado se rechaza");

    escribir(temp / "missing-field.json", "{\"version\":1,\"name\":\"test\",\"objects\":[]}");
    comprobar(!cargar(temp / "missing-field.json", objetos, error, nombre), "campo obligatorio ausente se rechaza");

    const std::string object = "{\"id\":\"same\",\"model\":\"models/cube.obj\",\"position\":[0,0,0],\"rotation\":[0,0,0],\"scale\":[1,1,1]}";
    escribir(temp / "duplicate-id.json", escena("[" + object + "," + object + "]"));
    comprobar(!cargar(temp / "duplicate-id.json", objetos, error, nombre), "IDs duplicados se rechazan");

    std::string missingModel = object;
    missingModel.replace(missingModel.find("models/cube.obj"), std::string("models/cube.obj").size(), "models/missing.obj");
    escribir(temp / "missing-model.json", escena("[" + missingModel + "]"));
    comprobar(!cargar(temp / "missing-model.json", objetos, error, nombre), "modelo inexistente se rechaza");

    std::string zeroScale = object;
    zeroScale.replace(zeroScale.find("[1,1,1]"), 7, "[0,1,1]");
    escribir(temp / "zero-scale.json", escena("[" + zeroScale + "]"));
    comprobar(!cargar(temp / "zero-scale.json", objetos, error, nombre), "escala cero se rechaza");

    escribir(temp / "relative-model.json", escena("[" + object + "]"));
    comprobar(cargar(temp / "relative-model.json", objetos, error, nombre), "ruta relativa de modelo valida");
    comprobar(objetos.size() == 1 && objetos[0].modelo == "models/cube.obj", "se conserva la ruta relativa");

    const auto objetosPrevios = objetos;
    const auto nombrePrevio = nombre;
    comprobar(!cargar(temp / "malformed.json", objetos, error, nombre), "escena invalida no se publica");
    comprobar(objetos.size() == objetosPrevios.size() && objetos[0].id == objetosPrevios[0].id && nombre == nombrePrevio,
        "fallo no reemplaza objetos ni nombre previos");

    std::error_code cleanupError;
    fs::remove_all(temp, cleanupError);
    if (cleanupError)
    {
        std::cerr << "No se pudo limpiar el directorio temporal de pruebas: " << cleanupError.message() << '\n';
        ++failures;
    }
    if (failures == 0) std::cout << "SceneLoaderTests: OK\n";
    return failures == 0 ? 0 : 1;
}
