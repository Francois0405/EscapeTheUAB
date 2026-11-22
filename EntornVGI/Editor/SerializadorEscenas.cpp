#include "SerializadorEscenas.h"

#include "../third_party/nlohmann/json.hpp"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

namespace editor
{
	namespace
	{
		using nlohmann::json;
		std::atomic<unsigned long long> contadorTemporal{ 0 };

		bool VectorValido(const glm::vec3& vector, bool escala)
		{
			for (int eje = 0; eje < 3; ++eje)
				if (!std::isfinite(vector[eje]) || (escala && vector[eje] == 0.0f)) return false;
			return true;
		}

		json VectorJSON(const glm::vec3& vector)
		{
			return json::array({ vector.x, vector.y, vector.z });
		}

		bool ReemplazarArchivo(const std::filesystem::path& temporal, const std::filesystem::path& destino,
			std::string& error)
		{
#ifdef _WIN32
			if (!MoveFileExW(temporal.c_str(), destino.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				error = "No se pudo sustituir el JSON de escena (error de Windows " + std::to_string(GetLastError()) + ").";
				return false;
			}
#else
			std::error_code ec;
			std::filesystem::rename(temporal, destino, ec);
			if (ec)
			{
				error = "No se pudo sustituir el JSON de escena: " + ec.message();
				return false;
			}
#endif
			return true;
		}

		std::filesystem::path RutaFuenteCorrespondiente(const std::filesystem::path& rutaEscena)
		{
#if defined(ENTORNVGI_RUNTIME_ASSETS_DIR) && defined(ENTORNVGI_SOURCE_ASSETS_DIR)
			const std::filesystem::path runtimeAssets(ENTORNVGI_RUNTIME_ASSETS_DIR);
			const std::filesystem::path sourceAssets(ENTORNVGI_SOURCE_ASSETS_DIR);
			const auto relativa = rutaEscena.lexically_relative(runtimeAssets.lexically_normal());
			if (relativa.empty() || relativa.is_absolute()) return {};
			for (const auto& componente : relativa)
				if (componente == "..") return {};
			return (sourceAssets / relativa).lexically_normal();
#else
			(void)rutaEscena;
			return {};
#endif
		}

		bool GuardarCopiaAtomica(const std::filesystem::path& destino, const std::string& contenido,
			std::string& error)
		{
			const auto secuencia = contadorTemporal.fetch_add(1, std::memory_order_relaxed);
			const auto instante = std::chrono::steady_clock::now().time_since_epoch().count();
			std::filesystem::path temporal = destino;
			temporal += ".tmp-" + std::to_string(instante) + "-" + std::to_string(secuencia);
			{
				std::ofstream archivo(temporal, std::ios::binary | std::ios::trunc);
				if (!archivo)
				{
					error = "No se pudo crear el archivo temporal de la copia fuente.";
					return false;
				}
				archivo.write(contenido.data(), static_cast<std::streamsize>(contenido.size()));
				archivo.flush();
				if (!archivo)
				{
					archivo.close();
					std::error_code ec;
					std::filesystem::remove(temporal, ec);
					error = "Falló la escritura temporal de la copia fuente.";
					return false;
				}
			}
			if (!ReemplazarArchivo(temporal, destino, error))
			{
				std::error_code ec;
				std::filesystem::remove(temporal, ec);
				return false;
			}
			return true;
		}
	}

