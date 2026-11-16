// EscenaFisica.cpp
#include "EscenaFisica.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace fisica
{
	namespace
	{
		// Rotación (sin escala) del objeto, con la misma convención que el dibujo.
		glm::mat4 RotacionDeObjeto(const SceneObject& objeto)
		{
			glm::mat4 r(1.0f);
			r = glm::rotate(r, glm::radians(objeto.rotacion.z), glm::vec3(0.0f, 0.0f, 1.0f));
			r = glm::rotate(r, glm::radians(objeto.rotacion.y), glm::vec3(0.0f, 1.0f, 0.0f));
			r = glm::rotate(r, glm::radians(objeto.rotacion.x), glm::vec3(1.0f, 0.0f, 0.0f));
			return r;
		}

		// Quita la escala de una matriz de 3x3 quedándose solo con el giro.
		glm::quat SoloGiro(const glm::mat4& m)
		{
			glm::mat3 base(m);
			for (int columna = 0; columna < 3; ++columna) base[columna] = glm::normalize(base[columna]);
			return glm::quat_cast(base);
		}
	}

	glm::mat4 MatrizDeObjeto(const SceneObject& objeto)
	{
		const glm::mat4 colocar = glm::translate(glm::mat4(1.0f), objeto.posicion);
		return glm::scale(colocar * RotacionDeObjeto(objeto), objeto.escala);
	}

	bool CargarObjetosAnimadosDeEscena(const std::vector<SceneObjetoAnimado>& animados,
		const std::filesystem::path& carpetaDelJson, glb::EscenaAnimada& destino, std::string& error)
	{
		for (const SceneObjetoAnimado& datos : animados)
		{
			const std::string ruta = (carpetaDelJson / datos.modelo).lexically_normal().string();
			if (!glb::AnadirObjeto(destino, datos.id, ruta, datos.posicion, datos.giroZGrados, error))
			{
				error = "objeto animado '" + datos.id + "': " + error;
				return false;
			}
			glb::ObjetoAnimado& instancia = destino.objetos.back();
			instancia.escala = datos.escala;
			if (!glb::SeleccionarAnimacion(instancia, datos.animacion, error)) return false;
		}
		return true;
	}

	void EscenaFisica::Vaciar()
	{
		for (IdCuerpo id : todos) mundo.EliminarCuerpo(id);
		todos.clear();
		dinamicos.clear();
		colisionesNodo.clear();
	}

	// Calcula dónde está AHORA la caja de un nodo animado, en coordenadas del mundo.
	//   W = colocación de la instancia * (glTF Y-arriba -> Z-arriba) * matriz de mundo del nodo
	// W puede llevar escala; se separa el giro y la escala se aplica una sola vez a halfSize.
	void EscenaFisica::ColocarCaja(const Colision& caja, const glb::ObjetoAnimado& objeto, glm::vec3& centro,
		glm::quat& rotacion, glm::vec3& mitad) const
	{
		const glm::mat4 mundoNodo = glb::MatrizBaseDelObjeto(objeto) * glb::MatrizYArribaAZArriba() *
			glb::MatrizMundoDelNodo(*objeto.modelo, objeto.pose, caja.nodo);
		centro = glm::vec3(mundoNodo * glm::vec4(caja.desplazamiento, 1.0f));
		rotacion = SoloGiro(mundoNodo);
		mitad = caja.mitadTamano * glm::vec3(glm::length(glm::vec3(mundoNodo[0])),
			glm::length(glm::vec3(mundoNodo[1])), glm::length(glm::vec3(mundoNodo[2])));
	}

	bool EscenaFisica::Crear(const std::vector<SceneObject>& objetos, const glb::EscenaAnimada& animados,
		const std::vector<SceneObjetoAnimado>& datosAnimados, std::string& error)
	{
		Vaciar();   // esta EscenaFisica solo gestiona SU escena
		auto fallar = [this, &error](const std::string& mensaje)
		{
			error = mensaje;
			Vaciar();   // deshace lo creado hasta ahora: nada queda a medias
			return false;
		};

		// 1) Objetos OBJ con "physics".
		for (int i = 0; i < static_cast<int>(objetos.size()); ++i)
		{
			const SceneObject& objeto = objetos[i];
			if (!objeto.fisica.presente) continue;   // sin físicas: se queda como antes

			const glm::mat4 giro = RotacionDeObjeto(objeto);
			const glm::vec3 desplazamiento = objeto.escala * objeto.fisica.desplazamiento;   // escala UNA vez
			const glm::vec3 centro = objeto.posicion + glm::vec3(giro * glm::vec4(desplazamiento, 0.0f));
			const glm::vec3 mitad = glm::abs(objeto.escala) * objeto.fisica.mitadTamano;

			const IdCuerpo id = mundo.CrearCaja(objeto.fisica.tipo, centro, glm::quat_cast(glm::mat3(giro)),
				mitad, objeto.fisica.masa);
			if (id == CuerpoInvalido)
				return fallar("no se pudo crear la caja fisica de '" + objeto.id + "' (tamano o masa no validos)");
			todos.push_back(id);
			if (objeto.fisica.tipo == TipoCuerpo::Dinamico)
				dinamicos.push_back({ i, id, desplazamiento, objeto.escala });
		}

		// 2) Colisionadores de los objetos animados.
		for (int i = 0; i < static_cast<int>(datosAnimados.size()); ++i)
		{
			const glb::ObjetoAnimado* instancia = glb::BuscarObjeto(const_cast<glb::EscenaAnimada&>(animados), datosAnimados[i].id);
			if (instancia == nullptr)
				return fallar("el objeto animado '" + datosAnimados[i].id + "' no esta cargado");

			for (const SceneColisionador& datos : datosAnimados[i].colisionadores)
			{
				Colision caja;
				caja.indiceAnimado = static_cast<int>(instancia - animados.objetos.data());
				caja.nodo = glb::BuscarNodoPorNombre(*instancia->modelo, datos.nodo);
				if (caja.nodo < 0)
					return fallar("'" + datosAnimados[i].id + "': el modelo no tiene un nodo llamado '" + datos.nodo + "'");
				caja.tipo = datos.tipo;
				caja.mitadTamano = datos.mitadTamano;
				caja.desplazamiento = datos.desplazamiento;

				glm::vec3 centro, mitad;
				glm::quat rotacion;
				ColocarCaja(caja, *instancia, centro, rotacion, mitad);
				caja.cuerpo = mundo.CrearCaja(caja.tipo, centro, rotacion, mitad, 0.0f);
				if (caja.cuerpo == CuerpoInvalido)
					return fallar("no se pudo crear el colisionador del nodo '" + datos.nodo + "' de '" + datosAnimados[i].id + "'");
				todos.push_back(caja.cuerpo);
				colisionesNodo.push_back(caja);
			}
		}
		error.clear();
		return true;
	}

	void EscenaFisica::SincronizarCinematicos(const glb::EscenaAnimada& animados)
	{
		for (const Colision& caja : colisionesNodo)
		{
			if (caja.tipo != TipoCuerpo::Cinematico) continue;   // los estáticos no se mueven nunca
			glm::vec3 centro, mitad;
			glm::quat rotacion;
			ColocarCaja(caja, animados.objetos[caja.indiceAnimado], centro, rotacion, mitad);
			mundo.MoverCinematico(caja.cuerpo, centro, rotacion);
		}
	}

	glm::mat4 EscenaFisica::MatrizVisualDe(int indice, const SceneObject& objeto) const
	{
		for (const Dinamico& d : dinamicos)
		{
			if (d.indiceObjeto != indice) continue;
			// Bullet mueve el CENTRO de la caja; el modelo tiene su origen en otro punto, así
			// que se deshace el desplazamiento: M = trasladar(centro) * giro * trasladar(-offset) * escalar.
			const PoseCuerpo pose = mundo.PoseDe(d.cuerpo);
			return glm::translate(glm::mat4(1.0f), pose.posicion) * glm::mat4_cast(pose.rotacion) *
				glm::translate(glm::mat4(1.0f), -d.desplazamientoEscalado) * glm::scale(glm::mat4(1.0f), d.escala);
		}
		return MatrizDeObjeto(objeto);
	}

	// ---- Agarre ---------------------------------------------------------------

	namespace
	{
		// Punto del mundo donde queda el objeto: delante de los ojos, a DistanciaDeLaMano. La parte
		// vertical se limita: si se mira muy abajo, el objeto no debe acabar dentro de los pies.
		glm::vec3 PuntoDeLaMano(const glm::vec3& origen, const glm::vec3& direccion)
		{
			const glm::vec3 mirada = glm::normalize(direccion);
			glm::vec3 horizontal(mirada.x, mirada.y, 0.0f);
			if (glm::length(horizontal) < 1.0e-4f) horizontal = glm::vec3(1.0f, 0.0f, 0.0f);   // mirando recto arriba/abajo
			horizontal = glm::normalize(horizontal);
			const float subida = std::clamp(mirada.z * EscenaFisica::DistanciaDeLaMano, -0.4f, 0.6f);
			return origen + horizontal * (EscenaFisica::DistanciaDeLaMano * 0.9f) + glm::vec3(0.0f, 0.0f, subida);
		}
	}

	int EscenaFisica::IntentarAgarrar(const glm::vec3& origen, const glm::vec3& direccion)
	{
		const IdCuerpo mirado = mundo.CuerpoAlFinalDelRayo(origen, direccion, AlcanceDeAgarre);
		const int indice = ObjetoDinamicoDeCuerpo(mirado);
		if (!SujetarObjetoDinamico(indice, origen, direccion)) return -1;
		return indice;
	}

	int EscenaFisica::ObjetoDinamicoDeCuerpo(IdCuerpo cuerpo) const
	{
		for (const Dinamico& d : dinamicos)
			if (d.cuerpo == cuerpo && cuerpo != CuerpoInvalido) return d.indiceObjeto;
		return -1;
	}

	int EscenaFisica::ObjetoAnimadoDeCuerpo(IdCuerpo cuerpo) const
	{
		for (const Colision& c : colisionesNodo)
			if (c.cuerpo == cuerpo && cuerpo != CuerpoInvalido) return c.indiceAnimado;
		return -1;
	}

	bool EscenaFisica::SujetarObjetoDinamico(int indiceObjeto, const glm::vec3& origen, const glm::vec3& direccion)
	{
		for (const Dinamico& d : dinamicos)
		{
			if (d.indiceObjeto != indiceObjeto) continue;
			if (!mundo.SujetarCuerpo(d.cuerpo)) return false;
			mundo.FijarDestinoDeSujecion(PuntoDeLaMano(origen, direccion));
			return true;
		}
		return false;
	}

	void EscenaFisica::ActualizarAgarre(const glm::vec3& origen, const glm::vec3& direccion)
	{
		if (!HayAgarre()) return;
		mundo.FijarDestinoDeSujecion(PuntoDeLaMano(origen, direccion));
		// Si algo (una pared) frena el objeto y la mano ya está lejos, se suelta: así no queda
		// "tensando" contra el obstáculo ni cruza al otro lado cuando por fin cede.
		if (glm::distance(mundo.PoseDe(mundo.CuerpoSujeto()).posicion, mundo.DestinoDeSujecion()) > SoltarSiSeAleja)
			SoltarAgarre();
	}

	void EscenaFisica::SoltarAgarre() { mundo.SoltarCuerpoSujeto(); }
	bool EscenaFisica::LanzarAgarre(const glm::vec3& velocidad) { return mundo.LanzarCuerpoSujeto(velocidad); }
	bool EscenaFisica::HayAgarre() const { return ObjetoAgarrado() >= 0; }

	int EscenaFisica::ObjetoAgarrado() const
	{
		const IdCuerpo sujeto = mundo.CuerpoSujeto();
		for (const Dinamico& d : dinamicos)
			if (d.cuerpo == sujeto && sujeto != CuerpoInvalido) return d.indiceObjeto;
		return -1;
	}
}
