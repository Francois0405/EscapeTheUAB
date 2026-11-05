// ModeloGLB.cpp
// -----------------------------------------------------------------------------
// Carga de archivos .glb con cgltf (third_party/cgltf) y cálculo de poses.
// cgltf lee el archivo y nos da punteros a sus datos; aquí los copiamos a las
// estructuras sencillas de ModeloGLB.h y liberamos cgltf.
// -----------------------------------------------------------------------------
#define CGLTF_IMPLEMENTATION      // cgltf es de un solo archivo: se "implementa" aquí y en ningún otro sitio
#include "cgltf.h"

#include "ModeloGLB.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>   // glm::make_mat4

namespace glb
{
	namespace
	{
		// ------------------------------------------------------------------
		// Utilidades para leer datos de cgltf
		// ------------------------------------------------------------------

		const char* TextoDeResultado(cgltf_result resultado)
		{
			switch (resultado)
			{
			case cgltf_result_file_not_found:  return "archivo no encontrado";
			case cgltf_result_unknown_format:  return "no es un archivo glTF/GLB";
			case cgltf_result_invalid_json:    return "JSON interno invalido";
			case cgltf_result_invalid_gltf:    return "glTF invalido";
			case cgltf_result_data_too_short:  return "archivo incompleto";
			case cgltf_result_legacy_gltf:     return "glTF 1.0 antiguo (se necesita 2.0)";
			case cgltf_result_io_error:        return "error de lectura";
			default:                           return "error desconocido";
			}
		}

		// Copia todos los números de un accessor (una "columna" de datos del archivo).
		std::vector<float> LeerFlotantes(const cgltf_accessor* accesor)
		{
			const cgltf_size cantidad = cgltf_accessor_unpack_floats(accesor, nullptr, 0);
			std::vector<float> datos(cantidad);
			cgltf_accessor_unpack_floats(accesor, datos.data(), cantidad);
			return datos;
		}

		std::vector<glm::vec3> LeerVectores3(const cgltf_accessor* accesor)
		{
			const std::vector<float> datos = LeerFlotantes(accesor);
			std::vector<glm::vec3> vectores(datos.size() / 3);
			for (size_t i = 0; i < vectores.size(); ++i)
				vectores[i] = glm::vec3(datos[3 * i], datos[3 * i + 1], datos[3 * i + 2]);
			return vectores;
		}

		// ------------------------------------------------------------------
		// Mallas
		// ------------------------------------------------------------------

		// Si el archivo no trae normales las calculamos: la normal de cada vértice es la
		// suma de las normales de los triángulos que lo tocan (después se normaliza).
		void CalcularNormalesSiFaltan(PrimitivaGLB& primitiva)
		{
			primitiva.normales.assign(primitiva.posiciones.size(), glm::vec3(0.0f));
			for (size_t i = 0; i + 2 < primitiva.indices.size(); i += 3)
			{
				const unsigned int a = primitiva.indices[i];
				const unsigned int b = primitiva.indices[i + 1];
				const unsigned int c = primitiva.indices[i + 2];
				const glm::vec3 normalTriangulo = glm::cross(
					primitiva.posiciones[b] - primitiva.posiciones[a],
					primitiva.posiciones[c] - primitiva.posiciones[a]);
				primitiva.normales[a] += normalTriangulo;
				primitiva.normales[b] += normalTriangulo;
				primitiva.normales[c] += normalTriangulo;
			}
			for (glm::vec3& normal : primitiva.normales)
				if (glm::length(normal) > 0.0f) normal = glm::normalize(normal);
		}

		// Los índices dicen qué vértices forman cada triángulo. Si el archivo no los
		// trae, los triángulos son simplemente los vértices de tres en tres.
		void LeerIndices(const cgltf_primitive& origen, PrimitivaGLB& primitiva)
		{
			if (origen.indices != nullptr)
			{
				primitiva.indices.resize(origen.indices->count);
				cgltf_accessor_unpack_indices(origen.indices, primitiva.indices.data(),
					sizeof(unsigned int), primitiva.indices.size());
				return;
			}
			primitiva.indices.resize(primitiva.posiciones.size());
			for (size_t i = 0; i < primitiva.indices.size(); ++i)
				primitiva.indices[i] = static_cast<unsigned int>(i);
		}

