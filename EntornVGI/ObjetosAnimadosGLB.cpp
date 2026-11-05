// ObjetosAnimadosGLB.cpp
#include "ObjetosAnimadosGLB.h"
#include <glm/gtc/matrix_transform.hpp>

namespace glb
{
	namespace
	{
		// Devuelve el modelo de ese archivo, cargándolo si aún no está. nullptr si falla.
		const ModeloGLB* ObtenerModelo(EscenaAnimada& escena, const std::string& rutaGlb, std::string& error)
		{
			const auto existente = escena.modelos.find(rutaGlb);
			if (existente != escena.modelos.end()) return &existente->second;

			ModeloGLB nuevo;
			if (!CargarModeloGLB(rutaGlb, nuevo, error)) return nullptr;
			return &escena.modelos.emplace(rutaGlb, std::move(nuevo)).first->second;
		}

		bool TieneAnimacion(const ObjetoAnimado& objeto)
		{
			return objeto.modelo != nullptr && objeto.indiceAnimacion >= 0 &&
				objeto.indiceAnimacion < static_cast<int>(objeto.modelo->animaciones.size());
		}

		// Cuando el reloj se detiene, "Abriendo" pasa a "Abierta" y "Cerrando" a "Cerrada".
		void PasarAlEstadoFinalSiHaTerminado(ObjetoAnimado& objeto)
		{
			if (objeto.reproductor.reproduciendo) return;
			if (objeto.estado == EstadoPuerta::Abriendo) objeto.estado = EstadoPuerta::Abierta;
			if (objeto.estado == EstadoPuerta::Cerrando) objeto.estado = EstadoPuerta::Cerrada;
		}

		// Coseno del ángulo entre la dirección de mirada y la dirección hacia un punto.
		// 1 = justo enfrente, 0 = a 90 grados, negativo = a la espalda.
		float CosenoHaciaElPunto(const glm::vec3& origen, const glm::vec3& direccionUnitaria, const glm::vec3& punto)
		{
			const glm::vec3 haciaElPunto = punto - origen;
			const float distancia = glm::length(haciaElPunto);
			if (distancia < 1.0e-6f) return 1.0f;   // el punto está justo en el origen
			return glm::dot(direccionUnitaria, haciaElPunto / distancia);
		}
	}

	bool AnadirObjeto(EscenaAnimada& escena, const std::string& id, const std::string& rutaGlb,
		const glm::vec3& posicion, float giroZGrados, std::string& error)
	{
		if (BuscarObjeto(escena, id) != nullptr)
		{
			error = "ya existe un objeto animado con el id '" + id + "'";
			return false;
		}
		const ModeloGLB* modelo = ObtenerModelo(escena, rutaGlb, error);
		if (modelo == nullptr) return false;

		ObjetoAnimado objeto;
		objeto.id = id;
		objeto.modelo = modelo;
		objeto.posicion = posicion;
		objeto.giroZGrados = giroZGrados;
		objeto.pose = CrearPoseDeReposo(*modelo);   // cada instancia empieza con su pose propia
		objeto.centroDelModelo = CalcularCentroDelModelo(*modelo, objeto.pose);
		if (TieneAnimacion(objeto))   // reloj parado en t = 0: la puerta empieza cerrada
			objeto.reproductor.duracion = modelo->animaciones[objeto.indiceAnimacion].duracion;
		escena.objetos.push_back(std::move(objeto));
		return true;
	}

	bool SeleccionarAnimacion(ObjetoAnimado& objeto, const std::string& nombre, std::string& error)
	{
		if (objeto.modelo == nullptr) return false;
		int indice = 0;   // sin nombre: la primera animación
		if (!nombre.empty())
		{
			indice = -1;
			for (std::size_t i = 0; i < objeto.modelo->animaciones.size(); ++i)
				if (objeto.modelo->animaciones[i].nombre == nombre) { indice = static_cast<int>(i); break; }
			if (indice < 0)
			{
				error = "el modelo de '" + objeto.id + "' no tiene la animacion '" + nombre + "'";
				return false;
			}
		}
		objeto.indiceAnimacion = indice;
		if (TieneAnimacion(objeto))   // reloj parado en t = 0, con la duración de la animación elegida
			objeto.reproductor.duracion = objeto.modelo->animaciones[indice].duracion;
		return true;
	}

	ObjetoAnimado* BuscarObjeto(EscenaAnimada& escena, const std::string& id)
	{
		for (ObjetoAnimado& objeto : escena.objetos)
			if (objeto.id == id) return &objeto;
		return nullptr;
	}

