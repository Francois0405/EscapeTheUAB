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
#include "src/vr/OpenXRManager.h"

#include <iostream>

using namespace vr;

namespace
{
	int fallos = 0;
	void comprobar(bool condicion, const char* caso)
	{
		if (!condicion) { std::cerr << "FALLO: " << caso << '\n'; ++fallos; }
	}

	bool EsFallo(OpenXrState e)
	{
		return e == OpenXrState::SinRuntime || e == OpenXrState::SinVisor || e == OpenXrState::FaltaExtensionOpenGL ||
			e == OpenXrState::OpenGLInsuficiente || e == OpenXrState::Error;
	}
}

int main()
{
	// 1) Seleccion del modo: solo es VR si se pidio Y existe la sesion. Nunca "por accidente".
	comprobar(ChooseGamePlayMode(false, OpenXrState::SesionCreada) == GamePlayMode::Pantalla, "sin pedir VR se juega en pantalla aunque haya sesion");
	comprobar(ChooseGamePlayMode(true, OpenXrState::SinRuntime) == GamePlayMode::Pantalla, "VR pedido sin runtime: pantalla");
	comprobar(ChooseGamePlayMode(true, OpenXrState::NoCompilado) == GamePlayMode::Pantalla, "VR pedido sin OpenXR compilado: pantalla");
	comprobar(ChooseGamePlayMode(true, OpenXrState::InstanciaYSistemaListos) == GamePlayMode::Pantalla, "visor detectado pero sin sesion: pantalla");
	comprobar(ChooseGamePlayMode(true, OpenXrState::SesionCreada) == GamePlayMode::VR, "VR pedido y sesion creada: VR");

	// 2) Crear el gestor NO activa nada: OpenXR solo se inicia de forma explicita.
	{
		OpenXrManager gestor;
		comprobar(gestor.CurrentState() == OpenXrState::Desactivado, "al crear el gestor, OpenXR esta desactivado");
		comprobar(!gestor.HasResources(), "al crear el gestor no hay instancia ni sesion");
		gestor.ProcessEvents();   // sin activar no debe hacer nada ni fallar
		comprobar(!gestor.HasResources(), "procesar eventos sin activar no crea nada");
		gestor.Deactivate();        // desactivar sin haber activado es seguro
		gestor.Deactivate();        // y repetirlo tambien
		comprobar(!gestor.HasResources(), "desactivar sin activar es seguro");
		// Bucle de fotogramas sin sesion: todo es inerte y no toca OpenGL.
		comprobar(!gestor.IsSessionRunning() && !gestor.IsFocused(), "sin sesion no hay sesion en marcha");
		comprobar(gestor.BeginXrFrame() == FrameResult::NoActivo, "sin sesion no hay fotograma");
		VrEye ojo;
		comprobar(!gestor.AcquireEyeImage(0, ojo), "sin sesion no se adquiere ningun ojo");
		gestor.ReleaseEyeImage(0);
		gestor.EndXrFrame();   // sin fotograma abierto: no hace nada
		comprobar(gestor.EyeCount() == 0 && gestor.ReferenceSpaceHandle() == ReferenceSpaceKind::Ninguno, "sin sesion no hay ojos ni espacio");
	}

	// 3) Activacion explicita sin visor/runtime: falla con un motivo, sin recursos a medias.
	{
		OpenXrManager gestor;
		const OpenXrState resultado = gestor.SetActive(nullptr, 4, 6);
		std::cout << "  (info) Activar -> " << NombreDelEstado(resultado) << ": " << gestor.Mensaje() << "\n";
		comprobar(!gestor.Mensaje().empty(), "siempre hay un mensaje comprensible");
		if (!OpenXrManager::IsCompiledIn())
		{
			comprobar(resultado == OpenXrState::NoCompilado, "sin OpenXR compilado, Activar informa NoCompilado");
			comprobar(!gestor.HasResources(), "sin OpenXR no hay recursos");
		}
		else if (EsFallo(resultado))
		{
			comprobar(!gestor.HasResources(), "tras un fallo de inicializacion no queda ninguna instancia ni sesion viva");
			comprobar(gestor.CurrentState() == resultado, "el estado conserva el motivo del fallo");
			// Reintentar tras un fallo es seguro (no acumula recursos).
			gestor.SetActive(nullptr, 4, 6);
			comprobar(!gestor.HasResources() || !EsFallo(gestor.CurrentState()), "reintentar tras un fallo no deja recursos a medias");
		}
		else
			std::cout << "  (info) esta maquina tiene runtime y visor OpenXR: se omiten las pruebas de ausencia\n";

		gestor.Deactivate();
		comprobar(!gestor.HasResources(), "Desactivar libera todo");
		comprobar(gestor.CurrentState() == OpenXrState::Desactivado || gestor.CurrentState() == OpenXrState::NoCompilado, "tras desactivar vuelve al estado inicial");
	}

	// 4) Version de OpenGL insuficiente: se informa y se limpia (solo comprobable si hay visor; si no, cae en SinRuntime/SinVisor antes).
	{
		OpenXrManager gestor;
		const OpenXrState e = gestor.SetActive(nullptr, 1, 0);   // OpenGL 1.0 nunca basta
		comprobar(e != OpenXrState::SesionCreada && e != OpenXrState::InstanciaYSistemaListos, "OpenGL 1.0 nunca da una sesion valida");
		comprobar(!gestor.HasResources() || e == OpenXrState::NoCompilado, "y no deja recursos vivos");
	}

	if (fallos == 0) std::cout << "OpenXRTests: OK (sin hardware)\n";
	return fallos == 0 ? 0 : 1;
}
