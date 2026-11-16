#include <fstream>
#include <string>
#include <filesystem>
#include <vector>
#include <unordered_set>
#include <cmath>
#include <initializer_list>
#include <limits>
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

    // Número (no vector) finito dentro de float.
    bool leerNumero(const json& valor, float& destino, const std::string& campo, std::string& error)
    {
        if (!valor.is_number() || !std::isfinite(valor.get<double>()) ||
            std::abs(valor.get<double>()) > std::numeric_limits<float>::max())
        {
            error = "El campo '" + campo + "' debe ser un numero finito.";
            return false;
        }
        destino = valor.get<float>();
        return true;
    }

    // Rechaza claves desconocidas: así un "halfsize" mal escrito no se ignora en silencio.
    bool soloClavesConocidas(const json& objeto, std::initializer_list<const char*> permitidas,
        const std::string& contexto, std::string& error)
    {
        for (auto it = objeto.begin(); it != objeto.end(); ++it)
        {
            bool conocida = false;
            for (const char* clave : permitidas) conocida = conocida || it.key() == clave;
            if (!conocida)
            {
                error = contexto + " contiene un campo desconocido: '" + it.key() + "'.";
                return false;
            }
        }
        return true;
    }

    bool leerTipo(const json& valor, bool permitirCinematico, fisica::TipoCuerpo& tipo,
        const std::string& contexto, std::string& error)
    {
        if (valor.is_string())
        {
            const std::string texto = valor.get<std::string>();
            if (texto == "static") { tipo = fisica::TipoCuerpo::Estatico; return true; }
            if (texto == "dynamic" && !permitirCinematico) { tipo = fisica::TipoCuerpo::Dinamico; return true; }
            if (texto == "kinematic" && permitirCinematico) { tipo = fisica::TipoCuerpo::Cinematico; return true; }
        }
        error = contexto + ".type debe ser " +
            (permitirCinematico ? "\"static\" o \"kinematic\"." : "\"static\" o \"dynamic\".");
        return false;
    }

    // Lee halfSize (obligatorio, > 0) y offset (opcional) de una caja.
    bool leerCaja(const json& entrada, glm::vec3& mitad, glm::vec3& desplazamiento,
        const std::string& contexto, std::string& error)
    {
        if (!entrada.contains("halfSize"))
        {
            error = contexto + " no contiene el campo obligatorio 'halfSize'.";
            return false;
        }
        if (!leerVector3(entrada["halfSize"], mitad, contexto + ".halfSize", error)) return false;
        for (int eje = 0; eje < 3; ++eje)
            if (!(mitad[eje] > 0.0f))
            {
                error = contexto + ".halfSize debe ser mayor que cero en los tres ejes.";
                return false;
            }
        desplazamiento = glm::vec3(0.0f);
        if (entrada.contains("offset") && !leerVector3(entrada["offset"], desplazamiento, contexto + ".offset", error))
            return false;
        return true;
    }

    // Campo opcional "physics" de un objeto OBJ.
    bool leerFisica(const json& entrada, SceneFisica& fisica, const std::string& contexto, std::string& error)
    {
        if (!entrada.contains("physics")) return true;   // sin físicas: comportamiento anterior
        const json& datos = entrada["physics"];
        const std::string ctx = contexto + ".physics";
        if (!datos.is_object())
        {
            error = ctx + " debe ser un objeto JSON.";
            return false;
        }
        if (!soloClavesConocidas(datos, { "type", "shape", "halfSize", "offset", "mass" }, ctx, error)) return false;
        if (!datos.contains("type"))
        {
            error = ctx + " no contiene el campo obligatorio 'type'.";
            return false;
        }
        if (!leerTipo(datos["type"], false, fisica.tipo, ctx, error)) return false;
        if (datos.contains("shape") && !(datos["shape"].is_string() && datos["shape"] == "box"))
        {
            error = ctx + ".shape solo admite \"box\" en esta version.";
            return false;
        }
        if (!leerCaja(datos, fisica.mitadTamano, fisica.desplazamiento, ctx, error)) return false;

        if (fisica.tipo == fisica::TipoCuerpo::Dinamico)
        {
            if (!datos.contains("mass"))
            {
                error = ctx + " es dynamic y necesita 'mass' (kg).";
                return false;
            }
            if (!leerNumero(datos["mass"], fisica.masa, ctx + ".mass", error)) return false;
            if (!(fisica.masa > 0.0f))
            {
                error = ctx + ".mass debe ser mayor que cero.";
                return false;
            }
        }
        else if (datos.contains("mass"))
        {
            error = ctx + ".mass solo tiene sentido en cuerpos dynamic.";
            return false;
        }
        fisica.presente = true;
        return true;
    }

    // Un elemento de la lista opcional "animated".
    bool leerObjetoAnimado(const json& entrada, const std::string& contexto, const std::filesystem::path& carpetaJson,
        SceneObjetoAnimado& objeto, std::string& error)
    {
        if (!entrada.is_object())
        {
            error = contexto + " debe ser un objeto JSON.";
            return false;
        }
        if (!soloClavesConocidas(entrada, { "id", "model", "position", "rotation", "scale", "animation", "colliders" },
            contexto, error)) return false;
        for (const char* campo : { "id", "model", "position" })
            if (!entrada.contains(campo))
            {
                error = contexto + " no contiene el campo obligatorio '" + campo + "'.";
                return false;
            }
        if (!entrada["id"].is_string() || entrada["id"].get<std::string>().empty())
        {
            error = contexto + " tiene un id vacio o incorrecto.";
            return false;
        }
        if (!entrada["model"].is_string() || entrada["model"].get<std::string>().empty())
        {
            error = contexto + " debe tener una ruta GLB no vacia en 'model'.";
            return false;
        }
        objeto.id = entrada["id"].get<std::string>();
        objeto.modelo = entrada["model"].get<std::string>();
        if (!leerVector3(entrada["position"], objeto.posicion, contexto + ".position", error)) return false;

        if (entrada.contains("rotation"))
        {
            glm::vec3 rotacion(0.0f);
            if (!leerVector3(entrada["rotation"], rotacion, contexto + ".rotation", error)) return false;
            if (rotacion.x != 0.0f || rotacion.y != 0.0f)
            {
                error = contexto + ".rotation: los objetos animados solo giran sobre Z, usa [0, 0, grados].";
                return false;
            }
            objeto.giroZGrados = rotacion.z;
        }
        if (entrada.contains("scale"))
        {
            if (!leerNumero(entrada["scale"], objeto.escala, contexto + ".scale", error)) return false;
            if (!(objeto.escala > 0.0f))
            {
                error = contexto + ".scale debe ser un numero mayor que cero (escala uniforme).";
                return false;
            }
        }
        if (entrada.contains("animation"))
        {
            if (!entrada["animation"].is_string())
            {
                error = contexto + ".animation debe ser el nombre (texto) de la animacion.";
                return false;
            }
            objeto.animacion = entrada["animation"].get<std::string>();
        }
        if (entrada.contains("colliders"))
        {
            if (!entrada["colliders"].is_array())
            {
                error = contexto + ".colliders debe ser una lista.";
                return false;
            }
            for (std::size_t i = 0; i < entrada["colliders"].size(); ++i)
            {
                const json& caja = entrada["colliders"][i];
                const std::string ctx = contexto + ".colliders[" + std::to_string(i) + "]";
                if (!caja.is_object())
                {
                    error = ctx + " debe ser un objeto JSON.";
                    return false;
                }
                if (!soloClavesConocidas(caja, { "node", "type", "halfSize", "offset" }, ctx, error)) return false;
                if (!caja.contains("node") || !caja["node"].is_string() || caja["node"].get<std::string>().empty() ||
                    !caja.contains("type"))
                {
                    error = ctx + " necesita 'node' (nombre del nodo del GLB) y 'type'.";
                    return false;
                }
                SceneColisionador colisionador;
                colisionador.nodo = caja["node"].get<std::string>();
                if (!leerTipo(caja["type"], true, colisionador.tipo, ctx, error)) return false;
                if (!leerCaja(caja, colisionador.mitadTamano, colisionador.desplazamiento, ctx, error)) return false;
                objeto.colisionadores.push_back(std::move(colisionador));
            }
        }
        const auto rutaModelo = (carpetaJson / objeto.modelo).lexically_normal();
        if (!std::filesystem::exists(rutaModelo) || !std::filesystem::is_regular_file(rutaModelo))
        {
            error = contexto + " apunta a un modelo GLB inexistente: " + rutaModelo.string();
            return false;
        }
        return true;
    }
}

