#include "Editor/SerializadorEscenas.h"
#include "SceneLoader.h"
#include "third_party/nlohmann/json.hpp"

#include <chrono>
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
	const fs::path escena = temp / "scenes" / "room.json";
	escribir(temp / "models" / "table.obj", "# test model\n");
	escribir(temp / "models" / "door.glb", "# test animated model\n");
	const std::string original = R"json({
  "version": 1,
  "name": "persistence-test",
  "architecture": [
    {"id":"floor","model":"../models/table.obj","position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}
  ],
  "objects": [
    {"id":"table","model":"../models/table.obj","position":[1,2,3],"rotation":[0,0,0],"scale":[1,1,1],
     "physics":{"type":"static","halfSize":[0.5,0.4,0.3],"offset":[0.1,0.2,0.3]}}
  ],
  "animated": [
    {"id":"door","model":"../models/door.glb","position":[4,5,6],"rotation":[0,0,90],"scale":1.0,
     "colliders":[{"node":"Door","type":"kinematic","halfSize":[0.4,1.0,0.1],"offset":[0,0,0]}]}
  ]
})json";
	escribir(escena, original);

	std::string error;
	editor::SerializadorEscenas serializador;
	comprobar(serializador.Cargar(escena, error), "carga el documento JSON de la escena");
	DatosEscena datos;
	comprobar(CargarDatosEscenaJSON(escena, datos, error), "fixture JSON válido");
	SceneObject mesa = datos.objetos.at(1);
	mesa.posicion = glm::vec3(7.25f, -3.5f, 1.75f);
	mesa.rotacion = glm::vec3(15.0f, -20.0f, 90.0f);
	mesa.escala = glm::vec3(1.25f, 0.75f, 2.0f);
	datos.objetos[1] = mesa;
	comprobar(serializador.Guardar(datos.objetos, error), "guarda transformaciones válidas");
	comprobar(fs::exists(escena.string() + ".bak"), "crea una copia de seguridad antes de reemplazar");

	nlohmann::json guardado = nlohmann::json::parse(leer(escena));
	const auto& guardada = guardado["objects"][0];
	comprobar(guardada["model"] == "../models/table.obj", "preserva la ruta relativa original");
	comprobar(guardada["position"] == nlohmann::json::array({ 7.25, -3.5, 1.75 }), "persiste posición");
	comprobar(guardada["rotation"] == nlohmann::json::array({ 15.0, -20.0, 90.0 }), "persiste rotación");
	comprobar(guardada["scale"] == nlohmann::json::array({ 1.25, 0.75, 2.0 }), "persiste escala");
	comprobar(guardada["physics"]["halfSize"] == nlohmann::json::array({ 0.5, 0.4, 0.3 }), "preserva halfSize físico");
	comprobar(guardada["physics"]["offset"] == nlohmann::json::array({ 0.1, 0.2, 0.3 }), "preserva offset físico");
	comprobar(guardado["animated"][0]["colliders"][0]["type"] == "kinematic", "preserva colisionador animado");
	comprobar(guardado["architecture"][0]["position"] == nlohmann::json::array({ 0, 0, 0 }), "preserva instancias no editadas");
	DatosEscena recargada;
	comprobar(CargarDatosEscenaJSON(escena, recargada, error), "recarga JSON editado");
	comprobar(recargada.objetos[1].posicion == mesa.posicion && recargada.objetos[1].rotacion == mesa.rotacion &&
		recargada.objetos[1].escala == mesa.escala, "la recarga conserva transformaciones exactas");
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
