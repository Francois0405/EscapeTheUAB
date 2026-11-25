include_directories("${PROJECT_SOURCE_DIR}")
find_package(glm CONFIG REQUIRED)
file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}" ENTORNVGI_TEST_SOURCE_ROOT)
file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}/bin" ENTORNVGI_TEST_RUNTIME_ROOT)

add_executable(GrabDiagnosticsTests
	GrabDiagnosticsTests.cpp
)
target_link_libraries(GrabDiagnosticsTests PRIVATE EntornVGI_grab_diagnostics)
target_compile_features(GrabDiagnosticsTests PRIVATE cxx_std_20)
add_test(NAME GrabDiagnosticsTests COMMAND GrabDiagnosticsTests)

add_executable(SceneLoaderTests
	SceneLoaderTests.cpp
)
target_include_directories(SceneLoaderTests PRIVATE
	${PROJECT_SOURCE_DIR}/EntornVGI
)
target_compile_definitions(SceneLoaderTests PRIVATE
	ESCAPETHEUAB_SOURCE_DIR="${ENTORNVGI_TEST_SOURCE_ROOT}"
)
if(TARGET glm::glm)
	target_link_libraries(SceneLoaderTests PRIVATE glm::glm EntornVGI_scene_loader)
else()
	target_link_libraries(SceneLoaderTests PRIVATE glm EntornVGI_scene_loader)
endif()
target_compile_features(SceneLoaderTests PRIVATE cxx_std_20)

add_test(NAME SceneLoaderTests COMMAND SceneLoaderTests)

add_executable(EditorPersistenceTests
	EditorPersistenceTests.cpp
	${PROJECT_SOURCE_DIR}/src/editor/SerializadorEscenas.cpp
)
target_include_directories(EditorPersistenceTests PRIVATE
	${PROJECT_SOURCE_DIR}/EntornVGI
	${PROJECT_SOURCE_DIR}/third_party
)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_BINARY_DIR}/EditorPersistenceRuntimeAssets" EDITOR_PERSISTENCE_RUNTIME_ASSETS)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_BINARY_DIR}/EditorPersistenceSourceAssets" EDITOR_PERSISTENCE_SOURCE_ASSETS)
target_compile_definitions(EditorPersistenceTests PRIVATE
	ENTORNVGI_RUNTIME_ASSETS_DIR="${EDITOR_PERSISTENCE_RUNTIME_ASSETS}"
	ENTORNVGI_SOURCE_ASSETS_DIR="${EDITOR_PERSISTENCE_SOURCE_ASSETS}"
)
target_compile_features(EditorPersistenceTests PRIVATE cxx_std_20)
if(TARGET glm::glm)
	target_link_libraries(EditorPersistenceTests PRIVATE glm::glm EntornVGI_scene_loader)
else()
	target_link_libraries(EditorPersistenceTests PRIVATE glm EntornVGI_scene_loader)
endif()
add_test(NAME EditorPersistenceTests COMMAND EditorPersistenceTests)

add_executable(EditorTests
	EditorTests.cpp
	${PROJECT_SOURCE_DIR}/src/editor/SerializadorEscenas.cpp
)
target_include_directories(EditorTests PRIVATE
	${PROJECT_SOURCE_DIR}/EntornVGI
	${PROJECT_SOURCE_DIR}/third_party
)
target_compile_definitions(EditorTests PRIVATE GLM_ENABLE_EXPERIMENTAL)
target_compile_features(EditorTests PRIVATE cxx_std_20)
if(TARGET glm::glm)
	target_link_libraries(EditorTests PRIVATE glm::glm EntornVGI_editor_core EntornVGI_scene_loader EntornVGI_glb_model)
else()
	target_link_libraries(EditorTests PRIVATE glm EntornVGI_editor_core EntornVGI_scene_loader EntornVGI_glb_model)
endif()
add_test(NAME EditorTests COMMAND EditorTests)

add_executable(EditorPlacementTests
	EditorPlacementTests.cpp
)
target_include_directories(EditorPlacementTests PRIVATE ${PROJECT_SOURCE_DIR}/EntornVGI)
target_compile_features(EditorPlacementTests PRIVATE cxx_std_20)
if(TARGET glm::glm)
	target_link_libraries(EditorPlacementTests PRIVATE glm::glm EntornVGI_editor_core)
else()
	target_link_libraries(EditorPlacementTests PRIVATE glm EntornVGI_editor_core)
endif()
add_test(NAME EditorPlacementTests COMMAND EditorPlacementTests)

