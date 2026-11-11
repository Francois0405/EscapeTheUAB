// OpenXRTests.cpp
// -----------------------------------------------------------------------------
// Pruebas de OpenXRManager que NO necesitan visor ni ventana. Se ejecutan igual con y sin OpenXR
// compilado:
//   * sin OpenXR   -> Activar() debe decir "NoCompilado" y no hacer nada.
//   * con OpenXR   -> en una maquina sin runtime/visor debe fallar con un motivo claro, sin colgarse
//                     ni dejar recursos vivos. Si la maquina SI tiene runtime y visor, esas comprobaciones
//                     se saltan (no se puede suponer que no lo hay).
//
// NO se prueba aqui (requiere un casco real, ver docs/OPENXR.md): crear la sesion, eventos reales,
// version de OpenGL aceptada por Quest Link, bucle de fotogramas.
// -----------------------------------------------------------------------------
#include "OpenXRManager.h"

#include <iostream>

using namespace vr;

namespace
{
	int fallos = 0;
	void comprobar(bool condicion, const char* caso)
	{
		if (!condicion) { std::cerr << "FALLO: " << caso << '\n'; ++fallos; }
	}

	bool EsFallo(EstadoOpenXR e)
	{
		return e == EstadoOpenXR::SinRuntime || e == EstadoOpenXR::SinVisor || e == EstadoOpenXR::FaltaExtensionOpenGL ||
			e == EstadoOpenXR::OpenGLInsuficiente || e == EstadoOpenXR::Error;
	}
}

int main()
{
	// 1) Seleccion del modo: solo es VR si se pidio Y existe la sesion. Nunca "por accidente".
	comprobar(ElegirModoDeJuego(false, EstadoOpenXR::SesionCreada) == ModoDeJuego::Pantalla, "sin pedir VR se juega en pantalla aunque haya sesion");
	comprobar(ElegirModoDeJuego(true, EstadoOpenXR::SinRuntime) == ModoDeJuego::Pantalla, "VR pedido sin runtime: pantalla");
	comprobar(ElegirModoDeJuego(true, EstadoOpenXR::NoCompilado) == ModoDeJuego::Pantalla, "VR pedido sin OpenXR compilado: pantalla");
	comprobar(ElegirModoDeJuego(true, EstadoOpenXR::InstanciaYSistemaListos) == ModoDeJuego::Pantalla, "visor detectado pero sin sesion: pantalla");
	comprobar(ElegirModoDeJuego(true, EstadoOpenXR::SesionCreada) == ModoDeJuego::VR, "VR pedido y sesion creada: VR");

	// 2) Crear el gestor NO activa nada: OpenXR solo se inicia de forma explicita.
	{
		GestorOpenXR gestor;
		comprobar(gestor.Estado() == EstadoOpenXR::Desactivado, "al crear el gestor, OpenXR esta desactivado");
		comprobar(!gestor.TieneRecursos(), "al crear el gestor no hay instancia ni sesion");
		gestor.ProcesarEventos();   // sin activar no debe hacer nada ni fallar
		comprobar(!gestor.TieneRecursos(), "procesar eventos sin activar no crea nada");
		gestor.Desactivar();        // desactivar sin haber activado es seguro
		gestor.Desactivar();        // y repetirlo tambien
		comprobar(!gestor.TieneRecursos(), "desactivar sin activar es seguro");
		// Bucle de fotogramas sin sesion: todo es inerte y no toca OpenGL.
		comprobar(!gestor.SesionEnMarcha() && !gestor.Enfocada(), "sin sesion no hay sesion en marcha");
		comprobar(gestor.EmpezarFotograma() == ResultadoFotograma::NoActivo, "sin sesion no hay fotograma");
		OjoVR ojo;
		comprobar(!gestor.AdquirirOjo(0, ojo), "sin sesion no se adquiere ningun ojo");
		gestor.LiberarOjo(0);
		gestor.TerminarFotograma();   // sin fotograma abierto: no hace nada
		comprobar(gestor.NumeroDeOjos() == 0 && gestor.Espacio() == EspacioReferencia::Ninguno, "sin sesion no hay ojos ni espacio");
	}

	// 3) Activacion explicita sin visor/runtime: falla con un motivo, sin recursos a medias.
	{
		GestorOpenXR gestor;
		const EstadoOpenXR resultado = gestor.Activar(nullptr, 4, 6);
		std::cout << "  (info) Activar -> " << NombreDelEstado(resultado) << ": " << gestor.Mensaje() << "\n";
		comprobar(!gestor.Mensaje().empty(), "siempre hay un mensaje comprensible");
		if (!GestorOpenXR::EstaCompilado())
		{
			comprobar(resultado == EstadoOpenXR::NoCompilado, "sin OpenXR compilado, Activar informa NoCompilado");
			comprobar(!gestor.TieneRecursos(), "sin OpenXR no hay recursos");
		}
		else if (EsFallo(resultado))
		{
			comprobar(!gestor.TieneRecursos(), "tras un fallo de inicializacion no queda ninguna instancia ni sesion viva");
			comprobar(gestor.Estado() == resultado, "el estado conserva el motivo del fallo");
			// Reintentar tras un fallo es seguro (no acumula recursos).
			gestor.Activar(nullptr, 4, 6);
			comprobar(!gestor.TieneRecursos() || !EsFallo(gestor.Estado()), "reintentar tras un fallo no deja recursos a medias");
		}
		else
			std::cout << "  (info) esta maquina tiene runtime y visor OpenXR: se omiten las pruebas de ausencia\n";

		gestor.Desactivar();
		comprobar(!gestor.TieneRecursos(), "Desactivar libera todo");
		comprobar(gestor.Estado() == EstadoOpenXR::Desactivado || gestor.Estado() == EstadoOpenXR::NoCompilado, "tras desactivar vuelve al estado inicial");
	}

	// 4) Version de OpenGL insuficiente: se informa y se limpia (solo comprobable si hay visor; si no, cae en SinRuntime/SinVisor antes).
	{
		GestorOpenXR gestor;
		const EstadoOpenXR e = gestor.Activar(nullptr, 1, 0);   // OpenGL 1.0 nunca basta
		comprobar(e != EstadoOpenXR::SesionCreada && e != EstadoOpenXR::InstanciaYSistemaListos, "OpenGL 1.0 nunca da una sesion valida");
		comprobar(!gestor.TieneRecursos() || e == EstadoOpenXR::NoCompilado, "y no deja recursos vivos");
	}

	if (fallos == 0) std::cout << "OpenXRTests: OK (sin hardware)\n";
	return fallos == 0 ? 0 : 1;
}