		// Color del material. Solo usamos el "color base"; las texturas no se leen todavía.
		glm::vec4 LeerColorBase(const cgltf_material* material, std::vector<std::string>& avisos)
		{
			if (material == nullptr || !material->has_pbr_metallic_roughness)
				return glm::vec4(1.0f);   // por defecto en glTF: blanco

			const cgltf_pbr_metallic_roughness& pbr = material->pbr_metallic_roughness;
			if (pbr.base_color_texture.texture != nullptr)
				avisos.push_back("El material '" + std::string(material->name ? material->name : "?") +
					"' tiene textura: se usa solo su color base.");
			return glm::vec4(pbr.base_color_factor[0], pbr.base_color_factor[1],
				pbr.base_color_factor[2], pbr.base_color_factor[3]);
		}

		// Convierte una primitiva de cgltf en una PrimitivaGLB. Devuelve false si hay un error grave.
		bool LeerPrimitiva(const cgltf_primitive& origen, PrimitivaGLB& primitiva,
			std::vector<std::string>& avisos, std::string& error)
		{
			const cgltf_accessor* posiciones = nullptr;
			const cgltf_accessor* normales = nullptr;
			for (cgltf_size i = 0; i < origen.attributes_count; ++i)
			{
				if (origen.attributes[i].type == cgltf_attribute_type_position) posiciones = origen.attributes[i].data;
				if (origen.attributes[i].type == cgltf_attribute_type_normal)   normales = origen.attributes[i].data;
			}
			if (posiciones == nullptr) { error = "una primitiva no tiene posiciones"; return false; }

			primitiva.posiciones = LeerVectores3(posiciones);
			LeerIndices(origen, primitiva);

			for (unsigned int indice : primitiva.indices)
				if (indice >= primitiva.posiciones.size()) { error = "un indice apunta fuera de la malla"; return false; }

			if (normales != nullptr) primitiva.normales = LeerVectores3(normales);
			if (primitiva.normales.size() != primitiva.posiciones.size()) CalcularNormalesSiFaltan(primitiva);

			primitiva.colorBase = LeerColorBase(origen.material, avisos);
			return true;
		}

		bool LeerMallas(const cgltf_data& datos, ModeloGLB& modelo, std::string& error)
		{
			modelo.mallas.resize(datos.meshes_count);
			for (cgltf_size m = 0; m < datos.meshes_count; ++m)
			{
				const cgltf_mesh& origen = datos.meshes[m];
				modelo.mallas[m].nombre = origen.name ? origen.name : "";

				for (cgltf_size p = 0; p < origen.primitives_count; ++p)
				{
					if (origen.primitives[p].type != cgltf_primitive_type_triangles)
					{
						modelo.avisos.push_back("La malla '" + modelo.mallas[m].nombre + "' tiene una primitiva que no son triangulos: se ignora.");
						continue;
					}
					PrimitivaGLB primitiva;
					if (!LeerPrimitiva(origen.primitives[p], primitiva, modelo.avisos, error)) return false;
					modelo.mallas[m].primitivas.push_back(std::move(primitiva));
				}
			}
			return true;
		}

		// ------------------------------------------------------------------
		// Nodos
		// ------------------------------------------------------------------