# Prueba de integración sin ventana: ejecuta ImGuizmo sobre un contexto ImGui real
# para comprobar que el gesto que selecciona nunca se convierte en un arrastre.
add_executable(EditorGizmoInteractionTests
	EditorGizmoInteractionTests.cpp
	${PROJECT_SOURCE_DIR}/third_party/imguizmo/ImGuizmo.cpp
	${PROJECT_SOURCE_DIR}/third_party/imgui/imgui.cpp
	${PROJECT_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
	${PROJECT_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp
	${PROJECT_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
)
target_include_directories(EditorGizmoInteractionTests PRIVATE
	${PROJECT_SOURCE_DIR}/EntornVGI
	${PROJECT_SOURCE_DIR}/third_party/imguizmo
	${PROJECT_SOURCE_DIR}/third_party/imgui
)
target_compile_features(EditorGizmoInteractionTests PRIVATE cxx_std_20)
if(TARGET glm::glm)
	target_link_libraries(EditorGizmoInteractionTests PRIVATE glm::glm)
else()
	target_link_libraries(EditorGizmoInteractionTests PRIVATE glm)
endif()
add_test(NAME EditorGizmoInteractionTests COMMAND EditorGizmoInteractionTests)

add_executable(GlbTests
	GlbTests.cpp
)
target_include_directories(GlbTests PRIVATE
	${PROJECT_SOURCE_DIR}/EntornVGI
)
target_compile_definitions(GlbTests PRIVATE
	GLM_ENABLE_EXPERIMENTAL
	ESCAPETHEUAB_SOURCE_DIR="${ENTORNVGI_TEST_SOURCE_ROOT}"
	ESCAPETHEUAB_RUNTIME_ASSET_ROOT="${ENTORNVGI_TEST_RUNTIME_ROOT}"
)
target_link_libraries(GlbTests PRIVATE EntornVGI_animated_glb)
target_compile_features(GlbTests PRIVATE cxx_std_20)

add_test(NAME GlbTests COMMAND GlbTests)

add_executable(PhysicsTests
	PhysicsTests.cpp
)
target_include_directories(PhysicsTests PRIVATE
	${PROJECT_SOURCE_DIR}/EntornVGI
)
target_compile_definitions(PhysicsTests PRIVATE
	GLM_ENABLE_EXPERIMENTAL
	ESCAPETHEUAB_SOURCE_DIR="${ENTORNVGI_TEST_SOURCE_ROOT}"
	ESCAPETHEUAB_RUNTIME_ASSET_ROOT="${ENTORNVGI_TEST_RUNTIME_ROOT}"
)
target_link_libraries(PhysicsTests PRIVATE EntornVGI_physics)
target_compile_features(PhysicsTests PRIVATE cxx_std_20)

add_test(NAME PhysicsTests COMMAND PhysicsTests)

find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(GLEW REQUIRED)

if(TARGET glfw)
	set(TEST_GLFW_TARGET glfw)
else()
	set(TEST_GLFW_TARGET glfw3::glfw)
endif()
if(TARGET GLEW::GLEW)
	set(TEST_GLEW_TARGET GLEW::GLEW)
else()
	set(TEST_GLEW_TARGET unofficial::glew::glew)
endif()

add_executable(ObjLoaderTests
	ObjLoaderTests.cpp
	${PROJECT_SOURCE_DIR}/src/scene/objLoader.cpp
	${PROJECT_SOURCE_DIR}/src/rendering/ObjMeshOptimization.cpp
	${PROJECT_SOURCE_DIR}/src/rendering/glut_geometry.cpp
	${PROJECT_SOURCE_DIR}/src/rendering/material.cpp
)
target_include_directories(ObjLoaderTests PRIVATE
	${PROJECT_SOURCE_DIR}/EntornVGI
	${PROJECT_SOURCE_DIR}/src/legacy_vgi
	${PROJECT_SOURCE_DIR}/src/scene
	${PROJECT_SOURCE_DIR}/src/platform
	${PROJECT_SOURCE_DIR}/third_party/imgui
	${PROJECT_SOURCE_DIR}/third_party/SOIL2/src
)
target_compile_definitions(ObjLoaderTests PRIVATE
	GLM_ENABLE_EXPERIMENTAL
	ESCAPETHEUAB_SOURCE_DIR="${ENTORNVGI_TEST_SOURCE_ROOT}"
)
target_link_libraries(ObjLoaderTests PRIVATE
	OpenGL::GL
	${TEST_GLFW_TARGET}
	${TEST_GLEW_TARGET}
	soil2
)
target_compile_features(ObjLoaderTests PRIVATE cxx_std_20)
if(ENTORNVGI_MESHOPT_TARGET)
    target_link_libraries(ObjLoaderTests PRIVATE ${ENTORNVGI_MESHOPT_TARGET})
    target_compile_definitions(ObjLoaderTests PRIVATE ENTORNVGI_HAS_MESHOPTIMIZER=1)
endif()

add_executable(ObjMeshOptimizationTests
    ObjMeshOptimizationTests.cpp
    ${PROJECT_SOURCE_DIR}/src/rendering/ObjMeshOptimization.cpp
)
target_include_directories(ObjMeshOptimizationTests PRIVATE ${PROJECT_SOURCE_DIR}/EntornVGI)
target_compile_features(ObjMeshOptimizationTests PRIVATE cxx_std_20)
if(ENTORNVGI_MESHOPT_TARGET)
    target_link_libraries(ObjMeshOptimizationTests PRIVATE ${ENTORNVGI_MESHOPT_TARGET})
    target_compile_definitions(ObjMeshOptimizationTests PRIVATE ENTORNVGI_HAS_MESHOPTIMIZER=1)
endif()
add_test(NAME ObjMeshOptimizationTests COMMAND ObjMeshOptimizationTests)

if(WIN32)
	add_custom_command(TARGET ObjLoaderTests POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_RUNTIME_DLLS:ObjLoaderTests>
			"$<TARGET_FILE_DIR:ObjLoaderTests>"
		COMMAND_EXPAND_LISTS
	)
endif()

add_test(NAME ObjLoaderTests COMMAND ObjLoaderTests)
set_tests_properties(ObjLoaderTests PROPERTIES SKIP_RETURN_CODE 77)

if(WIN32)
	add_executable(LauncherTestStub LauncherTestStub.cpp)
	target_compile_features(LauncherTestStub PRIVATE cxx_std_20)
	add_test(NAME WindowsLauncherDoesNotRunStaleBuild
		COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
			-File "${CMAKE_CURRENT_SOURCE_DIR}/WindowsLauncherFailureTest.ps1"
			-LauncherPath "${PROJECT_SOURCE_DIR}/run-windows.ps1"
			-StubPath "$<TARGET_FILE:LauncherTestStub>"
	)
endif()

# OpenXR: pruebas SIN visor ni runtime (seleccion de modo, ausencia de runtime, activacion explicita,
# limpieza tras un fallo). Las que necesitan un casco real NO estan aqui: ver docs/OPENXR.md.
add_executable(OpenXRTests
	OpenXRTests.cpp
	${PROJECT_SOURCE_DIR}/src/vr/OpenXRManager.cpp
)
target_include_directories(OpenXRTests PRIVATE ${PROJECT_SOURCE_DIR}/EntornVGI)
# OpenXRManager.cpp usa GLEW/OpenGL para los framebuffers de los swapchains (solo se llaman con sesion real).
target_link_libraries(OpenXRTests PRIVATE EntornVGI_openxr OpenGL::GL ${TEST_GLEW_TARGET})
if(WIN32)
	add_custom_command(TARGET OpenXRTests POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_RUNTIME_DLLS:OpenXRTests>
			"$<TARGET_FILE_DIR:OpenXRTests>"
		COMMAND_EXPAND_LISTS
	)
endif()
target_compile_features(OpenXRTests PRIVATE cxx_std_20)
add_test(NAME OpenXRTests COMMAND OpenXRTests)

# Matematica de la vista VR (OpenXR -> motor con Z arriba, proyeccion asimetrica): sin visor ni ventana.
add_executable(VistaVRTests
	VistaVRTests.cpp
)
target_include_directories(VistaVRTests PRIVATE ${PROJECT_SOURCE_DIR}/EntornVGI)
if(TARGET glm::glm)
	target_link_libraries(VistaVRTests PRIVATE glm::glm)
else()
	target_link_libraries(VistaVRTests PRIVATE glm)
endif()
target_compile_features(VistaVRTests PRIVATE cxx_std_20)
add_test(NAME VistaVRTests COMMAND VistaVRTests)

# Velocidad, sprint, estamina y balanceo de camara (sin ventana ni fisica).
add_executable(MovimientoFPSTests
	MovimientoFPSTests.cpp
)
target_include_directories(MovimientoFPSTests PRIVATE ${PROJECT_SOURCE_DIR}/EntornVGI)
target_compile_features(MovimientoFPSTests PRIVATE cxx_std_20)
add_test(NAME MovimientoFPSTests COMMAND MovimientoFPSTests)

# Formula de escala logica y DPI de la interfaz, sin ventana ni contexto OpenGL.
add_executable(EscalaUITests EscalaUITests.cpp)
target_include_directories(EscalaUITests PRIVATE ${PROJECT_SOURCE_DIR}/EntornVGI)
target_compile_features(EscalaUITests PRIVATE cxx_std_20)
add_test(NAME EscalaUITests COMMAND EscalaUITests)

# Console status bar text and multiplayer panel decisions extracted from main.cpp (no window, no OpenGL context).
add_executable(RuntimeUiTests
	RuntimeUiTests.cpp
	${PROJECT_SOURCE_DIR}/src/ui/StatusBar.cpp
)
target_include_directories(RuntimeUiTests PRIVATE ${PROJECT_SOURCE_DIR}/EntornVGI)
# GLEW headers only provide the GL scalar types; NetJuegoLogica needs the network headers.
target_link_libraries(RuntimeUiTests PRIVATE ${TEST_GLEW_TARGET} EntornVGI_network_game)
target_compile_features(RuntimeUiTests PRIVATE cxx_std_20)
add_test(NAME RuntimeUiTests COMMAND RuntimeUiTests)

# Descubrimiento por mirada y animacion de Hiriti (sin ventana ni fisica).
add_executable(AnimacionHiritiTests
	AnimacionHiritiTests.cpp
)
target_include_directories(AnimacionHiritiTests PRIVATE ${PROJECT_SOURCE_DIR}/EntornVGI)
target_compile_features(AnimacionHiritiTests PRIVATE cxx_std_20)
if(TARGET glm::glm)
	target_link_libraries(AnimacionHiritiTests PRIVATE glm::glm)
else()
	target_link_libraries(AnimacionHiritiTests PRIVATE glm)
endif()
add_test(NAME AnimacionHiritiTests COMMAND AnimacionHiritiTests)

# Comprueba que miniaudio puede decodificar los sonidos OGG de Hirity.
add_executable(AudioDecoderTests
	AudioDecoderTests.cpp
	"${ENTORNVGI_STB_IMPLEMENTATION_SOURCE}"
)
target_include_directories(AudioDecoderTests PRIVATE
	"${ENTORNVGI_STB_INCLUDE_DIR}"
	"${PROJECT_SOURCE_DIR}/EntornVGI/third_party"
	"${PROJECT_SOURCE_DIR}/third_party/miniaudio"
)
target_compile_definitions(AudioDecoderTests PRIVATE
	ENTORNVGI_SOURCE_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets"
)
target_compile_features(AudioDecoderTests PRIVATE cxx_std_20)
add_test(NAME AudioDecoderTests COMMAND AudioDecoderTests)

# Capa de red (protocolo, sesion, servicio thread-safe) con transporte en memoria: sin ENet ni sockets.
add_executable(NetworkTests
	NetworkTests.cpp
)
target_link_libraries(NetworkTests PRIVATE EntornVGI_network)
add_test(NAME NetworkTests COMMAND NetworkTests)

# Adaptador de red del juego (poses PC/OpenXR, interpolacion, puertas con autoridad del host, propiedad temporal):
# transporte en memoria, sin ENet, sin OpenGL, sin Bullet y sin visor.
add_executable(NetJuegoTests
	NetJuegoTests.cpp
)
target_include_directories(NetJuegoTests PRIVATE ${PROJECT_SOURCE_DIR}/EntornVGI)
target_link_libraries(NetJuegoTests PRIVATE EntornVGI_network_game EntornVGI_scene_loader)
target_compile_definitions(NetJuegoTests PRIVATE
	ENTORNVGI_SOURCE_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets"
)
target_compile_features(NetJuegoTests PRIVATE cxx_std_20)
add_test(NAME NetJuegoTests COMMAND NetJuegoTests)

# Persistencia del ultimo destino de red: usa una ruta temporal aislada.
add_executable(NetworkPreferencesTests
	NetworkPreferencesTests.cpp
)
target_include_directories(NetworkPreferencesTests PRIVATE ${PROJECT_SOURCE_DIR}/EntornVGI)
target_link_libraries(NetworkPreferencesTests PRIVATE EntornVGI_network_preferences)
target_compile_features(NetworkPreferencesTests PRIVATE cxx_std_20)
add_test(NAME NetworkPreferencesTests COMMAND NetworkPreferencesTests)

# Bucle local con ENet REAL (127.0.0.1). Solo si la capa de red encontro ENet; sin ENet no se registra.
if(TARGET EntornVGI_network AND ENTORNVGI_NETWORK_TIENE_ENET)
	add_executable(ENetLoopbackTests ENetLoopbackTests.cpp)
	target_link_libraries(ENetLoopbackTests PRIVATE EntornVGI_network)
	add_test(NAME ENetLoopbackTests COMMAND ENetLoopbackTests)
endif()
