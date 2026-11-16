#include "SerializadorEscenas.h"

#include "../third_party/nlohmann/json.hpp"

#include <atomic>
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
		documentoOriginal = std::move(contenido);
		cargado = true;
		error.clear();
		return true;
	}

	bool SerializadorEscenas::Guardar(const std::vector<SceneObject>& objetos, std::string& error)
	{
		return Guardar(objetos, {}, error);
	}

	bool SerializadorEscenas::Guardar(const std::vector<SceneObject>& objetos, const std::vector<SceneObjetoAnimado>& animados,
		std::string& error)
	{
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
					escritos.insert(id);
					++entrada;
				}
			}
			if (!animados.empty() && documento.contains("animated") && documento["animated"].is_array())
			{
				for (auto& entrada : documento["animated"])
				{
					const std::string id = entrada.at("id").get<std::string>();
					for (const SceneObjetoAnimado& animado : animados)
					{
						if (animado.id != id) continue;
						if (!VectorValido(animado.posicion, false) || !std::isfinite(animado.giroZGrados) ||
							!std::isfinite(animado.escala) || animado.escala <= 0.0f)
						{
							error = "La puerta '" + id + "' tiene una transformacion no finita/invalida.";
							return false;
						}
						entrada["position"] = VectorJSON(animado.posicion);
						entrada["rotation"] = VectorJSON(glm::vec3(0.0f, 0.0f, animado.giroZGrados));
						if (entrada.contains("scale") || animado.escala != 1.0f) entrada["scale"] = animado.escala;
						break;
					}
				}
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
		std::filesystem::path temporal = ruta;
		temporal += ".tmp-" + std::to_string(instante) + "-" + std::to_string(secuencia);
		{
			std::ofstream archivo(temporal, std::ios::binary | std::ios::trunc);
			if (!archivo)
			{
				error = "No se pudo crear el archivo temporal junto al JSON.";
				return false;
			}
			const std::string serializado = documento.dump(2) + "\n";
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
		documentoOriginal = documento.dump(2) + "\n";
		error.clear();
		return true;
	}
}