		// Un nodo guarda su transformación como posición+rotación+escala, o bien como una
		// matriz de 4x4. Si es matriz la descomponemos para poder animar cada parte.
		// (No admite matrices con cizalla ni escala negativa; Blender no las exporta.)
		TransformacionLocal TransformacionDesdeMatriz(const float* m)
		{
			TransformacionLocal resultado;
			const glm::mat4 matriz = glm::make_mat4(m);
			resultado.posicion = glm::vec3(matriz[3]);
			resultado.escala = glm::vec3(glm::length(glm::vec3(matriz[0])),
				glm::length(glm::vec3(matriz[1])), glm::length(glm::vec3(matriz[2])));
			glm::mat3 giro(glm::vec3(matriz[0]) / resultado.escala.x,
				glm::vec3(matriz[1]) / resultado.escala.y, glm::vec3(matriz[2]) / resultado.escala.z);
			resultado.rotacion = glm::normalize(glm::quat_cast(giro));
			return resultado;
		}

		TransformacionLocal LeerTransformacion(const cgltf_node& nodo)
		{
			if (nodo.has_matrix) return TransformacionDesdeMatriz(nodo.matrix);

			TransformacionLocal resultado;
			if (nodo.has_translation)
				resultado.posicion = glm::vec3(nodo.translation[0], nodo.translation[1], nodo.translation[2]);
			if (nodo.has_rotation)   // el archivo guarda x,y,z,w
				resultado.rotacion = glm::normalize(glm::quat(nodo.rotation[3], nodo.rotation[0], nodo.rotation[1], nodo.rotation[2]));
			if (nodo.has_scale)
				resultado.escala = glm::vec3(nodo.scale[0], nodo.scale[1], nodo.scale[2]);
			return resultado;
		}

		void LeerNodos(const cgltf_data& datos, ModeloGLB& modelo)
		{
			modelo.nodos.resize(datos.nodes_count);
			for (cgltf_size i = 0; i < datos.nodes_count; ++i)
			{
				const cgltf_node& origen = datos.nodes[i];
				NodoGLB& nodo = modelo.nodos[i];
				nodo.nombre = origen.name ? origen.name : "";
				nodo.padre = origen.parent ? static_cast<int>(origen.parent - datos.nodes) : -1;
				nodo.indiceMalla = origen.mesh ? static_cast<int>(origen.mesh - datos.meshes) : -1;
				nodo.reposo = LeerTransformacion(origen);

				if (origen.skin != nullptr)
					modelo.avisos.push_back("El nodo '" + nodo.nombre + "' tiene esqueleto: las animaciones esqueleticas no estan soportadas.");
			}
		}

		// ------------------------------------------------------------------
		// Animaciones
		// ------------------------------------------------------------------

		bool PropiedadDesdeCgltf(cgltf_animation_path_type tipo, PropiedadAnimada& propiedad)
		{
			switch (tipo)
			{
			case cgltf_animation_path_type_translation: propiedad = PropiedadAnimada::Posicion; return true;
			case cgltf_animation_path_type_rotation:    propiedad = PropiedadAnimada::Rotacion; return true;
			case cgltf_animation_path_type_scale:       propiedad = PropiedadAnimada::Escala;   return true;
			default: return false;   // "weights" (morph targets): no soportado
			}
		}

		// Convierte los números sueltos del archivo en un vec4 por keyframe.
		std::vector<glm::vec4> AgruparValores(const std::vector<float>& numeros, size_t componentes)
		{
			std::vector<glm::vec4> valores(numeros.size() / componentes);
			for (size_t i = 0; i < valores.size(); ++i)
			{
				glm::vec4 valor(0.0f);
				for (size_t c = 0; c < componentes; ++c) valor[static_cast<int>(c)] = numeros[componentes * i + c];
				valores[i] = valor;
			}
			return valores;
		}