bool CargarEscenaJSON(const std::filesystem::path& ruta,
    std::vector<SceneObject>& objetos, std::string& error, std::string& nombre)
{
    DatosEscena datos;
    if (!CargarDatosEscenaJSON(ruta, datos, error)) return false;
    objetos = std::move(datos.objetos);
    nombre = std::move(datos.nombre);
    return true;
}

bool CargarDatosEscenaJSON(const std::filesystem::path& ruta, DatosEscena& datosSalida, std::string& error)
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
        std::vector<SceneObjetoAnimado> candidatosAnimados;
        std::unordered_set<std::string> ids;   // los ids son unicos en TODA la escena (OBJ y animados)
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

                if (!leerFisica(entrada, objeto.fisica, contexto, error)) return false;

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

        // Lista OPCIONAL de objetos animados GLB.
        if (documento.contains("animated"))
        {
            if (!documento["animated"].is_array())
            {
                error = "El campo 'animated' debe ser una lista.";
                return false;
            }
            for (std::size_t indice = 0; indice < documento["animated"].size(); ++indice)
            {
                const std::string contexto = "animated[" + std::to_string(indice) + "]";
                SceneObjetoAnimado animado;
                if (!leerObjetoAnimado(documento["animated"][indice], contexto, ruta.parent_path(), animado, error))
                    return false;
                if (!ids.insert(animado.id).second)
                {
                    error = contexto + " tiene un id duplicado: '" + animado.id + "'.";
                    return false;
                }
                candidatosAnimados.push_back(std::move(animado));
            }
        }

        if (candidatos.empty() && candidatosAnimados.empty())
        {
            error = "La escena debe contener al menos un elemento.";
            return false;
        }

        // Todo es valido: ahora si se publica (nunca un resultado parcial).
        datosSalida.nombre = documento["name"].get<std::string>();
        datosSalida.objetos = std::move(candidatos);
        datosSalida.animados = std::move(candidatosAnimados);
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
