#include "Editor/SerializadorEscenas.h"
#include "SceneLoader.h"
#include <nlohmann/json.hpp>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

namespace fs = std::filesystem;

namespace
{
	int fallos = 0;

	void comprobar(bool condicion, const char* caso)
	{
		if (!condicion)
		{
			std::cerr << "FALLO: " << caso << '\n';
			++fallos;
		}
	}

	void escribir(const fs::path& ruta, const std::string& contenido)
	{
		fs::create_directories(ruta.parent_path());
		std::ofstream archivo(ruta, std::ios::binary | std::ios::trunc);
		archivo << contenido;
	}

	std::string leer(const fs::path& ruta)
	{
		std::ifstream archivo(ruta, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(archivo), std::istreambuf_iterator<char>());
	}
}

int main()
{
	const auto unico = std::chrono::steady_clock::now().time_since_epoch().count();
	const fs::path temp = fs::temp_directory_path() / ("EscapeTheUAB-EditorPersistenceTests-" + std::to_string(unico));
	fs::create_directories(temp / "models");
	escribir(temp / "models" / "floor.png", "fixture texture");
	const fs::path escena = temp / "scenes" / "room.json";
	escribir(temp / "models" / "table.obj", "# test model\n");
	escribir(temp / "models" / "door.glb", "# test animated model\n");
	const std::string original = R"json({
  "version": 1,
  "name": "persistence-test",
  "indirectLighting": {"intensity":0.20,"ambientBase":0.035,"directScale":0.80,"floorColor":[0.42,0.46,0.43],"skyColor":[0.24,0.28,0.34]},
  "architecture": [
    {"id":"floor","model":"../models/table.obj","position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}
  ],
  "objects": [
    {"id":"table","model":"../models/table.obj","position":[1,2,3],"rotation":[0,0,0],"scale":[1,1,1],"allowArchitecturePenetration":true,
     "light":{"enabled":true,"offset":[0,0.2,0],"color":[0.8,0.9,1.0],"intensity":2.5,"areaSize":[2.4,0.38]},
     "physics":{"type":"static","halfSize":[0.5,0.4,0.3],"offset":[0.1,0.2,0.3]}}
  ],
  "animated": [
    {"id":"door","model":"../models/door.glb","position":[4,5,6],"rotation":[0,0,90],"scale":1.0,"allowArchitecturePenetration":true,
     "colliders":[{"node":"Door","type":"kinematic","halfSize":[0.4,1.0,0.1],"offset":[0,0,0]}]}
  ]
})json";
	escribir(escena, original);

	std::string error;
	editor::SerializadorEscenas serializador;
	comprobar(serializador.Cargar(escena, error), "carga el documento JSON de la escena");
	DatosEscena datos;
	comprobar(CargarDatosEscenaJSON(escena, datos, error), "fixture JSON válido");
	comprobar(std::abs(datos.indirectIntensity - 0.20f) < 1e-6f &&
		std::abs(datos.indirectAmbientBase - 0.035f) < 1e-6f &&
		std::abs(datos.directSceneLightScale - 0.80f) < 1e-6f,
		"carga los parametros de iluminacion configurables");
	SceneObject mesa = datos.objetos.at(1);
	mesa.posicion = glm::vec3(7.25f, -3.5f, 1.75f);
	mesa.rotacion = glm::vec3(15.0f, -20.0f, 90.0f);
	mesa.escala = glm::vec3(1.25f, 0.75f, 2.0f);
	mesa.material.presente = true;
	mesa.material.textura = "../models/floor.png";
	mesa.material.tileSize = glm::vec2(1.0f, 1.25f);
	mesa.material.roughness = 0.92f;
	mesa.material.dirtStrength = 0.2f;
	mesa.material.dirtScale = 0.42f;
	mesa.material.dirtSeed = 17.0f;
	datos.objetos[1] = mesa;
	comprobar(serializador.Guardar(datos.objetos, error), "guarda transformaciones válidas");
	comprobar(fs::exists(escena.string() + ".bak"), "crea una copia de seguridad antes de reemplazar");

	nlohmann::json guardado = nlohmann::json::parse(leer(escena));
	const auto& guardada = guardado["objects"][0];
	comprobar(guardada["model"] == "../models/table.obj", "preserva la ruta relativa original");
	comprobar(guardada["position"] == nlohmann::json::array({ 7.25, -3.5, 1.75 }), "persiste posición");
	comprobar(guardado["indirectLighting"]["intensity"] == 0.20 &&
		guardado["indirectLighting"]["ambientBase"] == 0.035 &&
		guardado["indirectLighting"]["directScale"] == 0.80 &&
		guardado["indirectLighting"]["floorColor"] == nlohmann::json::array({ 0.42, 0.46, 0.43 }) &&
		guardado["indirectLighting"]["skyColor"] == nlohmann::json::array({ 0.24, 0.28, 0.34 }),
		"Save preserva intactos los parametros globales de iluminacion");
	comprobar(guardada["rotation"] == nlohmann::json::array({ 15.0, -20.0, 90.0 }), "persiste rotación");
	comprobar(guardada["scale"] == nlohmann::json::array({ 1.25, 0.75, 2.0 }), "persiste escala");
	comprobar(guardada["allowArchitecturePenetration"] == true, "persiste la excepcion explicita de arquitectura");
	comprobar(guardada["physics"]["halfSize"] == nlohmann::json::array({ 0.5, 0.4, 0.3 }), "preserva halfSize físico");
	comprobar(guardada["physics"]["offset"] == nlohmann::json::array({ 0.1, 0.2, 0.3 }), "preserva offset físico");
	comprobar(guardada["light"]["enabled"] == true &&
		std::abs(guardada["light"]["offset"][0].get<double>()) < 1e-5 &&
		std::abs(guardada["light"]["offset"][1].get<double>() - 0.2) < 1e-5 &&
		std::abs(guardada["light"]["offset"][2].get<double>()) < 1e-5 &&
		std::abs(guardada["light"]["color"][0].get<double>() - 0.8) < 1e-5 &&
		std::abs(guardada["light"]["color"][1].get<double>() - 0.9) < 1e-5 &&
		std::abs(guardada["light"]["color"][2].get<double>() - 1.0) < 1e-5 &&
		std::abs(guardada["light"]["intensity"].get<double>() - 2.5) < 1e-5 &&
		std::abs(guardada["light"]["areaSize"][0].get<double>() - 2.4) < 1e-5 &&
		std::abs(guardada["light"]["areaSize"][1].get<double>() - 0.38) < 1e-5,
		"preserva la luz asociada a la instancia al guardar desde EDIT");
	comprobar(guardada["material"]["texture"] == "../models/floor.png" &&
		guardada["material"]["tileSize"] == nlohmann::json::array({ 1.0, 1.25 }) &&
		std::abs(guardada["material"]["roughness"].get<double>() - 0.92) < 1e-5 &&
		std::abs(guardada["material"]["dirtStrength"].get<double>() - 0.2) < 1e-5 &&
		std::abs(guardada["material"]["dirtScale"].get<double>() - 0.42) < 1e-5 &&
		std::abs(guardada["material"]["dirtSeed"].get<double>() - 17.0) < 1e-5,
		"guarda textura relativa y parametros del material de instancia");
	comprobar(guardado["animated"][0]["colliders"][0]["type"] == "kinematic", "preserva colisionador animado");
	comprobar(guardado["animated"][0]["allowArchitecturePenetration"] == true, "persiste la excepcion explicita en la puerta GLB");
	comprobar(guardado["architecture"][0]["position"] == nlohmann::json::array({ 0, 0, 0 }), "preserva instancias no editadas");
	DatosEscena recargada;
	comprobar(CargarDatosEscenaJSON(escena, recargada, error), "recarga JSON editado");
	comprobar(recargada.animados[0].permitirPenetracionArquitectura, "la puerta GLB restaura su excepcion explicita");
	comprobar(recargada.objetos[1].posicion == mesa.posicion && recargada.objetos[1].rotacion == mesa.rotacion &&
		recargada.objetos[1].escala == mesa.escala, "la recarga conserva transformaciones exactas");
	comprobar(recargada.objetos[1].permitirPenetracionArquitectura, "la recarga restaura la excepcion explicita");
	comprobar(recargada.objetos[1].luz.presente && recargada.objetos[1].luz.encendida &&
		recargada.objetos[1].luz.areaSize == glm::vec2(2.4f, 0.38f), "la recarga restaura estado y forma de la luz");
	comprobar(recargada.objetos[1].material.presente && recargada.objetos[1].material.textura == mesa.material.textura &&
		recargada.objetos[1].material.tileSize == mesa.material.tileSize && recargada.objetos[1].material.roughness == mesa.material.roughness &&
		recargada.objetos[1].material.dirtStrength == mesa.material.dirtStrength &&
		recargada.objetos[1].material.dirtScale == mesa.material.dirtScale && recargada.objetos[1].material.dirtSeed == mesa.material.dirtSeed,
		"la recarga restaura el material configurable");
	SceneObject duplicada = mesa;
	duplicada.id = "table_copy";
	comprobar(serializador.Guardar({ datos.objetos[0], mesa, duplicada }, error), "serializa una instancia duplicada con ID nuevo");
	guardado = nlohmann::json::parse(leer(escena));
	comprobar(guardado["objects"].size() == 2 && guardado["objects"][1]["id"] == "table_copy",
		"añade la copia sin cambiar la ruta relativa");
	comprobar(guardado["objects"][1]["model"] == "../models/table.obj", "serializa ruta relativa para objeto añadido");
	comprobar(serializador.Guardar({ datos.objetos[0], mesa }, error), "serializa la eliminación de una instancia");
	guardado = nlohmann::json::parse(leer(escena));
	comprobar(guardado["objects"].size() == 1 && guardado["objects"][0]["id"] == "table",
		"elimina solo la instancia retirada");
	comprobar(serializador.Guardar({ datos.objetos[0], mesa }, std::vector<SceneObjetoAnimado>{}, error),
		"guarda la escena al eliminar la ultima instancia GLB");
	guardado = nlohmann::json::parse(leer(escena));
	comprobar(guardado.contains("animated") && guardado["animated"].is_array() && guardado["animated"].empty(),
		"borrar la ultima puerta deja animated vacio en JSON");
	comprobar(CargarDatosEscenaJSON(escena, recargada, error) && recargada.animados.empty(),
		"recargar no resucita la puerta GLB eliminada");
	const fs::path paredEscena = temp / "scenes" / "wall-openings.json";
	escribir(paredEscena, R"json({"version":1,"name":"wall-openings","architecture":[
      {"id":"wall","model":"../models/table.obj","position":[0,0,0],"rotation":[0,0,90],"scale":[5,0.15,1.5],
       "physics":{"type":"static","halfSize":[1,1,1]},
       "wall":{"apertures":[{"id":"door-opening","doorId":"door","centerU":0.5,"width":1.0,"height":2.1,"base":0.0}]}}
      ],"objects":[],"animated":[{"id":"door","model":"../models/door.glb","position":[0.5,0,-1.5],"rotation":[0,0,90],"wallId":"wall","openingId":"door-opening"}]})json");
	editor::SerializadorEscenas serializerWall;
	comprobar(serializerWall.Cargar(paredEscena, error), "carga escena con pared paramétrica y asociación de puerta");
	DatosEscena paredDatos;
	comprobar(CargarDatosEscenaJSON(paredEscena, paredDatos, error), "valida pared y apertura asociada");
	comprobar(serializerWall.Guardar(paredDatos.objetos, paredDatos.animados, error), "guarda pared paramétrica sin hornear el OBJ");
	DatosEscena paredRecargada;
	comprobar(CargarDatosEscenaJSON(paredEscena, paredRecargada, error) &&
		paredRecargada.objetos[0].pared.aperturas[0].id == "door-opening" &&
		paredRecargada.animados[0].paredId == "wall" && paredRecargada.animados[0].aperturaId == "door-opening",
		"Save/Reload conserva exactamente el vínculo pared, vano y puerta");
	paredDatos.objetos[0].pared.aperturas[0].idPuerta.clear();
	paredDatos.animados.clear();
	comprobar(serializerWall.Guardar(paredDatos.objetos, paredDatos.animados, error), "guarda una apertura abierta sin puerta");
	comprobar(CargarDatosEscenaJSON(paredEscena, paredRecargada, error) && paredRecargada.animados.empty() &&
		paredRecargada.objetos[0].pared.aperturas.size() == 1 && paredRecargada.objetos[0].pared.aperturas[0].idPuerta.empty(),
		"el hueco puede persistir después de eliminar solo la puerta");

	const fs::path runtimeScene = fs::path(ENTORNVGI_RUNTIME_ASSETS_DIR) / "persistence-test" / "room.json";
	const fs::path sourceScene = fs::path(ENTORNVGI_SOURCE_ASSETS_DIR) / "persistence-test" / "room.json";
	
	// Modelos ficticios necesarios para validar las rutas del JSON.
	escribir(runtimeScene.parent_path().parent_path() / "models" / "table.obj",
			"# test model\n");

	escribir(runtimeScene.parent_path().parent_path() / "models" / "door.glb",
			"# test animated model\n");
	escribir(runtimeScene.parent_path().parent_path() / "models" / "floor.png", "fixture texture");
	escribir(sourceScene.parent_path().parent_path() / "models" / "floor.png", "fixture texture");
	
	escribir(runtimeScene, original);
	escribir(sourceScene, original);

	editor::SerializadorEscenas serializadorConCopiaFuente;
	comprobar(serializadorConCopiaFuente.Cargar(runtimeScene, error), "carga una escena desde assets de ejecución");
	comprobar(serializadorConCopiaFuente.RutaFuente() == sourceScene.lexically_normal(),
		"resuelve la escena fuente correspondiente");
	comprobar(serializadorConCopiaFuente.Guardar({ datos.objetos[0], mesa }, error),
		"guarda en runtime y en la escena fuente");
	comprobar(leer(runtimeScene) == leer(sourceScene), "mantiene iguales las copias de runtime y fuente");
	std::error_code copiaEc;
	fs::remove_all(runtimeScene.parent_path(), copiaEc);
	if (copiaEc) { std::cerr << "No se pudo limpiar la escena runtime de prueba: " << copiaEc.message() << '\n'; ++fallos; }
	copiaEc.clear();
	fs::remove_all(sourceScene.parent_path(), copiaEc);
	if (copiaEc) { std::cerr << "No se pudo limpiar la escena fuente de prueba: " << copiaEc.message() << '\n'; ++fallos; }

	const std::string antesDelFallo = leer(escena);
	SceneObject invalida = mesa;
	invalida.escala.x = 0.0f;
	comprobar(!serializador.Guardar({ datos.objetos[0], invalida }, error), "rechaza escala cero antes de sustituir");
	comprobar(leer(escena) == antesDelFallo, "una validación fallida conserva el JSON original");

#ifdef _WIN32
	HANDLE bloqueo = CreateFileW(escena.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	comprobar(bloqueo != INVALID_HANDLE_VALUE, "abre bloqueo de prueba del archivo JSON");
	comprobar(!serializador.Guardar(datos.objetos, error), "un reemplazo bloqueado devuelve error");
	if (bloqueo != INVALID_HANDLE_VALUE) CloseHandle(bloqueo);
	comprobar(leer(escena) == antesDelFallo, "un reemplazo fallido conserva los bytes originales");
#endif

	std::error_code ec;
	fs::remove_all(temp, ec);
	if (ec) { std::cerr << "No se pudo limpiar la carpeta temporal de prueba: " << ec.message() << '\n'; ++fallos; }
	if (fallos == 0) std::cout << "Todas las pruebas de persistencia del editor pasaron.\n";
	return fallos == 0 ? 0 : 1;
}
