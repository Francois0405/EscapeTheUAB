#include "SceneLoader.h"
#include "MundoFisico.h"

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

    // ---------------------------------------------------------------------
    // Ampliacion del formato: "physics" y la lista "animated" (ambos OPCIONALES).
    // ---------------------------------------------------------------------
    escribir(temp / "models" / "puerta.glb", "glb falso: solo se comprueba que existe\n");
    const auto conFisica = [&](const std::string& fisica) {
        std::string o = object;
        o.insert(o.size() - 1, ",\"physics\":" + fisica);
        return escena("[" + o + "]");
    };
    const auto textoDelError = [&](const std::string& json, const char* archivo) {
        escribir(temp / archivo, json);
        DatosEscena datos;
        std::string mensaje;
        const bool ok = CargarDatosEscenaJSON(temp / archivo, datos, mensaje);
        return ok ? std::string("<sin error>") : mensaje;
    };

    // 1) Un JSON antiguo (sin physics ni animated) sigue cargando y no tiene fisica.
    DatosEscena datos;
    comprobar(CargarDatosEscenaJSON(temp / "relative-model.json", datos, error), "JSON antiguo carga con el cargador nuevo");
    comprobar(datos.objetos.size() == 1 && !datos.objetos[0].fisica.presente && datos.animados.empty(),
        "sin 'physics' el objeto no tiene fisica");
    comprobar(CargarDatosEscenaJSON(habitacion, datos, error) && datos.objetos.size() == 12,
        "habitacion_beta sigue con 12 instancias OBJ");
    comprobar(datos.animados.size() == 2 && datos.animados[0].colisionadores.size() == 4,
        "habitacion_beta declara dos puertas animadas con 4 colisionadores cada una");

    // 2) Fisica valida: se interpreta cada campo.
    escribir(temp / "fisica-ok.json", conFisica(
        "{\"type\":\"dynamic\",\"shape\":\"box\",\"halfSize\":[0.5,0.25,1],\"offset\":[0,0,0.5],\"mass\":2.5}"));
    comprobar(CargarDatosEscenaJSON(temp / "fisica-ok.json", datos, error), "physics valido carga");
    {
        const SceneFisica& f = datos.objetos[0].fisica;
        comprobar(f.presente && f.tipo == fisica::TipoCuerpo::Dinamico && f.masa == 2.5f &&
            f.mitadTamano == glm::vec3(0.5f, 0.25f, 1.0f) && f.desplazamiento == glm::vec3(0, 0, 0.5f),
            "physics: tipo, masa, halfSize y offset se leen bien");
    }
    escribir(temp / "fisica-estatica.json", conFisica("{\"type\":\"static\",\"halfSize\":[1,1,1]}"));
    comprobar(CargarDatosEscenaJSON(temp / "fisica-estatica.json", datos, error) &&
        datos.objetos[0].fisica.tipo == fisica::TipoCuerpo::Estatico && datos.objetos[0].fisica.desplazamiento == glm::vec3(0.0f),
        "physics estatico: offset opcional vale 0");

    // 3) Fisica incorrecta: error comprensible que dice QUE campo falla.
    const auto contiene = [](const std::string& texto, const char* trozo) { return texto.find(trozo) != std::string::npos; };
    std::string mensaje = textoDelError(conFisica("{\"type\":\"volador\",\"halfSize\":[1,1,1]}"), "f1.json");
    comprobar(contiene(mensaje, "physics.type"), "tipo desconocido: el error nombra physics.type");
    mensaje = textoDelError(conFisica("{\"type\":\"static\"}"), "f2.json");
    comprobar(contiene(mensaje, "halfSize"), "falta halfSize: el error lo nombra");
    mensaje = textoDelError(conFisica("{\"type\":\"static\",\"halfSize\":[1,0,1]}"), "f3.json");
    comprobar(contiene(mensaje, "halfSize") && contiene(mensaje, "mayor que cero"), "halfSize con 0 se rechaza");
    mensaje = textoDelError(conFisica("{\"type\":\"dynamic\",\"halfSize\":[1,1,1]}"), "f4.json");
    comprobar(contiene(mensaje, "mass"), "dynamic sin masa se rechaza");
    mensaje = textoDelError(conFisica("{\"type\":\"dynamic\",\"halfSize\":[1,1,1],\"mass\":-3}"), "f5.json");
    comprobar(contiene(mensaje, "mass"), "masa negativa se rechaza");
    mensaje = textoDelError(conFisica("{\"type\":\"static\",\"halfSize\":[1,1,1],\"mass\":3}"), "f6.json");
    comprobar(contiene(mensaje, "mass"), "masa en un estatico se rechaza");
    mensaje = textoDelError(conFisica("{\"type\":\"static\",\"halfsize\":[1,1,1]}"), "f7.json");
    comprobar(contiene(mensaje, "halfsize"), "un campo mal escrito se detecta en vez de ignorarse");
    mensaje = textoDelError(conFisica("{\"type\":\"static\",\"shape\":\"esfera\",\"halfSize\":[1,1,1]}"), "f8.json");
    comprobar(contiene(mensaje, "box"), "forma distinta de box se rechaza");

    // 4) Lista "animated".
    const std::string animado =
        "{\"id\":\"p1\",\"model\":\"models/puerta.glb\",\"position\":[1,2,3],\"rotation\":[0,0,-90],\"scale\":2,"
        "\"animation\":\"Abrir\",\"colliders\":[{\"node\":\"Door\",\"type\":\"kinematic\",\"halfSize\":[1,1,1]}]}";
    const auto conAnimados = [&](const std::string& lista) {
        return "{\"version\":1,\"name\":\"t\",\"architecture\":[],\"objects\":[" + object + "],\"animated\":" + lista + "}";
    };
    escribir(temp / "animado-ok.json", conAnimados("[" + animado + "]"));
    comprobar(CargarDatosEscenaJSON(temp / "animado-ok.json", datos, error), "objeto animado valido carga");
    comprobar(datos.animados.size() == 1 && datos.animados[0].giroZGrados == -90.0f && datos.animados[0].escala == 2.0f &&
        datos.animados[0].animacion == "Abrir" && datos.animados[0].colisionadores[0].tipo == fisica::TipoCuerpo::Cinematico,
        "objeto animado: giro, escala, animacion y colisionador se leen bien");
    std::string distinto = animado;
    distinto.replace(distinto.find("\"p1\""), 4, "\"p2\"");
    escribir(temp / "animado-dos.json", conAnimados("[" + animado + "," + distinto + "]"));
    comprobar(CargarDatosEscenaJSON(temp / "animado-dos.json", datos, error) && datos.animados.size() == 2,
        "dos instancias del mismo GLB con ids distintos son validas");
    mensaje = textoDelError(conAnimados("[" + animado + "," + animado + "]"), "a1.json");
    comprobar(contiene(mensaje, "duplicado"), "id animado duplicado se rechaza");
    std::string conObjeto = animado;
    conObjeto.replace(conObjeto.find("\"p1\""), 4, "\"same\"");
    mensaje = textoDelError(conAnimados("[" + conObjeto + "]"), "a2.json");
    comprobar(contiene(mensaje, "duplicado"), "un animado no puede repetir el id de un objeto OBJ");
    std::string girado = animado;
    girado.replace(girado.find("[0,0,-90]"), 9, "[45,0,0]");
    mensaje = textoDelError(conAnimados("[" + girado + "]"), "a3.json");
    comprobar(contiene(mensaje, "rotation"), "rotacion en X/Y de un animado se rechaza con explicacion");
    std::string sinGlb = animado;
    sinGlb.replace(sinGlb.find("puerta.glb"), 10, "falta.glb");
    mensaje = textoDelError(conAnimados("[" + sinGlb + "]"), "a4.json");
    comprobar(contiene(mensaje, "inexistente"), "GLB inexistente se rechaza");
    std::string tipoMalo = animado;
    tipoMalo.replace(tipoMalo.find("kinematic"), 9, "dynamic");
    mensaje = textoDelError(conAnimados("[" + tipoMalo + "]"), "a5.json");
    comprobar(contiene(mensaje, "colliders[0].type"), "un colisionador animado no puede ser dynamic");
    mensaje = textoDelError(conAnimados("{}"), "a6.json");
    comprobar(contiene(mensaje, "animated"), "'animated' debe ser una lista");

    // 5) Una carga fallida no publica nada: los datos previos quedan intactos.
    DatosEscena previos;
    comprobar(CargarDatosEscenaJSON(temp / "animado-dos.json", previos, error), "preparar datos previos");
    comprobar(!CargarDatosEscenaJSON(temp / "a3.json", previos, error) && previos.animados.size() == 2 &&
        previos.objetos.size() == 1, "carga fallida no modifica los datos previos");

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