	bool SerializadorEscenas::Cargar(const std::filesystem::path& rutaEntrada, std::string& error)
	{
		DatosEscena validacion;
		if (!CargarDatosEscenaJSON(rutaEntrada, validacion, error)) return false;
		std::ifstream archivo(rutaEntrada, std::ios::binary);
		if (!archivo)
		{
			error = "No se pudo abrir el JSON para preparar su persistencia.";
			return false;
		}
		std::string contenido((std::istreambuf_iterator<char>(archivo)), std::istreambuf_iterator<char>());
		if (!archivo.good() && !archivo.eof())
		{
			error = "No se pudo leer completamente el JSON de escena.";
			return false;
		}
		std::error_code ec;
		auto rutaAbsoluta = std::filesystem::absolute(rutaEntrada, ec).lexically_normal();
		if (ec)
		{
			error = "No se pudo resolver la ruta activa de escena: " + ec.message();
			return false;
		}
		ruta = std::move(rutaAbsoluta);
		rutaFuente = RutaFuenteCorrespondiente(ruta);
		documentoOriginal = std::move(contenido);
		cargado = true;
		error.clear();
		return true;
	}

	bool SerializadorEscenas::Guardar(const std::vector<SceneObject>& objetos, std::string& error)
	{
		llamadaSinListaAnimados = true;
		return Guardar(objetos, {}, error);
	}

