#pragma once

#include "../SceneLoader.h"
#include "../ObjetosAnimadosGLB.h"
#include "CamaraEditor.h"
#include "HistorialEscena.h"
#include "LimitesModelo.h"
#include "VisibilidadEditor.h"

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
	struct ContextoEscena
	{
		std::vector<SceneObject>& objetos;
		std::vector<SceneObjetoAnimado>& animados;
		glb::EscenaAnimada& escenaGlb;   // para que la puerta se vea en su sitio nuevo al instante
	};

	// Carga (o reutiliza) un OBJ en la GPU. Devuelve la clave con la que la escena dibuja ese modelo.
	using CargadorModelo = std::function<bool(const std::filesystem::path& ruta, std::string& clave, std::string& error)>;

	class EditorEscenas
	{
	public:
		bool Activo() const { return activo; }
		void Activar(bool activar);

		// Acciones que main.cpp debe ejecutar (guardar en disco, volver a PLAY, cerrar).
		bool ConsumeEntradaPlay();
		bool ConsumeGuardarRequest();
		bool ConsumeCerrarConfirmado();
		bool ConsumeCerrarTrasGuardar();

		void FijarCargadorModelos(CargadorModelo cargador) { cargadorModelos = std::move(cargador); }
		void FijarRaizModelos(const std::filesystem::path& raiz);
		// Una escena nueva (cargada desde disco) borra historial y selección.
		void NuevaEscena();

		void Atajo(int tecla, int accion, int modificadores, bool escribiendoTexto);
		void Actualizar(GLFWwindow* ventana, float delta);
		void DibujarInterfaz(ContextoEscena contexto, int ancho, int alto);
		// Rueda del ratón: solo hace zoom si el cursor está sobre el viewport (ni sobre un panel ImGui, ni arrastrando un gizmo,
		// ni navegando con otro botón).
		void RuedaRaton(float cantidad);
		void Matrices(float aspecto, glm::mat4& vista, glm::mat4& proyeccion) const;

		// Visibilidad TEMPORAL de la vista de edición (no toca la escena ni el JSON). Fuera de EDIT todo es visible.
		bool ObjetoVisible(std::size_t indice) const;
		bool MostrarAnimados() const;
		bool MostrarColisionadores() const { return mostrarColisionadores; }

		int Seleccionado() const { return seleccionado; }
		void MarcarGuardado() { modificada = false; }
		bool HayCambiosSinGuardar() const { return modificada; }
		void InformarEstado(std::string estado, bool error = false) { mensajeEstado = std::move(estado); mensajeEsError = error; }
		void SolicitarConfirmacionCierre() { confirmacionCierre = true; }

		// --- Operaciones (públicas para poder probarlas sin ventana) ---
		bool Deshacer(ContextoEscena contexto);
		bool Rehacer(ContextoEscena contexto);
		// Añade un OBJ ya cargado en la GPU. Devuelve el id de la instancia nueva ("" si falla).
		std::string AgregarModelo(ContextoEscena contexto, const std::string& clave, bool yArribaAZArriba, bool colisionador, bool dinamico);
		bool DuplicarSeleccion(ContextoEscena contexto);
		bool EliminarSeleccion(ContextoEscena contexto);
		const HistorialEscena& Historial() const { return historial; }
		void Seleccionar(int indice) { seleccionado = indice; seleccionCambiada = true; }
		VisibilidadEditor& Visibilidad() { return visibilidad; }

	private:
		struct ElementoSeleccionable
		{
			int indice = -1;
			glm::mat4 modelo{ 1.0f };
			CajaLocal caja;
		};

		EstadoEscena Capturar(const ContextoEscena& contexto) const;
		void Restaurar(ContextoEscena contexto, EstadoEscena estado);
		void SincronizarGlb(const ContextoEscena& contexto) const;
		bool IndiceValido(const ContextoEscena& contexto, int indice) const;
		bool EsAnimado(const ContextoEscena& contexto, int indice) const { return indice >= static_cast<int>(contexto.objetos.size()); }
		std::string IdDe(const ContextoEscena& contexto, int indice) const;
		ElementoSeleccionable Elemento(const ContextoEscena& contexto, int indice);
		CajaLocal CajaDeObjeto(const SceneObject& objeto);
		CajaLocal CajaDeAnimado(const ContextoEscena& contexto, const SceneObjetoAnimado& animado);
		std::string IdUnico(const ContextoEscena& contexto, const std::string& base) const;
		float AlturaDelSuelo(const ContextoEscena& contexto) const;

		void ActualizarVisibilidad(const ContextoEscena& contexto);
		void AbrirTransaccionSiHaceFalta();
		void CerrarTransaccionSiTermino(const ContextoEscena& contexto);
		void EnfocarSeleccion(const ContextoEscena& contexto);
		void DibujarBarra(ContextoEscena contexto);
		void DibujarJerarquia(ContextoEscena contexto);
		void DibujarInspector(ContextoEscena contexto);
		void DibujarAssetBrowser(ContextoEscena contexto);
		void DibujarGizmo(ContextoEscena contexto, int ancho, int alto);
		void DibujarContornoSeleccion(ContextoEscena contexto, int ancho, int alto);
		void SeleccionarPorRaton(ContextoEscena contexto, int ancho, int alto);
		void ProcesarSolicitudes(ContextoEscena contexto);
		void AnadirDesdeAssetBrowser(ContextoEscena contexto);
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
		bool anadirSolicitado = false;
		bool confirmacionCierre = false;
		bool cerrarConfirmado = false;
		bool cerrarTrasGuardar = false;
		bool enfocarSolicitado = false;
		bool modificada = false;
		bool local = false;
		bool snap = false;
		bool mostrarColisionadores = false;
		bool seleccionCambiada = false;
		bool mensajeEsError = false;
		int seleccionado = -1;
		Herramienta herramienta = Herramienta::Mover;

		// --- Cámara estilo Unity y arbitraje de la entrada ---
		enum class ModoCamara { Ninguno, Mirar, Pan, Orbitar };
		CamaraEditor camara;
		OpcionesCamara opciones;
		ModoCamara modoCamara = ModoCamara::Ninguno;
		glm::vec3 pivoteOrbita{ 0.0f };
		GLFWwindow* ventanaCaptura = nullptr;   // != nullptr mientras el cursor está capturado (botón derecho / Alt+izquierdo)
		double cursorGuardadoX = 0.0, cursorGuardadoY = 0.0;
		double anteriorX = 0.0, anteriorY = 0.0;
		bool reiniciarDelta = true;             // descarta el primer delta tras capturar/liberar el cursor (sin saltos)
		bool mostrarOpcionesCamara = false;
		struct EntradaEditor
		{
			bool sobrePanel = false;     // el cursor está sobre una ventana ImGui
			bool imguiActivo = false;    // un widget de ImGui está en uso (campo de texto, slider...)
			bool gizmoUsando = false;    // ImGuizmo está arrastrando
			bool popup = false;
			bool altPulsado = false;
			bool viewportLibre = false;  // prioridad 1-2 libres: la cámara y la selección pueden actuar
		} entrada;

		// Flechas: se acumulan en el callback de teclado y se aplican en DibujarInterfaz (con el historial).
		struct PulsacionFlecha { Flecha flecha; bool ctrl; bool shift; };
		std::vector<PulsacionFlecha> flechasPendientes;
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

		// Historial y transacciones (arrastre del gizmo / edición de un campo = UNA operación).
		HistorialEscena historial;
		EstadoEscena respaldo;          // estado al inicio del fotograma, antes de cualquier edición
		bool hayRespaldo = false;
		bool gizmoEnUso = false;

		// Visibilidad temporal.
		VisibilidadEditor visibilidad;
		std::vector<ClaseElemento> clases;
		std::vector<char> visibles;
		bool aisladoEsAnimado = false;

		// Cajas envolventes (por modelo) y assets.
		std::unordered_map<std::string, CajaLocal> cajasObj;
		std::unordered_map<std::string, CajaLocal> cajasGlb;
		CargadorModelo cargadorModelos;
		std::filesystem::path raizModelos = "assets/models";
		std::vector<std::filesystem::path> modelosDisponibles;
		bool inventarioCargado = false;
		std::string assetSeleccionado;
		char filtroAssets[64] = {};
		bool assetYArriba = true;
		bool assetColisionador = false;
		bool assetDinamico = false;
	};
}