		// Lee un canal. Devuelve false si el canal no se puede reproducir (y explica por qué en 'error').
		bool LeerCanal(const cgltf_data& datos, const cgltf_animation_channel& origen,
			CanalAnimacion& canal, std::string& error)
		{
			if (!PropiedadDesdeCgltf(origen.target_path, canal.propiedad)) return true;   // se salta sin error
			if (origen.target_node == nullptr) return true;

			const cgltf_animation_sampler& muestreador = *origen.sampler;
			if (muestreador.interpolation == cgltf_interpolation_type_cubic_spline)
			{
				error = "la animacion usa interpolacion CUBICSPLINE (curvas Bezier). "
					"Exporta desde Blender con 'Always Sample Animations' activado "
					"(tools/exportar_glb.py ya lo hace).";
				return false;
			}
			canal.interpolacion = (muestreador.interpolation == cgltf_interpolation_type_step)
				? Interpolacion::Escalon : Interpolacion::Lineal;
			canal.indiceNodo = static_cast<int>(origen.target_node - datos.nodes);
			canal.tiempos = LeerFlotantes(muestreador.input);

			const size_t componentes = (canal.propiedad == PropiedadAnimada::Rotacion) ? 4 : 3;
			canal.valores = AgruparValores(LeerFlotantes(muestreador.output), componentes);
			if (canal.tiempos.empty() || canal.tiempos.size() != canal.valores.size())
			{
				error = "un canal de animacion tiene distinto numero de tiempos y de valores";
				return false;
			}
			return true;
		}

		// Blender exporta la animación empezando en el fotograma 1 (t = 1/24 s), no en 0.
		// Restamos el primer instante para que todas las animaciones empiecen en t = 0.
		void NormalizarTiempos(AnimacionGLB& animacion)
		{
			if (animacion.canales.empty()) return;
			float inicio = 1.0e30f;
			float fin = 0.0f;
			for (const CanalAnimacion& canal : animacion.canales)
			{
				inicio = std::min(inicio, canal.tiempos.front());
				fin = std::max(fin, canal.tiempos.back());
			}
			for (CanalAnimacion& canal : animacion.canales)
				for (float& tiempo : canal.tiempos) tiempo -= inicio;
			animacion.duracion = fin - inicio;
		}

		bool LeerAnimaciones(const cgltf_data& datos, ModeloGLB& modelo, std::string& error)
		{
			for (cgltf_size a = 0; a < datos.animations_count; ++a)
			{
				const cgltf_animation& origen = datos.animations[a];
				AnimacionGLB animacion;
				animacion.nombre = origen.name ? origen.name : "animacion";

				for (cgltf_size c = 0; c < origen.channels_count; ++c)
				{
					CanalAnimacion canal;
					if (!LeerCanal(datos, origen.channels[c], canal, error))
					{
						error = "animacion '" + animacion.nombre + "': " + error;
						return false;
					}
					if (canal.indiceNodo >= 0) animacion.canales.push_back(std::move(canal));
				}
				NormalizarTiempos(animacion);
				modelo.animaciones.push_back(std::move(animacion));
			}
			return true;
		}
	}

	// ----------------------------------------------------------------------
	// Carga
	// ----------------------------------------------------------------------

	bool CargarModeloGLB(const std::filesystem::path& ruta, ModeloGLB& modelo, std::string& error)
	{
		const std::string rutaTexto = ruta.string();
		cgltf_options opciones = {};
		cgltf_data* datos = nullptr;

		cgltf_result resultado = cgltf_parse_file(&opciones, rutaTexto.c_str(), &datos);
		if (resultado == cgltf_result_success) resultado = cgltf_load_buffers(&opciones, datos, rutaTexto.c_str());
		if (resultado == cgltf_result_success) resultado = cgltf_validate(datos);
		if (resultado != cgltf_result_success)
		{
			error = rutaTexto + ": " + TextoDeResultado(resultado);
			if (datos != nullptr) cgltf_free(datos);
			return false;
		}

		// Se rellena un modelo temporal: si algo falla, el modelo recibido no se toca.
		ModeloGLB nuevo;
		LeerNodos(*datos, nuevo);
		const bool correcto = LeerMallas(*datos, nuevo, error) && LeerAnimaciones(*datos, nuevo, error);
		cgltf_free(datos);

		if (!correcto)
		{
			error = rutaTexto + ": " + error;
			return false;
		}
		modelo = std::move(nuevo);
		return true;
	}

	// ----------------------------------------------------------------------
	// Jerarquía y matrices
	// ----------------------------------------------------------------------