	bool SerializadorEscenas::Guardar(const std::vector<SceneObject>& objetos, const std::vector<SceneObjetoAnimado>& animados,
		std::string& error)
	{
		const bool preservarAnimated = llamadaSinListaAnimados;
		llamadaSinListaAnimados = false;
		if (!cargado)
		{
			error = "No hay una escena cargada para guardar.";
			return false;
		}

		std::unordered_map<std::string, const SceneObject*> porId;
		for (const SceneObject& objeto : objetos)
		{
			if (objeto.id.empty() || !porId.emplace(objeto.id, &objeto).second ||
				!VectorValido(objeto.posicion, false) || !VectorValido(objeto.rotacion, false) || !VectorValido(objeto.escala, true))
			{
				error = "La escena contiene un ID duplicado o una transformacion no finita/invalida.";
				return false;
			}
			if (objeto.luz.presente && (!VectorValido(objeto.luz.desplazamiento, false) ||
				!VectorValido(objeto.luz.color, false) || objeto.luz.color.x < 0.0f || objeto.luz.color.y < 0.0f || objeto.luz.color.z < 0.0f ||
				objeto.luz.color.x > 1.0f || objeto.luz.color.y > 1.0f || objeto.luz.color.z > 1.0f ||
				!std::isfinite(objeto.luz.intensidad) || objeto.luz.intensidad <= 0.0f || objeto.luz.intensidad > 16.0f ||
				!std::isfinite(objeto.luz.areaSize.x) || !std::isfinite(objeto.luz.areaSize.y) ||
				objeto.luz.areaSize.x < 0.0f || objeto.luz.areaSize.y < 0.0f ||
				objeto.luz.areaSize.x > 10.0f || objeto.luz.areaSize.y > 10.0f ||
				((objeto.luz.areaSize.x == 0.0f) != (objeto.luz.areaSize.y == 0.0f))))
			{
				error = "La instancia '" + objeto.id + "' tiene parametros de luz invalidos.";
				return false;
			}
			if (objeto.material.presente)
			{
				const auto& m = objeto.material;
				const std::filesystem::path textura(m.textura);
				if (!std::isfinite(m.tileSize.x) || !std::isfinite(m.tileSize.y) ||
					m.tileSize.x < 0.1f || m.tileSize.x > 20.0f || m.tileSize.y < 0.1f || m.tileSize.y > 20.0f ||
					!std::isfinite(m.roughness) || m.roughness < 0.0f || m.roughness > 1.0f ||
					!std::isfinite(m.dirtStrength) || m.dirtStrength < 0.0f || m.dirtStrength > 0.5f ||
					!std::isfinite(m.dirtScale) || m.dirtScale < 0.05f || m.dirtScale > 4.0f || !std::isfinite(m.dirtSeed) || textura.is_absolute() ||
					(!m.textura.empty() && !std::filesystem::is_regular_file(ruta.parent_path() / textura)))
				{
					error = "La instancia '" + objeto.id + "' tiene una textura o parametros de material invalidos.";
					return false;
				}
			}
		}

		json documento;
		try
		{
			documento = json::parse(documentoOriginal);
			std::unordered_set<std::string> escritos;
			for (const char* coleccion : { "architecture", "objects" })
			{
				auto& lista = documento.at(coleccion);
				for (auto entrada = lista.begin(); entrada != lista.end();)
				{
					const std::string id = entrada->at("id").get<std::string>();
					const auto encontrado = porId.find(id);
					if (encontrado == porId.end()) { entrada = lista.erase(entrada); continue; }
					const SceneObject& objeto = *encontrado->second;
					(*entrada)["position"] = VectorJSON(objeto.posicion);
					(*entrada)["rotation"] = VectorJSON(objeto.rotacion);
					(*entrada)["scale"] = VectorJSON(objeto.escala);
					if (objeto.permitirPenetracionArquitectura) (*entrada)["allowArchitecturePenetration"] = true;
					else entrada->erase("allowArchitecturePenetration");
					if (objeto.luz.presente)
					{
						json luz = { {"enabled", objeto.luz.encendida}, {"offset", VectorJSON(objeto.luz.desplazamiento)},
							{"color", VectorJSON(objeto.luz.color)}, {"intensity", objeto.luz.intensidad} };
						if (objeto.luz.areaSize.x > 0.0f)
							luz["areaSize"] = { objeto.luz.areaSize.x, objeto.luz.areaSize.y };
						(*entrada)["light"] = std::move(luz);
					}
					else entrada->erase("light");
					if (objeto.material.presente)
					{
						json material = { {"tileSize", {objeto.material.tileSize.x, objeto.material.tileSize.y}},
							{"roughness", objeto.material.roughness}, {"dirtStrength", objeto.material.dirtStrength},
							{"dirtScale", objeto.material.dirtScale}, {"dirtSeed", objeto.material.dirtSeed} };
						if (!objeto.material.textura.empty()) material["texture"] = objeto.material.textura;
						(*entrada)["material"] = std::move(material);
					}
					else entrada->erase("material");
					if (objeto.pared.presente)
					{
						json aperturas = json::array();
						for (const auto& apertura : objeto.pared.aperturas)
			aperturas.push_back({ {"id", apertura.id}, {"doorId", apertura.idPuerta}, {"centerU", apertura.centerU},
								{"width", apertura.ancho}, {"height", apertura.alto}, {"base", apertura.base} });
						(*entrada)["wall"] = {{"apertures", std::move(aperturas)}};
					}
					else entrada->erase("wall");
					escritos.insert(id);
					++entrada;
				}
			}
			if (!preservarAnimated)
			{
				json reconciliados = json::array();
				std::unordered_set<std::string> animadosEscritos;
				if (documento.contains("animated") && documento["animated"].is_array())
				for (auto entrada : documento["animated"])
				{
					const std::string id = entrada.at("id").get<std::string>();
					const auto actual = std::find_if(animados.begin(), animados.end(),
						[&](const SceneObjetoAnimado& item) { return item.id == id; });
					if (actual == animados.end()) continue; // deleted instance, even when this was the last GLB
					const SceneObjetoAnimado& animado = *actual;
					if (!VectorValido(animado.posicion, false) || !std::isfinite(animado.giroZGrados) ||
						!std::isfinite(animado.escala) || animado.escala <= 0.0f)
					{
						error = "La puerta '" + id + "' tiene una transformacion no finita/invalida.";
						return false;
					}
					entrada["position"] = VectorJSON(animado.posicion);
					entrada["rotation"] = VectorJSON(glm::vec3(0.0f, 0.0f, animado.giroZGrados));
					if (animado.permitirPenetracionArquitectura) entrada["allowArchitecturePenetration"] = true;
					else entrada.erase("allowArchitecturePenetration");
					if (entrada.contains("scale") || animado.escala != 1.0f) entrada["scale"] = animado.escala;
					if (!animado.paredId.empty()) { entrada["wallId"] = animado.paredId; entrada["openingId"] = animado.aperturaId; }
					else { entrada.erase("wallId"); entrada.erase("openingId"); }
					animadosEscritos.insert(id);
					reconciliados.push_back(std::move(entrada));
				}
				for (const SceneObjetoAnimado& animado : animados)
				{
					if (animadosEscritos.contains(animado.id)) continue;
					if (animado.id.empty() || !VectorValido(animado.posicion, false) || !std::isfinite(animado.giroZGrados) ||
						!std::isfinite(animado.escala) || animado.escala <= 0.0f)
					{ error = "La puerta nueva contiene un ID o una transformacion invalida."; return false; }
					std::error_code rutaError;
					const std::filesystem::path modelo(animado.modelo);
					const auto absoluta = std::filesystem::absolute(modelo.is_absolute() ? modelo : ruta.parent_path() / modelo, rutaError).lexically_normal();
					if (rutaError || !std::filesystem::is_regular_file(absoluta, rutaError))
					{ error = "El GLB de '" + animado.id + "' no existe."; return false; }
					const auto relativa = absoluta.lexically_relative(ruta.parent_path());
					if (relativa.empty() || relativa.is_absolute()) { error = "No se pudo crear una ruta relativa para el GLB."; return false; }
					json nueva = {{"id", animado.id}, {"model", relativa.generic_string()},
						{"position", VectorJSON(animado.posicion)}, {"rotation", VectorJSON(glm::vec3(0, 0, animado.giroZGrados))},
						{"scale", animado.escala}};
					if (!animado.animacion.empty()) nueva["animation"] = animado.animacion;
					if (!animado.paredId.empty()) { nueva["wallId"] = animado.paredId; nueva["openingId"] = animado.aperturaId; }
					if (animado.permitirPenetracionArquitectura) nueva["allowArchitecturePenetration"] = true;
					json colisionadores = json::array();
					for (const SceneColisionador& c : animado.colisionadores)
						colisionadores.push_back({ {"node", c.nodo}, {"type", c.tipo == fisica::TipoCuerpo::Cinematico ? "kinematic" : "static"},
							{"halfSize", VectorJSON(c.mitadTamano)}, {"offset", VectorJSON(c.desplazamiento)} });
					if (!colisionadores.empty()) nueva["colliders"] = std::move(colisionadores);
					reconciliados.push_back(std::move(nueva));
				}
				documento["animated"] = std::move(reconciliados);
			}
			for (const SceneObject& objeto : objetos)
			{
				if (escritos.contains(objeto.id)) continue;
				if (objeto.categoria != "objects")
				{
					error = "Solo se pueden crear instancias nuevas en la colección 'objects'.";
					return false;
				}
				std::error_code rutaError;
				const std::filesystem::path modeloPath(objeto.modelo);
				const auto modeloEntrada = modeloPath.is_absolute() ? modeloPath : ruta.parent_path() / modeloPath;
				const auto modeloAbsoluto = std::filesystem::absolute(modeloEntrada, rutaError).lexically_normal();
				if (rutaError) { error = "No se pudo resolver la ruta del nuevo modelo."; return false; }
				const auto modeloRelativo = modeloAbsoluto.lexically_relative(ruta.parent_path());
				if (modeloRelativo.empty() || modeloRelativo.is_absolute())
				{
					error = "No se pudo construir una ruta de modelo relativa al JSON.";
					return false;
				}
				json nueva = {
					{"id", objeto.id}, {"model", modeloRelativo.generic_string()},
					{"position", VectorJSON(objeto.posicion)}, {"rotation", VectorJSON(objeto.rotacion)},
					{"scale", VectorJSON(objeto.escala)}
				};
				if (objeto.permitirPenetracionArquitectura) nueva["allowArchitecturePenetration"] = true;
				if (objeto.luz.presente)
				{
					json luz = { {"enabled", objeto.luz.encendida}, {"offset", VectorJSON(objeto.luz.desplazamiento)},
						{"color", VectorJSON(objeto.luz.color)}, {"intensity", objeto.luz.intensidad} };
					if (objeto.luz.areaSize.x > 0.0f)
						luz["areaSize"] = { objeto.luz.areaSize.x, objeto.luz.areaSize.y };
					nueva["light"] = std::move(luz);
				}
				if (objeto.material.presente)
				{
					json material = { {"tileSize", {objeto.material.tileSize.x, objeto.material.tileSize.y}},
						{"roughness", objeto.material.roughness}, {"dirtStrength", objeto.material.dirtStrength},
						{"dirtScale", objeto.material.dirtScale}, {"dirtSeed", objeto.material.dirtSeed} };
					if (!objeto.material.textura.empty()) material["texture"] = objeto.material.textura;
					nueva["material"] = std::move(material);
				}
				if (objeto.fisica.presente)
				{
					json fisica = {
						{"type", objeto.fisica.tipo == fisica::TipoCuerpo::Dinamico ? "dynamic" : "static"},
						{"halfSize", VectorJSON(objeto.fisica.mitadTamano)},
						{"offset", VectorJSON(objeto.fisica.desplazamiento)}
					};
					if (objeto.fisica.tipo == fisica::TipoCuerpo::Dinamico) fisica["mass"] = objeto.fisica.masa;
					nueva["physics"] = std::move(fisica);
				}
				documento["objects"].push_back(std::move(nueva));
			}
		}
		catch (const std::exception& excepcion)
		{
			error = std::string("No se pudo preparar el JSON conservando sus campos: ") + excepcion.what();
			return false;
		}

		const auto secuencia = contadorTemporal.fetch_add(1, std::memory_order_relaxed);
		const auto instante = std::chrono::steady_clock::now().time_since_epoch().count();
		const std::string serializado = documento.dump(2) + "\n";
		std::filesystem::path temporal = ruta;
		temporal += ".tmp-" + std::to_string(instante) + "-" + std::to_string(secuencia);
		{
			std::ofstream archivo(temporal, std::ios::binary | std::ios::trunc);
			if (!archivo)
			{
				error = "No se pudo crear el archivo temporal junto al JSON.";
				return false;
			}
			archivo.write(serializado.data(), static_cast<std::streamsize>(serializado.size()));
			archivo.flush();
			if (!archivo)
			{
				archivo.close();
				std::error_code ec;
				std::filesystem::remove(temporal, ec);
				error = "Falló la escritura del archivo temporal; el JSON original sigue intacto.";
				return false;
			}
		}

		DatosEscena comprobacion;
		if (!CargarDatosEscenaJSON(temporal, comprobacion, error))
		{
			std::error_code ec;
			std::filesystem::remove(temporal, ec);
			error = "El JSON temporal no superó la validacion: " + error;
			return false;
		}

		std::filesystem::path copia = ruta;
		copia += ".bak";
		for (unsigned int sufijo = 1; std::filesystem::exists(copia); ++sufijo)
		{
			copia = ruta;
			copia += ".bak." + std::to_string(sufijo);
		}
		std::error_code ec;
		std::filesystem::copy_file(ruta, copia, std::filesystem::copy_options::none, ec);
		if (ec)
		{
			std::filesystem::remove(temporal, ec);
			error = "No se pudo crear la copia de seguridad; no se sustituyó el JSON original.";
			return false;
		}
		if (!ReemplazarArchivo(temporal, ruta, error))
		{
			std::filesystem::remove(temporal, ec);
			return false;
		}
		documentoOriginal = serializado;
		if (!rutaFuente.empty() && !GuardarCopiaAtomica(rutaFuente, serializado, error))
		{
			error = "La copia de ejecucion se guardo, pero no se pudo actualizar el archivo fuente: " + error;
			return false;
		}
		error.clear();
		return true;
	}
}
