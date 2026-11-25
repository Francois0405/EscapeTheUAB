#pragma once

#include "src/scene/SceneLoader.h"
#include "src/animation/GlbAnimatedObjects.h"
#include "EditorCamera.h"
#include "SceneHistory.h"
#include "ModelBounds.h"
#include "PlacementValidator.h"
#include "EditorVisibility.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace editor
{
	enum class Herramienta { Mover, Rotar, Escalar };

	// Lo que el editor puede modificar. Los objetos OBJ (arquitectura y objetos) y las instancias GLB animadas (puertas)
	// viven en listas distintas; el editor las presenta como una sola jerarquía.
	struct EditorSceneContext
	{
		std::vector<SceneObject>& objetos;
		std::vector<SceneAnimatedObject>& animados;
		glb::AnimatedScene& escenaGlb;   // para que la puerta se vea en su sitio nuevo al instante
	};

	// Carga (o reutiliza) un OBJ en la GPU. Devuelve la clave con la que la escena dibuja ese modelo.
	using CargadorModelo = std::function<bool(const std::filesystem::path& ruta, std::string& clave, std::string& error)>;
	using CargadorTextura = std::function<unsigned int(const std::filesystem::path& ruta)>;
	using CargadorPuerta = std::function<bool(const std::filesystem::path& ruta, const SceneAnimatedObject& datos,
		EditorSceneContext contexto, std::string& error)>;

	class SceneEditor
	{
	public:
		bool Activo() const { return activo; }
		void Activar(bool activar);

		// Acciones que main.cpp debe ejecutar (guardar en disco, volver a PLAY, cerrar).
		bool ConsumeEntradaPlay();
		bool ConsumeGuardarRequest();
		bool ConsumeCerrarConfirmado();
		bool ConsumeCerrarTrasGuardar();
		bool ConsumeReconstruirFisicaRequest() { const bool v = reconstruirFisicaSolicitada; reconstruirFisicaSolicitada = false; return v; }

		void FijarCargadorModelos(CargadorModelo cargador) { cargadorModelos = std::move(cargador); }
		void FijarCargadorTexturas(CargadorTextura cargador) { cargadorTexturas = std::move(cargador); }
		void FijarCargadorPuertas(CargadorPuerta cargador) { cargadorPuertas = std::move(cargador); }
		void FijarRaizModelos(const std::filesystem::path& raiz);
		void FijarRaizEscena(const std::filesystem::path& raiz) { raizEscena = raiz; }
		void FijarRaizAnimaciones(const std::filesystem::path& raiz) { raizAnimaciones = raiz; inventarioPuertasCargado = false; }
		// Una escena nueva (cargada desde disco) borra historial y selección.
		void NuevaEscena();

		void Atajo(int tecla, int accion, int modificadores, bool escribiendoTexto);
		void Actualizar(GLFWwindow* ventana, float delta);
		void DibujarInterfaz(EditorSceneContext contexto, int ancho, int alto);
		// Rueda del ratón: solo hace zoom si el cursor está sobre el viewport (ni sobre un panel ImGui, ni arrastrando un gizmo,
		// ni navegando con otro botón).
		void RuedaRaton(float cantidad);
		void Matrices(float aspecto, glm::mat4& vista, glm::mat4& proyeccion) const;

		// Visibilidad TEMPORAL de la vista de edición (no toca la escena ni el JSON). Fuera de EDIT todo es visible.
		bool ObjetoVisible(std::size_t indice) const;
		bool MostrarAnimados() const;
		bool MostrarColisionadores() const { return showColliders; }

		int Seleccionado() const { return seleccionado; }
		void MarcarGuardado() { modificada = false; }
		bool HayCambiosSinGuardar() const { return modificada; }
		void InformarEstado(std::string estado, bool error = false) { mensajeEstado = std::move(estado); mensajeEsError = error; }
		void SolicitarConfirmacionCierre() { confirmacionCierre = true; }

		// --- Operaciones (públicas para poder probarlas sin ventana) ---
		bool Deshacer(EditorSceneContext contexto);
		bool Rehacer(EditorSceneContext contexto);
		// Añade un OBJ ya cargado en la GPU. Devuelve el id de la instancia nueva ("" si falla).
		std::string AgregarModelo(EditorSceneContext contexto, const std::string& clave, bool yArribaAZArriba, bool colisionador, bool dinamico);
		bool DuplicarSeleccion(EditorSceneContext contexto);
		bool EliminarSeleccion(EditorSceneContext contexto, bool cerrarHueco = true);
		const SceneHistory& Historial() const { return historial; }
		void Seleccionar(int indice)
		{
			if (seleccionado != indice) { previsualizacionGizmoActiva = false; previsualizacionGizmoInvalida = false; idPrevisualizacionGizmo.clear(); }
			seleccionado = indice; seleccionCambiada = true; bloqueoGizmoTrasSeleccion.Bloquear();
		}
		EditorVisibility& Visibilidad() { return visibilidad; }

	private:
		struct SelectableElement
		{
			int indice = -1;
			glm::mat4 modelo{ 1.0f };
			LocalBox caja;
		};

		SceneState Capturar(const EditorSceneContext& contexto) const;
		void Restaurar(EditorSceneContext contexto, SceneState estado);
		void SincronizarGlb(const EditorSceneContext& contexto) const;
		bool IndiceValido(const EditorSceneContext& contexto, int indice) const;
		bool EsAnimado(const EditorSceneContext& contexto, int indice) const { return indice >= static_cast<int>(contexto.objetos.size()); }
		std::string IdDe(const EditorSceneContext& contexto, int indice) const;
		SelectableElement Elemento(const EditorSceneContext& contexto, int indice);
		LocalBox CajaDeObjeto(const SceneObject& objeto);
		LocalBox CajaDeAnimado(const EditorSceneContext& contexto, const SceneAnimatedObject& animado);
		std::vector<OrientedBox> CajasDeObjeto(const SceneObject& objeto, const glm::mat4& matriz);
		std::vector<OrientedBox> CajasDeAnimado(const EditorSceneContext& contexto, const SceneAnimatedObject& animado);
		PlacementResult ValidarObjeto(const EditorSceneContext& contexto, const SceneObject& candidato, int indiceIgnorado,
			const glm::mat4* matrizPropuesta = nullptr);
		PlacementResult ValidarAnimado(const EditorSceneContext& contexto, const SceneAnimatedObject& candidato, int indiceIgnorado);
		std::string IdUnico(const EditorSceneContext& contexto, const std::string& base) const;
		float AlturaDelSuelo(const EditorSceneContext& contexto) const;

		void ActualizarVisibilidad(const EditorSceneContext& contexto);
		void AbrirTransaccionSiHaceFalta();
		void CerrarTransaccionSiTermino(const EditorSceneContext& contexto);
		void EnfocarSeleccion(const EditorSceneContext& contexto);
		void DibujarBarra(EditorSceneContext contexto);
		void DibujarJerarquia(EditorSceneContext contexto);
		void DibujarInspector(EditorSceneContext contexto);
		void DibujarAssetBrowser(EditorSceneContext contexto);
		void DibujarGizmo(EditorSceneContext contexto, int ancho, int alto);
		void FijarSeleccion(int indice)
		{
			if (seleccionado != indice) { previsualizacionGizmoActiva = false; previsualizacionGizmoInvalida = false; idPrevisualizacionGizmo.clear(); }
			seleccionado = indice; seleccionCambiada = true; bloqueoGizmoTrasSeleccion.Bloquear();
		}
		void DibujarContornoSeleccion(EditorSceneContext contexto, int ancho, int alto);
		void SeleccionarPorRaton(EditorSceneContext contexto, int ancho, int alto);
		void ProcesarSolicitudes(EditorSceneContext contexto);
		void AnadirDesdeAssetBrowser(EditorSceneContext contexto);
		bool InstalarPuertaEnPared(EditorSceneContext contexto, int indicePuerta, int indicePared);
		void CambiarTransformacion(SceneObject& objeto, const glm::mat4& matriz);
		void MarcarModificada() { modificada = true; }
		void DibujarOpcionesCamara();
		void DibujarDepuracion();
		void TerminarCaptura();

		bool activo = false;
		bool salirAPlay = false;
		bool guardarSolicitado = false;
		bool deshacerSolicitado = false;
		bool rehacerSolicitado = false;
		bool duplicarSolicitado = false;
		bool eliminarSolicitado = false;
		bool eliminarConservarHuecoSolicitado = false;
		bool anadirSolicitado = false;
		bool confirmacionCierre = false;
		bool cerrarConfirmado = false;
		bool cerrarTrasGuardar = false;
		bool enfocarSolicitado = false;
		bool modificada = false;
		bool reconstruirFisicaSolicitada = false;
		std::unordered_map<std::string, glb::AnimatedObject> glbEliminados;
		bool local = false;
		bool snap = false;
		GizmoPostSelectionLock bloqueoGizmoTrasSeleccion;
		bool showColliders = false;
		bool seleccionCambiada = false;
		bool mensajeEsError = false;
		int seleccionado = -1;
		Herramienta herramienta = Herramienta::Mover;

		// --- Cámara estilo Unity y arbitraje de la entrada ---
		enum class CameraMode { Ninguno, Mirar, Pan, Orbitar };
		CamaraEditor camara;
		CameraOptions opciones;
		CameraMode modoCamara = CameraMode::Ninguno;
		glm::vec3 pivoteOrbita{ 0.0f };
		GLFWwindow* ventanaCaptura = nullptr;   // != nullptr mientras el cursor está capturado (botón derecho / Alt+izquierdo)
		double cursorGuardadoX = 0.0, cursorGuardadoY = 0.0;
		double anteriorX = 0.0, anteriorY = 0.0;
		bool reiniciarDelta = true;             // descarta el primer delta tras capturar/liberar el cursor (sin saltos)
		bool mostrarOpcionesCamara = false;
		struct EditorInput
		{
			bool sobrePanel = false;     // el cursor está sobre una ventana ImGui
			bool imguiActivo = false;    // un widget de ImGui está en uso (campo de texto, slider...)
			bool gizmoUsando = false;    // ImGuizmo está arrastrando
			bool popup = false;
			bool altPulsado = false;
			bool viewportLibre = false;  // prioridad 1-2 libres: la cámara y la selección pueden actuar
		} entrada;

		// Flechas: se acumulan en el callback de teclado y se aplican en DibujarInterfaz (con el historial).
		struct ArrowPress { ArrowKey flecha; bool ctrl; bool shift; };
		std::vector<ArrowPress> flechasPendientes;
		unsigned teclasFlechaPulsadas = 0;      // máscara de flechas mantenidas: mientras haya alguna, es UNA operación de historial
		bool haySeleccionValida = false;

		// Instrumentación del gizmo (F1 del editor: casilla "Debug input" o variable ESCAPETHEUAB_EDITOR_DEBUG=1).
		bool depurarEntrada = false;
		bool matrizCambiada = false;
		bool depuracionPorConsola = false;
		bool depuracionIniciada = false;
		int operacionDepuracion = 0;
		bool gizmoSobre = false;
		std::string mensajeEstado;
		std::string mensajeColocacion;
		bool previsualizacionGizmoActiva = false;
		bool previsualizacionGizmoInvalida = false;
		glm::mat4 matrizPrevisualizacionGizmo{ 1.0f };
		std::string idPrevisualizacionGizmo;

		// Historial y transacciones (arrastre del gizmo / edición de un campo = UNA operación).
		SceneHistory historial;
		SceneState respaldo;          // estado al inicio del fotograma, antes de cualquier edición
		bool hayRespaldo = false;
		bool gizmoEnUso = false;

		// Visibilidad temporal.
		EditorVisibility visibilidad;
		std::vector<ElementClass> clases;
		std::vector<char> visibles;
		bool aisladoEsAnimado = false;

		// Cajas envolventes (por modelo) y assets.
		std::unordered_map<std::string, LocalBox> cajasObj;
		std::unordered_map<std::string, LocalBox> cajasGlb;
		CargadorModelo cargadorModelos;
		CargadorTextura cargadorTexturas;
		CargadorPuerta cargadorPuertas;
		std::filesystem::path raizModelos = "assets/models";
		std::filesystem::path raizEscena;
		std::filesystem::path raizAnimaciones = "assets/animations";
		std::vector<std::filesystem::path> modelosDisponibles;
		bool inventarioCargado = false;
		std::string assetSeleccionado;
		char filtroAssets[64] = {};
		char filtroTexturas[128] = {};
		int categoriaTexturas = 0;
		int paredDestinoPuerta = -1;
		std::vector<std::filesystem::path> puertasDisponibles;
		bool inventarioPuertasCargado = false;
		bool assetEsPuerta = false;
		std::filesystem::path puertaSeleccionada;
		bool assetYArriba = true;
		bool assetColisionador = false;
		bool assetDinamico = false;
	};
}
