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
	enum class EditorTool { Move, Rotate, Scale };

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
		bool IsEnabled() const { return activo; }
		void SetActive(bool activar);

		// Acciones que src/app debe ejecutar (guardar en disco, volver a PLAY, cerrar).
		bool ConsumePlayEntry();
		bool ConsumeSaveRequest();
		bool ConsumeCloseConfirmed();
		bool ConsumeCloseAfterSave();
		bool ConsumeRebuildPhysicsRequest() { const bool v = rebuildPhysicsRequested; rebuildPhysicsRequested = false; return v; }

		void SetModelLoader(CargadorModelo cargador) { modelLoader = std::move(cargador); }
		void SetTextureLoader(CargadorTextura cargador) { textureLoader = std::move(cargador); }
		void SetDoorLoader(CargadorPuerta cargador) { doorLoader = std::move(cargador); }
		void SetModelsRoot(const std::filesystem::path& raiz);
		void SetSceneRoot(const std::filesystem::path& raiz) { sceneRoot = raiz; }
		void SetAnimationsRoot(const std::filesystem::path& raiz) { animationsRoot = raiz; inventarioPuertasCargado = false; }
		// Una escena nueva (cargada desde disco) borra historial y selección.
		void NewScene();

		void Shortcut(int tecla, int accion, int modificadores, bool escribiendoTexto);
		void Update(GLFWwindow* ventana, float delta);
		void DrawEditorInterface(EditorSceneContext contexto, int ancho, int alto);
		// Rueda del ratón: solo hace zoom si el cursor está sobre el viewport (ni sobre un panel ImGui, ni arrastrando un gizmo,
		// ni navegando con otro botón).
		void MouseWheelInput(float cantidad);
		void Matrices(float aspecto, glm::mat4& vista, glm::mat4& proyeccion) const;

		// Visibilidad TEMPORAL de la vista de edición (no toca la escena ni el JSON). Fuera de EDIT todo es visible.
		bool IsObjectVisible(std::size_t indice) const;
		bool ShowAnimated() const;
		bool ShowCollidersEnabled() const { return showColliders; }

		int IsSelected() const { return seleccionado; }
		void MarkSaved() { modificada = false; }
		bool HasUnsavedChanges() const { return modificada; }
		void ReportStatus(std::string estado, bool error = false) { statusMessage = std::move(estado); messageIsError = error; }
		void RequestCloseConfirmation() { closeConfirmation = true; }

		// --- Operaciones (públicas para poder probarlas sin ventana) ---
		bool Undo(EditorSceneContext contexto);
		bool Redo(EditorSceneContext contexto);
		// Añade un OBJ ya cargado en la GPU. Devuelve el id de la instancia nueva ("" si falla).
		std::string AppendModel(EditorSceneContext contexto, const std::string& clave, bool yArribaAZArriba, bool colisionador, bool dinamico);
		bool DuplicateSelection(EditorSceneContext contexto);
		bool DeleteSelection(EditorSceneContext contexto, bool cerrarHueco = true);
		const SceneHistory& Historial() const { return historial; }
		void SelectElement(int indice)
		{
			if (seleccionado != indice) { previsualizacionGizmoActiva = false; previsualizacionGizmoInvalida = false; idPrevisualizacionGizmo.clear(); }
			seleccionado = indice; selectionChanged = true; bloqueoGizmoTrasSeleccion.Lock();
		}
		EditorVisibility& VisibilityState() { return visibilidad; }

	private:
		struct SelectableElement
		{
			int indice = -1;
			glm::mat4 modelo{ 1.0f };
			LocalBox caja;
		};

		SceneState CaptureHistory(const EditorSceneContext& contexto) const;
		void RestorePose(EditorSceneContext contexto, SceneState estado);
		void SyncGlb(const EditorSceneContext& contexto) const;
		bool IsValidIndex(const EditorSceneContext& contexto, int indice) const;
		bool IsAnimated(const EditorSceneContext& contexto, int indice) const { return indice >= static_cast<int>(contexto.objetos.size()); }
		std::string IdOf(const EditorSceneContext& contexto, int indice) const;
		SelectableElement ElementAt(const EditorSceneContext& contexto, int indice);
		LocalBox BoxOfObject(const SceneObject& objeto);
		LocalBox BoxOfAnimated(const EditorSceneContext& contexto, const SceneAnimatedObject& animado);
		std::vector<OrientedBox> BoxesOfObject(const SceneObject& objeto, const glm::mat4& matriz);
		std::vector<OrientedBox> BoxesOfAnimated(const EditorSceneContext& contexto, const SceneAnimatedObject& animado);
		PlacementResult ValidateObject(const EditorSceneContext& contexto, const SceneObject& candidato, int indiceIgnorado,
			const glm::mat4* matrizPropuesta = nullptr);
		PlacementResult ValidateAnimated(const EditorSceneContext& contexto, const SceneAnimatedObject& candidato, int indiceIgnorado);
		std::string UniqueId(const EditorSceneContext& contexto, const std::string& base) const;
		float FloorHeight(const EditorSceneContext& contexto) const;

		void UpdateVisibility(const EditorSceneContext& contexto);
		void OpenTransactionIfNeeded();
		void CommitTransactionIfFinished(const EditorSceneContext& contexto);
		void FocusSelection(const EditorSceneContext& contexto);
		void DrawEditorToolbar(EditorSceneContext contexto);
		void DrawHierarchy(EditorSceneContext contexto);
		void DrawInspector(EditorSceneContext contexto);
		void DrawAssetBrowser(EditorSceneContext contexto);
		void DrawGizmo(EditorSceneContext contexto, int ancho, int alto);
		void SetSelection(int indice)
		{
			if (seleccionado != indice) { previsualizacionGizmoActiva = false; previsualizacionGizmoInvalida = false; idPrevisualizacionGizmo.clear(); }
			seleccionado = indice; selectionChanged = true; bloqueoGizmoTrasSeleccion.Lock();
		}
		void DrawSelectionOutline(EditorSceneContext contexto, int ancho, int alto);
		void SelectByMouse(EditorSceneContext contexto, int ancho, int alto);
		void ProcessPendingRequests(EditorSceneContext contexto);
		void AddFromAssetBrowser(EditorSceneContext contexto);
		bool InstallDoorInWall(EditorSceneContext contexto, int indicePuerta, int indicePared);
		void ChangeTransform(SceneObject& objeto, const glm::mat4& matriz);
		void MarkModified() { modificada = true; }
		void DrawCameraOptions();
		void DrawDebugShapes();
		void EndCapture();

		bool activo = false;
		bool exitToPlay = false;
		bool saveRequested = false;
		bool undoRequested = false;
		bool redoRequested = false;
		bool duplicateRequested = false;
		bool deleteRequested = false;
		bool eliminarConservarHuecoSolicitado = false;
		bool addRequested = false;
		bool closeConfirmation = false;
		bool closeConfirmed = false;
		bool closeAfterSave = false;
		bool focusRequested = false;
		bool modificada = false;
		bool rebuildPhysicsRequested = false;
		std::unordered_map<std::string, glb::AnimatedObject> glbEliminados;
		bool local = false;
		bool snap = false;
		GizmoPostSelectionLock bloqueoGizmoTrasSeleccion;
		bool showColliders = false;
		bool selectionChanged = false;
		bool messageIsError = false;
		int seleccionado = -1;
		EditorTool herramienta = EditorTool::Move;

		// --- Cámara estilo Unity y arbitraje de la entrada ---
		enum class CameraMode { None, Look, Pan, Orbit };
		CamaraEditor camara;
		CameraOptions opciones;
		CameraMode modoCamara = CameraMode::None;
		glm::vec3 orbitPivot{ 0.0f };
		GLFWwindow* ventanaCaptura = nullptr;   // != nullptr mientras el cursor está capturado (botón derecho / Alt+izquierdo)
		double cursorGuardadoX = 0.0, cursorGuardadoY = 0.0;
		double anteriorX = 0.0, anteriorY = 0.0;
		bool reiniciarDelta = true;             // descarta el primer delta tras capturar/liberar el cursor (sin saltos)
		bool showCameraOptions = false;
		struct EditorInput
		{
			bool sobrePanel = false;     // el cursor está sobre una ventana ImGui
			bool imguiActivo = false;    // un widget de ImGui está en uso (campo de texto, slider...)
			bool gizmoUsando = false;    // ImGuizmo está arrastrando
			bool popup = false;
			bool altPressed = false;
			bool viewportLibre = false;  // prioridad 1-2 libres: la cámara y la selección pueden actuar
		} entrada;

		// Flechas: se acumulan en el callback de teclado y se aplican en DibujarInterfaz (con el historial).
		struct ArrowPress { ArrowKey flecha; bool ctrl; bool shift; };
		std::vector<ArrowPress> pendingArrows;
		unsigned teclasFlechaPulsadas = 0;      // máscara de flechas mantenidas: mientras haya alguna, es UNA operación de historial
		bool haySeleccionValida = false;

		// Instrumentación del gizmo (F1 del editor: casilla "Debug input" o variable ESCAPETHEUAB_EDITOR_DEBUG=1).
		bool debugInput = false;
		bool matrizCambiada = false;
		bool consoleDebugging = false;
		bool depuracionIniciada = false;
		int operacionDepuracion = 0;
		bool gizmoSobre = false;
		std::string statusMessage;
		std::string placementMessage;
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
		CargadorModelo modelLoader;
		CargadorTextura textureLoader;
		CargadorPuerta doorLoader;
		std::filesystem::path modelsRoot = "assets/models";
		std::filesystem::path sceneRoot;
		std::filesystem::path animationsRoot = "assets/animations";
		std::vector<std::filesystem::path> availableModels;
		bool inventoryLoaded = false;
		std::string selectedAsset;
		char assetFilter[64] = {};
		char textureFilter[128] = {};
		int textureCategory = 0;
		int paredDestinoPuerta = -1;
		std::vector<std::filesystem::path> availableDoors;
		bool inventarioPuertasCargado = false;
		bool assetIsDoor = false;
		std::filesystem::path selectedDoor;
		bool assetYArriba = true;
		bool assetCollider = false;
		bool assetDynamic = false;
	};
}
