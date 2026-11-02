#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <vector>
#include <unordered_set>
#include <cmath>
#include <limits>
#include <Windows.h>
#include "SceneLoader.h"
#include "third_party/nlohmann/json.hpp"

using nlohmann::json;

namespace
{
    bool leerVector3(const json& valor, glm::vec3& destino, const std::string& campo, std::string& error)
    {
        if (!valor.is_array() || valor.size() != 3)
        {
            error = "El campo '" + campo + "' debe contener exactamente tres numeros.";
            return false;
        }

        for (std::size_t i = 0; i < 3; ++i)
        {
            if (!valor[i].is_number())
            {
                error = "El campo '" + campo + "' contiene un valor que no es numerico.";
                return false;
            }
            const double numero = valor[i].get<double>();
            if (!std::isfinite(numero) || std::abs(numero) > std::numeric_limits<float>::max())
            {
                error = "El campo '" + campo + "' contiene un valor no finito o fuera de rango.";
                return false;
            }
            destino[static_cast<glm::length_t>(i)] = static_cast<float>(numero);
        }
        return true;
    }
}

// Lee un archivo de escena y comprueba sus objetos.
// Cada entrada contiene el modelo y sus transformaciones.
std::vector<SceneObject> CargarEscena(const std::filesystem::path& ruta)
{
    std::ifstream archivo(ruta);

    if (!archivo.is_open())
    {
        OutputDebugStringA("No se ha podido abrir el archivo de escena.\n");
        return {};
    }

    OutputDebugStringA("Archivo de escena abierto correctamente.\n");

    // Leemos la ruta y las transformaciones de cada objeto.
    std::string linea;

    std::vector<SceneObject> objetos;
    bool datosInvalidos = false;

    while (std::getline(archivo, linea))
    {
        // Leemos los datos de cada objeto de la escena.

        // Posición del objeto.
        float x, y, z;

        // Rotación en grados.
        float rx, ry, rz;

        // Escala del objeto.
        float sx, sy, sz;

        std::istringstream datos(linea);
        std::string modelo;

        // Saltar líneas vacías y comentarios.
        if (!(datos >> modelo) || modelo[0] == '#')
            continue;

        if (!(datos >> x >> y >> z
            >> rx >> ry >> rz
            >> sx >> sy >> sz))
        {
            OutputDebugStringA("Error: faltan datos o hay valores incorrectos.\n");
            datosInvalidos = true;
            continue;
        }

        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
            !std::isfinite(rx) || !std::isfinite(ry) || !std::isfinite(rz) ||
            !std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(sz))
        {
            OutputDebugStringA("Error: la fila TXT contiene valores no finitos.\n");
            datosInvalidos = true;
            continue;
        }

        // Guardamos el modelo y sus transformaciones.
        objetos.push_back({
            "txt-" + std::to_string(objetos.size() + 1),
            "object",
            modelo,
            {x, y, z},
            {rx, ry, rz},
            {sx, sy, sz}
            });

        // Mostrar el modelo detectado.
        std::ostringstream mensaje;

        mensaje << "Modelo: " << modelo
            << " | Posicion: " << x << ", " << y << ", " << z
            << " | Rotacion: " << rx << ", " << ry << ", " << rz
            << " | Escala: " << sx << ", " << sy << ", " << sz
            << "\n";

        OutputDebugStringA(mensaje.str().c_str());
    }
    // Un TXT parcialmente mal formado no debe reemplazar la escena en uso.
    return datosInvalidos ? std::vector<SceneObject>{} : objetos;
}

bool CargarEscenaJSON(const std::filesystem::path& ruta,
    std::vector<SceneObject>& objetos, std::string& error, std::string& nombre)
{
    try
    {
        std::ifstream archivo(ruta);
        if (!archivo.is_open())
        {
            error = "No se pudo abrir la escena JSON: " + ruta.string();
            return false;
        }

        const json documento = json::parse(archivo);
        if (!documento.is_object())
        {
            error = "La raiz del JSON debe ser un objeto.";
            return false;
        }
        if (!documento.contains("version") || !documento["version"].is_number_integer() || documento["version"] != 1)
        {
            error = "El campo obligatorio 'version' debe ser el entero 1.";
            return false;
        }
        if (!documento.contains("name") || !documento["name"].is_string() || documento["name"].get<std::string>().empty())
        {
            error = "El campo obligatorio 'name' debe ser un texto no vacio.";
            return false;
        }
        for (const char* coleccion : { "architecture", "objects" })
        {
            if (!documento.contains(coleccion) || !documento[coleccion].is_array())
            {
                error = std::string("El campo obligatorio '") + coleccion + "' debe ser una lista.";
                return false;
            }
        }

        std::vector<SceneObject> candidatos;
        std::unordered_set<std::string> ids;
        for (const char* coleccion : { "architecture", "objects" })
        {
            for (std::size_t indice = 0; indice < documento[coleccion].size(); ++indice)
            {
                const json& entrada = documento[coleccion][indice];
                const std::string contexto = std::string(coleccion) + "[" + std::to_string(indice) + "]";
                if (!entrada.is_object())
                {
                    error = contexto + " debe ser un objeto JSON.";
                    return false;
                }

                for (const char* campo : { "id", "model", "position", "rotation", "scale" })
                {
                    if (!entrada.contains(campo))
                    {
                        error = contexto + " no contiene el campo obligatorio '" + campo + "'.";
                        return false;
                    }
                }
                if (!entrada["id"].is_string() || entrada["id"].get<std::string>().empty() ||
                    !ids.insert(entrada["id"].get<std::string>()).second)
                {
                    error = contexto + " tiene un id vacio, incorrecto o duplicado.";
                    return false;
                }
                if (!entrada["model"].is_string() || entrada["model"].get<std::string>().empty())
                {
                    error = contexto + " debe tener una ruta OBJ no vacia en 'model'.";
                    return false;
                }

                SceneObject objeto{};
                objeto.id = entrada["id"].get<std::string>();
                objeto.categoria = coleccion;
                objeto.modelo = entrada["model"].get<std::string>();
                if (!leerVector3(entrada["position"], objeto.posicion, contexto + ".position", error) ||
                    !leerVector3(entrada["rotation"], objeto.rotacion, contexto + ".rotation", error) ||
                    !leerVector3(entrada["scale"], objeto.escala, contexto + ".scale", error))
                    return false;

                for (int eje = 0; eje < 3; ++eje)
                {
                    if (objeto.escala[eje] == 0.0f)
                    {
                        error = contexto + ".scale no puede contener ceros.";
                        return false;
                    }
                }
                const auto rutaModelo = (ruta.parent_path() / objeto.modelo).lexically_normal();
                if (!std::filesystem::exists(rutaModelo) || !std::filesystem::is_regular_file(rutaModelo))
                {
                    error = contexto + " apunta a un modelo inexistente: " + rutaModelo.string();
                    return false;
                }
                candidatos.push_back(std::move(objeto));
            }
        }

        if (candidatos.empty())
        {
            error = "La escena debe contener al menos un elemento.";
            return false;
        }

        nombre = documento["name"].get<std::string>();
        objetos = std::move(candidatos);
        error.clear();
        return true;
    }
    catch (const json::parse_error& excepcion)
    {
        error = std::string("JSON no valido: ") + excepcion.what();
    }
    catch (const json::exception& excepcion)
    {
        error = std::string("Error al leer campos JSON: ") + excepcion.what();
    }
    catch (const std::filesystem::filesystem_error& excepcion)
    {
        error = std::string("Error comprobando rutas de modelo: ") + excepcion.what();
    }
    return false;
}