	glm::mat4 MatrizLocal(const TransformacionLocal& transformacion)
	{
		glm::mat4 matriz(1.0f);
		matriz = glm::translate(matriz, transformacion.posicion);   // 3. mover
		matriz = matriz * glm::mat4_cast(transformacion.rotacion);  // 2. girar
		matriz = glm::scale(matriz, transformacion.escala);         // 1. escalar (se aplica primero a los vértices)
		return matriz;
	}

	glm::mat4 MatrizMundoDelNodo(const ModeloGLB& modelo, const PoseGLB& pose, int indiceNodo)
	{
		glm::mat4 mundo(1.0f);
		size_t pasos = 0;
		// Subimos del nodo a su padre, al padre del padre... y vamos multiplicando por la izquierda.
		// El límite de pasos evita un bucle infinito si un archivo corrupto tuviera un ciclo.
		while (indiceNodo >= 0 && pasos++ <= modelo.nodos.size())
		{
			mundo = MatrizLocal(pose.nodos[indiceNodo]) * mundo;
			indiceNodo = modelo.nodos[indiceNodo].padre;
		}
		return mundo;
	}

	int BuscarNodoPorNombre(const ModeloGLB& modelo, const std::string& nombre)
	{
		for (size_t i = 0; i < modelo.nodos.size(); ++i)
			if (modelo.nodos[i].nombre == nombre) return static_cast<int>(i);
		return -1;
	}

	glm::vec3 CalcularCentroDelModelo(const ModeloGLB& modelo, const PoseGLB& pose)
	{
		glm::vec3 minimo(1.0e30f);
		glm::vec3 maximo(-1.0e30f);
		for (size_t i = 0; i < modelo.nodos.size(); ++i)
		{
			if (modelo.nodos[i].indiceMalla < 0) continue;
			const glm::mat4 mundo = MatrizMundoDelNodo(modelo, pose, static_cast<int>(i));
			for (const PrimitivaGLB& primitiva : modelo.mallas[modelo.nodos[i].indiceMalla].primitivas)
				for (const glm::vec3& vertice : primitiva.posiciones)
				{
					const glm::vec3 punto = glm::vec3(mundo * glm::vec4(vertice, 1.0f));
					minimo = glm::min(minimo, punto);
					maximo = glm::max(maximo, punto);
				}
		}
		if (minimo.x > maximo.x) return glm::vec3(0.0f);   // modelo sin mallas
		return 0.5f * (minimo + maximo);
	}

	glm::mat4 MatrizYArribaAZArriba()
	{
		return glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	// ----------------------------------------------------------------------
	// Animación
	// ----------------------------------------------------------------------

	PoseGLB CrearPoseDeReposo(const ModeloGLB& modelo)
	{
		PoseGLB pose;
		RestaurarPoseDeReposo(modelo, pose);
		return pose;
	}

	void RestaurarPoseDeReposo(const ModeloGLB& modelo, PoseGLB& pose)
	{
		pose.nodos.resize(modelo.nodos.size());
		for (size_t i = 0; i < modelo.nodos.size(); ++i) pose.nodos[i] = modelo.nodos[i].reposo;
	}

	void AplicarAnimacion(PoseGLB& pose, const AnimacionGLB& animacion, float tiempo)
	{
		for (const CanalAnimacion& canal : animacion.canales)
		{
			if (canal.indiceNodo < 0 || canal.indiceNodo >= static_cast<int>(pose.nodos.size())) continue;

			const glm::vec4 valor = ValorDelCanalEnTiempo(canal, tiempo);
			TransformacionLocal& nodo = pose.nodos[canal.indiceNodo];
			switch (canal.propiedad)
			{
			case PropiedadAnimada::Posicion: nodo.posicion = glm::vec3(valor); break;
			case PropiedadAnimada::Escala:   nodo.escala = glm::vec3(valor); break;
			case PropiedadAnimada::Rotacion: nodo.rotacion = CuaternionDesdeVec4(valor); break;
			}
		}
	}
}
