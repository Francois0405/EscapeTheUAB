#include <fstream>
#include <string>
#include <filesystem>
#include <vector>
#include <unordered_set>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <algorithm>
#include <array>
#include "src/scene/SceneLoader.h"
#include <nlohmann/json.hpp>

using nlohmann::json;

bool GenerarSeccionesPared(const glm::vec3& mitadPared, const std::vector<SceneWallOpening>& aperturas,
    std::vector<SceneWallSection>& secciones, std::string& error)
{
    std::vector<SceneWallSection> resultado;
    if (!std::isfinite(mitadPared.x) || !std::isfinite(mitadPared.y) || !std::isfinite(mitadPared.z) ||
        mitadPared.x <= 0.0f || mitadPared.y <= 0.0f || mitadPared.z <= 0.0f)
    { error = "Las dimensiones de pared deben ser finitas y positivas."; return false; }
    std::vector<SceneWallOpening> ordenadas = aperturas;
    std::sort(ordenadas.begin(), ordenadas.end(), [](const auto& a, const auto& b) { return a.centerU < b.centerU; });
    float cursor = -mitadPared.x;
    std::unordered_set<std::string> idsApertura, idsPuerta;
    constexpr float margenMinimo = 0.10f;
    for (std::size_t i = 0; i < ordenadas.size(); ++i)
    {
        const auto& a = ordenadas[i];
        if ((a.id.empty() && a.idPuerta.empty()) || !std::isfinite(a.centerU) || !std::isfinite(a.ancho) || !std::isfinite(a.alto) ||
            !std::isfinite(a.base) || a.ancho <= 0.0f || a.alto <= 0.0f || a.base < 0.0f)
        { error = "Una apertura contiene ID o dimensiones invalidos."; return false; }
        const std::string id = a.id.empty() ? a.idPuerta : a.id;
        if (!idsApertura.insert(id).second || (!a.idPuerta.empty() && !idsPuerta.insert(a.idPuerta).second))
        { error = "Una pared no puede repetir IDs de aperturas o asociar una puerta dos veces."; return false; }
        const float izquierda = a.centerU - a.ancho * 0.5f;
        const float derecha = a.centerU + a.ancho * 0.5f;
        const float arribaDesdeCentro = -mitadPared.z + a.base + a.alto;
        if (izquierda < -mitadPared.x + margenMinimo || derecha > mitadPared.x - margenMinimo ||
            arribaDesdeCentro > mitadPared.z - margenMinimo)
        { error = "La apertura '" + (a.id.empty() ? a.idPuerta : a.id) + "' invade el margen de esquina o supera la altura de la pared."; return false; }
        if (izquierda < cursor + margenMinimo)
        { error = "Las aperturas de pared se solapan o no conservan el margen minimo."; return false; }

        const float anchoLateral = izquierda - cursor;
        if (anchoLateral > 0.001f)
            resultado.push_back({ glm::vec3((cursor + izquierda) * 0.5f, 0.0f, 0.0f),
                glm::vec3(anchoLateral * 0.5f, mitadPared.y, mitadPared.z) });
        const float zAperturaInferior = -mitadPared.z + a.base;
        if (a.base > 0.001f)
            resultado.push_back({ glm::vec3(a.centerU, 0.0f, (-mitadPared.z + zAperturaInferior) * 0.5f),
                glm::vec3(a.ancho * 0.5f, mitadPared.y, a.base * 0.5f) });
        const float altoDintel = mitadPared.z - arribaDesdeCentro;
        if (altoDintel > 0.001f)
            resultado.push_back({ glm::vec3(a.centerU, 0.0f, (arribaDesdeCentro + mitadPared.z) * 0.5f),
                glm::vec3(a.ancho * 0.5f, mitadPared.y, altoDintel * 0.5f) });
        cursor = derecha;
    }
    const float anchoFinal = mitadPared.x - cursor;
    if (anchoFinal > 0.001f)
        resultado.push_back({ glm::vec3((cursor + mitadPared.x) * 0.5f, 0.0f, 0.0f),
            glm::vec3(anchoFinal * 0.5f, mitadPared.y, mitadPared.z) });
    if (resultado.empty()) { error = "La pared no tiene secciones solidas."; return false; }
    secciones = std::move(resultado);
    error.clear();
    return true;
}

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

    bool leerTipo(const json& valor, bool permitirCinematico, fisica::BodyType& tipo,
        const std::string& contexto, std::string& error)
    {
        if (valor.is_string())
        {
            const std::string texto = valor.get<std::string>();
            if (texto == "static") { tipo = fisica::BodyType::Estatico; return true; }
            if (texto == "dynamic" && !permitirCinematico) { tipo = fisica::BodyType::Dinamico; return true; }
            if (texto == "bouncy" && !permitirCinematico) { tipo = fisica::BodyType::Rebotador; return true; }
            if (texto == "kinematic" && permitirCinematico) { tipo = fisica::BodyType::Cinematico; return true; }
        }
        error = contexto + ".type debe ser " +
            (permitirCinematico ? "\"static\" o \"kinematic\"." : "\"static\", \"dynamic\" o \"bouncy\".");
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
    bool leerFisica(const json& entrada, ScenePhysics& fisica, const std::string& contexto, std::string& error)
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
        if (datos.contains("shape"))
        {
            if (!datos["shape"].is_string())
            { error = ctx + ".shape debe ser \"box\" o \"sphere\"."; return false; }
            const std::string forma = datos["shape"].get<std::string>();
            if (forma == "box") fisica.forma = fisica::BodyShape::Caja;
            else if (forma == "sphere") fisica.forma = fisica::BodyShape::Esfera;
            else
            { error = ctx + ".shape debe ser \"box\" o \"sphere\"."; return false; }
        }
        if (fisica.tipo == fisica::BodyType::Rebotador && fisica.forma != fisica::BodyShape::Esfera)
        { error = ctx + " de tipo bouncy requiere shape \"sphere\"."; return false; }
        if (!leerCaja(datos, fisica.mitadTamano, fisica.desplazamiento, ctx, error)) return false;

        if (fisica::EsCuerpoDinamico(fisica.tipo))
        {
            if (!datos.contains("mass"))
            {
                error = ctx + " es dinámico y necesita 'mass' (kg).";
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
            error = ctx + ".mass solo tiene sentido en cuerpos dynamic o bouncy.";
            return false;
        }
        fisica.presente = true;
        return true;
    }

    bool leerLuz(const json& entrada, SceneLight& luz, const std::string& contexto, std::string& error)
    {
        if (!entrada.contains("light")) return true;
        const json& datos = entrada["light"];
        const std::string ctx = contexto + ".light";
        if (!datos.is_object())
        {
            error = ctx + " debe ser un objeto JSON.";
            return false;
        }
        if (!soloClavesConocidas(datos, { "enabled", "offset", "color", "intensity", "areaSize" }, ctx, error)) return false;
        if (datos.contains("enabled"))
        {
            if (!datos["enabled"].is_boolean()) { error = ctx + ".enabled debe ser booleano."; return false; }
            luz.encendida = datos["enabled"].get<bool>();
        }
        if (datos.contains("offset") && !leerVector3(datos["offset"], luz.desplazamiento, ctx + ".offset", error)) return false;
        if (datos.contains("color") && !leerVector3(datos["color"], luz.color, ctx + ".color", error)) return false;
        for (int eje = 0; eje < 3; ++eje)
            if (luz.color[eje] < 0.0f || luz.color[eje] > 1.0f)
            { error = ctx + ".color debe estar entre 0 y 1."; return false; }
        if (datos.contains("intensity") && !leerNumero(datos["intensity"], luz.intensidad, ctx + ".intensity", error)) return false;
        if (!(luz.intensidad > 0.0f) || luz.intensidad > 16.0f)
        { error = ctx + ".intensity debe estar entre 0 y 16."; return false; }
        if (datos.contains("areaSize"))
        {
            const json& tamano = datos["areaSize"];
            if (!tamano.is_array() || tamano.size() != 2 || !tamano[0].is_number() || !tamano[1].is_number())
            { error = ctx + ".areaSize debe ser [ancho, fondo] en unidades locales."; return false; }
            luz.areaSize = glm::vec2(tamano[0].get<float>(), tamano[1].get<float>());
            if (!std::isfinite(luz.areaSize.x) || !std::isfinite(luz.areaSize.y) ||
                luz.areaSize.x < 0.0f || luz.areaSize.y < 0.0f ||
                luz.areaSize.x > 10.0f || luz.areaSize.y > 10.0f ||
                ((luz.areaSize.x == 0.0f) != (luz.areaSize.y == 0.0f)))
            { error = ctx + ".areaSize debe ser [0,0] o tener ambas dimensiones entre 0 y 10."; return false; }
        }
        luz.presente = true;
        return true;
    }

    bool leerMaterial(const json& entrada, SceneMaterial& material, const std::filesystem::path& carpeta,
        const std::string& contexto, std::string& error)
    {
        if (!entrada.contains("material")) return true;
        const json& datos = entrada["material"];
        const std::string ctx = contexto + ".material";
        if (!datos.is_object()) { error = ctx + " debe ser un objeto JSON."; return false; }
        if (!soloClavesConocidas(datos, { "texture", "tileSize", "roughness", "dirtStrength", "dirtScale", "dirtSeed" }, ctx, error)) return false;
        if (datos.contains("texture"))
        {
            if (!datos["texture"].is_string() || datos["texture"].get<std::string>().empty())
            { error = ctx + ".texture debe ser una ruta no vacia."; return false; }
            material.textura = datos["texture"].get<std::string>();
            const std::filesystem::path rutaTextura(material.textura);
            if (rutaTextura.is_absolute()) { error = ctx + ".texture debe ser relativa a la escena."; return false; }
            const auto imagen = (carpeta / rutaTextura).lexically_normal();
            if (!std::filesystem::is_regular_file(imagen))
            { error = ctx + " apunta a una textura inexistente: " + imagen.string(); return false; }
        }
        if (datos.contains("tileSize"))
        {
            if (!datos["tileSize"].is_array() || datos["tileSize"].size() != 2 ||
                !datos["tileSize"][0].is_number() || !datos["tileSize"][1].is_number())
            { error = ctx + ".tileSize debe ser [metrosX, metrosY]."; return false; }
            material.tileSize = glm::vec2(datos["tileSize"][0].get<float>(), datos["tileSize"][1].get<float>());
            if (!std::isfinite(material.tileSize.x) || !std::isfinite(material.tileSize.y) ||
                material.tileSize.x < 0.1f || material.tileSize.y < 0.1f ||
                material.tileSize.x > 20.0f || material.tileSize.y > 20.0f)
            { error = ctx + ".tileSize debe estar entre 0.1 y 20 metros."; return false; }
        }
        if (datos.contains("roughness") && !leerNumero(datos["roughness"], material.roughness, ctx + ".roughness", error)) return false;
        if (material.roughness < 0.0f || material.roughness > 1.0f)
        { error = ctx + ".roughness debe estar entre 0 y 1."; return false; }
        if (datos.contains("dirtStrength") && !leerNumero(datos["dirtStrength"], material.dirtStrength, ctx + ".dirtStrength", error)) return false;
        if (material.dirtStrength < 0.0f || material.dirtStrength > 0.5f)
        { error = ctx + ".dirtStrength debe estar entre 0 y 0.5."; return false; }
        if (datos.contains("dirtScale") && !leerNumero(datos["dirtScale"], material.dirtScale, ctx + ".dirtScale", error)) return false;
        if (material.dirtScale < 0.05f || material.dirtScale > 4.0f)
        { error = ctx + ".dirtScale debe estar entre 0.05 y 4 celdas por metro."; return false; }
        if (datos.contains("dirtSeed") && !leerNumero(datos["dirtSeed"], material.dirtSeed, ctx + ".dirtSeed", error)) return false;
        material.presente = true;
        return true;
    }

    // Un elemento de la lista opcional "animated".
    bool leerObjetoAnimado(const json& entrada, const std::string& contexto, const std::filesystem::path& carpetaJson,
        SceneAnimatedObject& objeto, std::string& error)
    {
        if (!entrada.is_object())
        {
            error = contexto + " debe ser un objeto JSON.";
            return false;
        }
        if (!soloClavesConocidas(entrada, { "id", "model", "position", "rotation", "scale", "animation", "colliders", "allowArchitecturePenetration", "wallId", "openingId" },
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
        if (entrada.contains("wallId") != entrada.contains("openingId"))
        { error = contexto + " requiere wallId y openingId conjuntamente."; return false; }
        if (entrada.contains("wallId"))
        {
            if (!entrada["wallId"].is_string() || !entrada["openingId"].is_string() ||
                entrada["wallId"].get<std::string>().empty() || entrada["openingId"].get<std::string>().empty())
            { error = contexto + ".wallId/openingId deben ser textos no vacios."; return false; }
            objeto.paredId = entrada["wallId"].get<std::string>();
            objeto.aperturaId = entrada["openingId"].get<std::string>();
        }
        if (entrada.contains("allowArchitecturePenetration"))
        {
            if (!entrada["allowArchitecturePenetration"].is_boolean())
            { error = contexto + ".allowArchitecturePenetration debe ser booleano."; return false; }
            objeto.permitirPenetracionArquitectura = entrada["allowArchitecturePenetration"].get<bool>();
        }
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
            objeto.animation = entrada["animation"].get<std::string>();
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
                SceneCollider colisionador;
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
    SceneData datos;
    if (!CargarDatosEscenaJSON(ruta, datos, error)) return false;
    objetos = std::move(datos.objetos);
    nombre = std::move(datos.nombre);
    return true;
}

bool CargarDatosEscenaJSON(const std::filesystem::path& ruta, SceneData& datosSalida, std::string& error)
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
        std::vector<SceneAnimatedObject> candidatosAnimados;
        SceneSpawn spawn;
        float indirectIntensity = 0.16f;
        float indirectAmbientBase = 0.025f;
        float directSceneLightScale = 1.0f;
        glm::vec3 indirectFloorColor(0.42f, 0.46f, 0.43f);
        glm::vec3 indirectSkyColor(0.24f, 0.28f, 0.34f);
        if (documento.contains("indirectLighting"))
        {
            const json& indirect = documento["indirectLighting"];
            if (!indirect.is_object() || !soloClavesConocidas(indirect,
                { "intensity", "ambientBase", "directScale", "floorColor", "skyColor" }, "indirectLighting", error) ||
                !indirect.contains("intensity") || !leerNumero(indirect["intensity"], indirectIntensity, "indirectLighting.intensity", error) ||
                indirectIntensity < 0.0f || indirectIntensity > 0.5f)
            {
                if (error.empty()) error = "indirectLighting.intensity debe estar entre 0 y 0.5.";
                return false;
            }
            if (indirect.contains("ambientBase") &&
                (!leerNumero(indirect["ambientBase"], indirectAmbientBase, "indirectLighting.ambientBase", error) ||
                    indirectAmbientBase < 0.0f || indirectAmbientBase > 0.15f))
            { if (error.empty()) error = "indirectLighting.ambientBase debe estar entre 0 y 0.15."; return false; }
            if (indirect.contains("directScale") &&
                (!leerNumero(indirect["directScale"], directSceneLightScale, "indirectLighting.directScale", error) ||
                    directSceneLightScale < 0.0f || directSceneLightScale > 1.5f))
            { if (error.empty()) error = "indirectLighting.directScale debe estar entre 0 y 1.5."; return false; }
            for (const auto& [clave, destino] : std::array<std::pair<const char*, glm::vec3*>, 2>{ {
                { "floorColor", &indirectFloorColor }, { "skyColor", &indirectSkyColor } } })
                if (indirect.contains(clave))
                {
                    if (!leerVector3(indirect[clave], *destino, std::string("indirectLighting.") + clave, error)) return false;
                    for (int eje = 0; eje < 3; ++eje)
                        if ((*destino)[eje] < 0.0f || (*destino)[eje] > 1.0f)
                        { error = std::string("indirectLighting.") + clave + " debe estar entre 0 y 1."; return false; }
                }
        }
        if (documento.contains("playerSpawn"))
        {
            const json& entrada = documento["playerSpawn"];
            if (!entrada.is_object() || !soloClavesConocidas(entrada, { "feetPosition", "lookDirection" }, "playerSpawn", error))
                return false;
            if (!entrada.contains("feetPosition") || !entrada.contains("lookDirection"))
            {
                error = "playerSpawn requiere 'feetPosition' y 'lookDirection'.";
                return false;
            }
            if (!leerVector3(entrada["feetPosition"], spawn.pies, "playerSpawn.feetPosition", error) ||
                !leerVector3(entrada["lookDirection"], spawn.direccion, "playerSpawn.lookDirection", error)) return false;
            if (glm::length(spawn.direccion) < 1.0e-4f)
            {
                error = "playerSpawn.lookDirection no puede ser un vector nulo.";
                return false;
            }
            spawn.direccion = glm::normalize(spawn.direccion);
            spawn.presente = true;
        }
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
                if (entrada.contains("allowArchitecturePenetration"))
                {
                    if (!entrada["allowArchitecturePenetration"].is_boolean())
                    { error = contexto + ".allowArchitecturePenetration debe ser booleano."; return false; }
                    objeto.permitirPenetracionArquitectura = entrada["allowArchitecturePenetration"].get<bool>();
                }
                if (!leerVector3(entrada["position"], objeto.posicion, contexto + ".position", error) ||
                    !leerVector3(entrada["rotation"], objeto.rotacion, contexto + ".rotation", error) ||
                    !leerVector3(entrada["scale"], objeto.escala, contexto + ".scale", error))
                    return false;

                if (!leerFisica(entrada, objeto.fisica, contexto, error) || !leerLuz(entrada, objeto.luz, contexto, error) ||
                    !leerMaterial(entrada, objeto.material, ruta.parent_path(), contexto, error)) return false;

                if (entrada.contains("wall"))
                {
                    const json& wall = entrada["wall"];
                    if (!wall.is_object() || !soloClavesConocidas(wall, { "apertures" }, contexto + ".wall", error) ||
                        !wall.contains("apertures") || !wall["apertures"].is_array())
                    { if (error.empty()) error = contexto + ".wall.apertures debe ser una lista."; return false; }
                    objeto.pared.presente = true;
                    for (std::size_t a = 0; a < wall["apertures"].size(); ++a)
                    {
                        const json& entradaApertura = wall["apertures"][a];
                        const std::string ctxA = contexto + ".wall.apertures[" + std::to_string(a) + "]";
                        if (!entradaApertura.is_object() || !soloClavesConocidas(entradaApertura,
                            { "id", "doorId", "centerU", "width", "height", "base" }, ctxA, error) ||
                            (!entradaApertura.contains("id") && !entradaApertura.contains("doorId")) ||
            (entradaApertura.contains("id") && !entradaApertura["id"].is_string()) ||
            (entradaApertura.contains("doorId") && !entradaApertura["doorId"].is_string()) ||
                            !entradaApertura.contains("centerU") || !entradaApertura.contains("width") || !entradaApertura.contains("height"))
                        { if (error.empty()) error = ctxA + " requiere id, centerU, width y height."; return false; }
                        SceneWallOpening ap;
                        ap.id = entradaApertura.contains("id") ? entradaApertura["id"].get<std::string>() : entradaApertura["doorId"].get<std::string>();
                        ap.idPuerta = entradaApertura.contains("doorId") ? entradaApertura["doorId"].get<std::string>() : std::string{};
                        if (!leerNumero(entradaApertura["centerU"], ap.centerU, ctxA + ".centerU", error) ||
                            !leerNumero(entradaApertura["width"], ap.ancho, ctxA + ".width", error) ||
                            !leerNumero(entradaApertura["height"], ap.alto, ctxA + ".height", error) ||
                            (entradaApertura.contains("base") && !leerNumero(entradaApertura["base"], ap.base, ctxA + ".base", error))) return false;
                        objeto.pared.aperturas.push_back(std::move(ap));
                    }
                    std::vector<SceneWallSection> comprobacion;
                    if (!GenerarSeccionesPared(glm::abs(objeto.escala) * objeto.fisica.mitadTamano, objeto.pared.aperturas, comprobacion, error))
                    { error = contexto + ".wall: " + error; return false; }
                    if (!objeto.fisica.presente || objeto.fisica.tipo != fisica::BodyType::Estatico ||
                        objeto.fisica.mitadTamano != glm::vec3(1.0f) ||
                        objeto.fisica.desplazamiento != glm::vec3(0.0f))
                    { error = contexto + ".wall requiere un collider static centrado en el cubo de pared."; return false; }
                }

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
                SceneAnimatedObject animado;
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

        for (const SceneObject& pared : candidatos)
            for (const SceneWallOpening& apertura : pared.pared.aperturas)
            {
                if (apertura.idPuerta.empty()) continue; // hueco intencional sin puerta
                const auto puerta = std::find_if(candidatosAnimados.begin(), candidatosAnimados.end(), [&](const auto& a) {
                    return a.id == apertura.idPuerta && a.paredId == pared.id;
                });
                if (puerta == candidatosAnimados.end() || puerta->aperturaId != apertura.id)
                { error = "La apertura de '" + pared.id + "' no tiene una puerta animada asociada de forma univoca."; return false; }
            }
        for (const SceneAnimatedObject& puerta : candidatosAnimados)
            if (!puerta.paredId.empty())
            {
                const auto pared = std::find_if(candidatos.begin(), candidatos.end(), [&](const SceneObject& item) { return item.id == puerta.paredId; });
                if (pared == candidatos.end() || !pared->pared.presente)
                { error = "La puerta '" + puerta.id + "' referencia una pared paramétrica inexistente."; return false; }
                const auto apertura = std::find_if(pared->pared.aperturas.begin(), pared->pared.aperturas.end(),
                    [&](const SceneWallOpening& item) { return item.id == puerta.aperturaId && item.idPuerta == puerta.id; });
                if (apertura == pared->pared.aperturas.end())
                { error = "La puerta '" + puerta.id + "' referencia un vano inexistente o asociado a otra instancia."; return false; }
            }

        if (candidatos.empty() && candidatosAnimados.empty())
        {
            error = "La escena debe contener al menos un elemento.";
            return false;
        }

        // Todo es valido: ahora si se publica (nunca un resultado parcial).
        datosSalida.nombre = documento["name"].get<std::string>();
        datosSalida.spawn = spawn;
        datosSalida.indirectIntensity = indirectIntensity;
        datosSalida.indirectAmbientBase = indirectAmbientBase;
        datosSalida.directSceneLightScale = directSceneLightScale;
        datosSalida.indirectFloorColor = indirectFloorColor;
        datosSalida.indirectSkyColor = indirectSkyColor;
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