	glm::mat4 MatrizBaseDelObjeto(const ObjetoAnimado& objeto)
	{
		const glm::mat4 colocar = glm::translate(glm::mat4(1.0f), objeto.posicion);
		const glm::mat4 girar = glm::rotate(colocar, glm::radians(objeto.giroZGrados), glm::vec3(0.0f, 0.0f, 1.0f));
		return glm::scale(girar, glm::vec3(objeto.escala));   // trasladar * girar * escalar
	}

	void IniciarAnimacion(ObjetoAnimado& objeto)
	{
		if (!TieneAnimacion(objeto)) return;
		RestaurarPoseDeReposo(*objeto.modelo, objeto.pose);
		objeto.reproductor.Iniciar(objeto.modelo->animaciones[objeto.indiceAnimacion].duracion);
		objeto.estado = EstadoPuerta::Abriendo;
	}

	void AlternarPuerta(ObjetoAnimado& objeto)
	{
		if (!TieneAnimacion(objeto)) return;
		switch (objeto.estado)
		{
		case EstadoPuerta::Cerrada:
		case EstadoPuerta::Cerrando:    // si se estaba cerrando, vuelve a abrirse desde donde esté
			objeto.estado = EstadoPuerta::Abriendo;
			objeto.reproductor.Reproducir(+1.0f);
			break;
		case EstadoPuerta::Abierta:
		case EstadoPuerta::Abriendo:    // si se estaba abriendo, empieza a cerrarse desde donde esté
			objeto.estado = EstadoPuerta::Cerrando;
			objeto.reproductor.Reproducir(-1.0f);
			break;
		}
	}

	const char* NombreDelEstado(EstadoPuerta estado)
	{
		switch (estado)
		{
		case EstadoPuerta::Cerrada:  return "Cerrada";
		case EstadoPuerta::Abriendo: return "Abriendo";
		case EstadoPuerta::Abierta:  return "Abierta";
		case EstadoPuerta::Cerrando: return "Cerrando";
		}
		return "?";
	}

	void ActualizarObjetos(EscenaAnimada& escena, float deltaSegundos)
	{
		for (ObjetoAnimado& objeto : escena.objetos)
		{
			if (!TieneAnimacion(objeto)) continue;
			objeto.reproductor.Actualizar(deltaSegundos);
			PasarAlEstadoFinalSiHaTerminado(objeto);
			AplicarAnimacion(objeto.pose, objeto.modelo->animaciones[objeto.indiceAnimacion], objeto.reproductor.tiempo);
		}
	}

	void VaciarEscenaAnimada(EscenaAnimada& escena)
	{
		escena.objetos.clear();   // primero las instancias, que apuntan a los modelos
		escena.modelos.clear();
	}

	glm::vec3 PuntoDeInteraccion(const ObjetoAnimado& objeto)
	{
		// glTF (Y arriba) -> juego (Z arriba) -> colocación de esta instancia en la habitación.
		const glm::mat4 aMundo = MatrizBaseDelObjeto(objeto) * MatrizYArribaAZArriba();
		return glm::vec3(aMundo * glm::vec4(objeto.centroDelModelo, 1.0f));
	}

	ObjetoAnimado* BuscarObjetoInteractuable(EscenaAnimada& escena, const glm::vec3& origen,
		const glm::vec3& direccion, float radio, float cosenoMinimo)
	{
		if (glm::length(direccion) < 1.0e-6f) return nullptr;
		const glm::vec3 mirada = glm::normalize(direccion);

		ObjetoAnimado* mejor = nullptr;
		float mejorCoseno = -2.0f;
		for (ObjetoAnimado& objeto : escena.objetos)
		{
			if (!TieneAnimacion(objeto)) continue;
			const glm::vec3 punto = PuntoDeInteraccion(objeto);
			if (glm::distance(origen, punto) > radio) continue;              // demasiado lejos

			const float coseno = CosenoHaciaElPunto(origen, mirada, punto);
			if (coseno < cosenoMinimo) continue;                             // no está delante
			if (coseno > mejorCoseno) { mejor = &objeto; mejorCoseno = coseno; }   // la más centrada gana
		}
		return mejor;
	}

	ObjetoAnimado* InteractuarConObjeto(EscenaAnimada& escena, const glm::vec3& origen, const glm::vec3& direccion)
	{
		ObjetoAnimado* objeto = BuscarObjetoInteractuable(escena, origen, direccion);
		if (objeto != nullptr) AlternarPuerta(*objeto);
		return objeto;
	}

	bool PuedeInteractuar(bool juegoPausado, bool ventanaConFoco, bool escribiendoEnInterfaz)
	{
		return !juegoPausado && ventanaConFoco && !escribiendoEnInterfaz;
	}
}
