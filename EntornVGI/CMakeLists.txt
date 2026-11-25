
set(REPO_ROOT "${PROJECT_SOURCE_DIR}")
set(ENTORNVGI_SOURCE_RESOURCE_ROOT "${REPO_ROOT}/EntornVGI")
set(ENTORNVGI_SOURCE_ASSETS_DIR "${ENTORNVGI_SOURCE_RESOURCE_ROOT}/assets")
set(ENTORNVGI_SOURCE_SHADERS_DIR "${ENTORNVGI_SOURCE_RESOURCE_ROOT}/shaders")
set(ENTORNVGI_UI_RESOURCE_ROOT "${REPO_ROOT}/res/ui")
if(NOT IS_DIRECTORY "${ENTORNVGI_SOURCE_ASSETS_DIR}")
	message(FATAL_ERROR "EscapeTheUAB source asset root is missing: ${ENTORNVGI_SOURCE_ASSETS_DIR}")
endif()
if(NOT IS_DIRECTORY "${ENTORNVGI_UI_RESOURCE_ROOT}")
	message(FATAL_ERROR "EscapeTheUAB UI resource root is missing: ${ENTORNVGI_UI_RESOURCE_ROOT}")
endif()
foreach(required_resource IN ITEMS
	"${ENTORNVGI_SOURCE_SHADERS_DIR}/gouraud_shdrML.vert"
	"${ENTORNVGI_SOURCE_ASSETS_DIR}/scenes/aula_informatica_calibrada.json"
)
	if(NOT EXISTS "${required_resource}")
		message(FATAL_ERROR "EscapeTheUAB required resource is missing: ${required_resource}")
	endif()
endforeach()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(IMGUI_DIR "${REPO_ROOT}/include/ImGui")

if(APPLE)
	# Prefer the system OpenGL.framework over XQuartz (/opt/X11), which has no Cocoa context.
	set(CMAKE_FIND_FRAMEWORK FIRST)
endif()
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

if(ENTORNVGI_USE_SYSTEM_DEPENDENCIES)
	if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
		message(FATAL_ERROR "System dependency mode is currently supported only on Linux; use vcpkg on Windows and macOS.")
	endif()
	find_package(glfw3 3.3 REQUIRED)
	find_package(GLEW REQUIRED)
	find_package(glm CONFIG REQUIRED)

	add_subdirectory("${REPO_ROOT}/third_party/nativefiledialog" "${CMAKE_BINARY_DIR}/third_party/nativefiledialog")
	set(ENTORNVGI_NFD_TARGET nfd)
else()
	find_package(glfw3 CONFIG REQUIRED)
	find_package(GLEW CONFIG REQUIRED)
	find_package(glm CONFIG REQUIRED)
	find_package(nfd CONFIG REQUIRED)
	if(TARGET nfd::nfd)
		set(ENTORNVGI_NFD_TARGET nfd::nfd)
	elseif(TARGET nfd)
		set(ENTORNVGI_NFD_TARGET nfd)
	else()
		message(FATAL_ERROR "The nativefiledialog-extended package did not define a supported NFD target.")
	endif()
endif()

add_subdirectory("${REPO_ROOT}/third_party/SOIL2" "${CMAKE_BINARY_DIR}/third_party/SOIL2")

option(ENTORNVGI_ENABLE_NETWORK "Compilar la capa de red (ENet opcional)" ON)
option(ENTORNVGI_REQUIRE_ENET "Fallar la configuracion si ENet no esta disponible" OFF)
add_subdirectory("${REPO_ROOT}/src/networking/transport" "${CMAKE_BINARY_DIR}/EntornVGI/networking/transport")

set(ENTORNVGI_SOURCES
	AudioManager.cpp
	DibujoColisionadores.cpp
	DibujoGLB.cpp
	Editor/EditorEscenas.cpp
	Editor/SerializadorEscenas.cpp
	escena.cpp
	glut_geometry.cpp
	third_party/ImGuizmo.cpp
	main.cpp
	material.cpp
	NetAvatar.cpp
	objLoader.cpp
	ObjMeshOptimization.cpp
	OpenXRManager.cpp
	quatern.cpp
	shader.cpp
	visualitzacio.cpp
)

set(IMGUI_SOURCES
	${IMGUI_DIR}/imgui.cpp
	${IMGUI_DIR}/imgui_demo.cpp
	${IMGUI_DIR}/imgui_draw.cpp
	${IMGUI_DIR}/imgui_impl_glfw.cpp
	${IMGUI_DIR}/imgui_impl_opengl3.cpp
	${IMGUI_DIR}/imgui_tables.cpp
	${IMGUI_DIR}/imgui_widgets.cpp
)

add_executable(EntornVGI ${ENTORNVGI_SOURCES} ${IMGUI_SOURCES})
target_sources(EntornVGI PRIVATE "${ENTORNVGI_STB_IMPLEMENTATION_SOURCE}")
if(ENTORNVGI_MESHOPT_TARGET)
    target_link_libraries(EntornVGI PRIVATE ${ENTORNVGI_MESHOPT_TARGET})
    target_compile_definitions(EntornVGI PRIVATE ENTORNVGI_HAS_MESHOPTIMIZER=1)
endif()

target_include_directories(EntornVGI PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${IMGUI_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/third_party
	${REPO_ROOT}/third_party/SOIL2/src
	${REPO_ROOT}/third_party/cgltf
	${REPO_ROOT}/third_party/miniaudio
	${ENTORNVGI_STB_INCLUDE_DIR}
)

target_compile_definitions(EntornVGI PRIVATE
	GLM_ENABLE_EXPERIMENTAL
	ENTORNVGI_RUNTIME_ASSETS_DIR="${CMAKE_BINARY_DIR}/bin/assets"
	ENTORNVGI_SOURCE_ASSETS_DIR="${ENTORNVGI_SOURCE_ASSETS_DIR}"
)

if(ENTORNVGI_USE_SYSTEM_DEPENDENCIES)
	target_compile_definitions(EntornVGI PRIVATE ENTORNVGI_USE_LEGACY_NFD)
	target_link_libraries(EntornVGI PRIVATE nfd)
else()
	target_compile_definitions(EntornVGI PRIVATE ENTORNVGI_USE_NFD_EXTENDED)
	set_property(TARGET EntornVGI PROPERTY NO_SYSTEM_FROM_IMPORTED ON)
	# The repository also contains an older include/ImGui/nfd.h. Resolve the
	# Extended header path from its imported target instead of assuming vcpkg
	# toolchain variables are set by every CMake caller.
	get_target_property(ENTORNVGI_NFD_INCLUDE_DIRS
		${ENTORNVGI_NFD_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
	if(ENTORNVGI_NFD_INCLUDE_DIRS)
		target_include_directories(EntornVGI BEFORE PRIVATE ${ENTORNVGI_NFD_INCLUDE_DIRS})
	else()
		message(FATAL_ERROR "The Native File Dialog target did not provide its include directory.")
	endif()
	target_link_libraries(EntornVGI PRIVATE ${ENTORNVGI_NFD_TARGET})
endif()

if(TARGET glfw)
	set(ENTORNVGI_GLFW_TARGET glfw)
elseif(TARGET glfw3::glfw)
	set(ENTORNVGI_GLFW_TARGET glfw3::glfw)
else()
	message(FATAL_ERROR "The GLFW package did not define a supported target.")
endif()

if(APPLE)
	# vcpkg's GLEW records XQuartz's libGL as a dependency; drop it so only OpenGL.framework is linked.
	foreach(_glew_tgt GLEW::GLEW GLEW::glew GLEW::glew_s)
		if(TARGET ${_glew_tgt})
			get_target_property(_glew_libs ${_glew_tgt} INTERFACE_LINK_LIBRARIES)
			if(_glew_libs)
				list(FILTER _glew_libs EXCLUDE REGEX "X11")
				set_target_properties(${_glew_tgt} PROPERTIES INTERFACE_LINK_LIBRARIES "${_glew_libs}")
			endif()
		endif()
	endforeach()
endif()

if(TARGET GLEW::GLEW)
	set(ENTORNVGI_GLEW_TARGET GLEW::GLEW)
elseif(TARGET unofficial::glew::glew)
	set(ENTORNVGI_GLEW_TARGET unofficial::glew::glew)
else()
	message(FATAL_ERROR "The GLEW package did not define a supported target.")
endif()

if(TARGET glm::glm)
	set(ENTORNVGI_GLM_TARGET glm::glm)
elseif(TARGET glm)
	set(ENTORNVGI_GLM_TARGET glm)
else()
	message(FATAL_ERROR "The GLM package did not define a supported target.")
endif()

add_library(EntornVGI_scene_loader STATIC
	SceneLoader.cpp
)
target_include_directories(EntornVGI_scene_loader PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(EntornVGI_scene_loader PUBLIC ${ENTORNVGI_GLM_TARGET})
target_compile_features(EntornVGI_scene_loader PUBLIC cxx_std_20)
target_compile_definitions(EntornVGI_scene_loader PRIVATE GLM_ENABLE_EXPERIMENTAL)

add_library(EntornVGI_glb_model STATIC
	ModeloGLB.cpp
)
target_include_directories(EntornVGI_glb_model
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
	PRIVATE ${REPO_ROOT}/third_party/cgltf
)
target_link_libraries(EntornVGI_glb_model PUBLIC ${ENTORNVGI_GLM_TARGET})
target_compile_features(EntornVGI_glb_model PUBLIC cxx_std_20)
target_compile_definitions(EntornVGI_glb_model PRIVATE GLM_ENABLE_EXPERIMENTAL)

add_library(EntornVGI_animated_glb STATIC
	ObjetosAnimadosGLB.cpp
)
target_include_directories(EntornVGI_animated_glb PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(EntornVGI_animated_glb PUBLIC
	${ENTORNVGI_GLM_TARGET}
	EntornVGI_glb_model
)
target_compile_features(EntornVGI_animated_glb PUBLIC cxx_std_20)
target_compile_definitions(EntornVGI_animated_glb PRIVATE GLM_ENABLE_EXPERIMENTAL)

add_library(EntornVGI_physics STATIC
	MundoFisico.cpp
	EscenaFisica.cpp
)
target_include_directories(EntornVGI_physics PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(EntornVGI_physics PUBLIC
	${ENTORNVGI_GLM_TARGET}
	EntornVGI_bullet
	EntornVGI_scene_loader
	EntornVGI_animated_glb
)
target_compile_features(EntornVGI_physics PUBLIC cxx_std_20)
target_compile_definitions(EntornVGI_physics PRIVATE GLM_ENABLE_EXPERIMENTAL)

add_library(EntornVGI_grab_diagnostics STATIC Network/GrabDiagnostics.cpp)
target_include_directories(EntornVGI_grab_diagnostics PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/Network
)
target_compile_features(EntornVGI_grab_diagnostics PUBLIC cxx_std_20)

add_library(EntornVGI_network_preferences STATIC Network/PreferenciasRed.cpp)
target_include_directories(EntornVGI_network_preferences PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(EntornVGI_network_preferences PUBLIC cxx_std_20)

add_library(EntornVGI_network_game STATIC NetJuego.cpp)
target_include_directories(EntornVGI_network_game PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(EntornVGI_network_game PUBLIC
	EntornVGI_network
	${ENTORNVGI_GLM_TARGET}
)
target_compile_features(EntornVGI_network_game PUBLIC cxx_std_20)
target_compile_definitions(EntornVGI_network_game PRIVATE GLM_ENABLE_EXPERIMENTAL)

add_library(EntornVGI_editor_core STATIC
	Editor/LimitesModelo.cpp
	Editor/SelectorObjetos.cpp
	Editor/ValidadorColocacion.cpp
	Editor/VisibilidadEditor.cpp
)
target_include_directories(EntornVGI_editor_core PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(EntornVGI_editor_core PUBLIC
	${ENTORNVGI_GLM_TARGET}
	EntornVGI_glb_model
	EntornVGI_scene_loader
)
target_compile_features(EntornVGI_editor_core PUBLIC cxx_std_20)
target_compile_definitions(EntornVGI_editor_core PRIVATE
	GLM_ENABLE_EXPERIMENTAL
)

target_link_libraries(EntornVGI PRIVATE
	OpenGL::GL
	${ENTORNVGI_GLFW_TARGET}
	${ENTORNVGI_GLEW_TARGET}
	${ENTORNVGI_GLM_TARGET}
	soil2
	EntornVGI_bullet
	EntornVGI_openxr
	EntornVGI_network_game
	EntornVGI_network
	EntornVGI_scene_loader
	EntornVGI_glb_model
	EntornVGI_editor_core
	EntornVGI_animated_glb
	EntornVGI_physics
	EntornVGI_grab_diagnostics
	EntornVGI_network_preferences
	Threads::Threads
	${CMAKE_DL_LIBS}
)

if(APPLE)
	target_link_libraries(EntornVGI PRIVATE
		"-framework CoreAudio"
		"-framework AudioToolbox"
		"-framework CoreFoundation"
	)
endif()

if(WIN32)
	target_compile_definitions(EntornVGI PRIVATE NOMINMAX)
	target_compile_definitions(EntornVGI_grab_diagnostics PRIVATE NOMINMAX)
	target_compile_definitions(EntornVGI_physics PRIVATE NOMINMAX)
	target_compile_definitions(EntornVGI_animated_glb PRIVATE NOMINMAX)
	target_compile_definitions(EntornVGI_network_preferences PRIVATE NOMINMAX)
	target_compile_definitions(EntornVGI_network_game PRIVATE NOMINMAX)
	add_custom_command(TARGET EntornVGI POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_RUNTIME_DLLS:EntornVGI>
			"$<TARGET_FILE_DIR:EntornVGI>"
		COMMAND_EXPAND_LISTS
		COMMENT "Copying runtime DLL dependencies next to EntornVGI"
	)
endif()

function(entornvgi_enable_warnings target)
	if(MSVC)
		target_compile_options(${target} PRIVATE /W3)
	else()
		target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unused-parameter)
	endif()
endfunction()

foreach(target IN ITEMS
	EntornVGI
	EntornVGI_scene_loader
	EntornVGI_glb_model
	EntornVGI_editor_core
	EntornVGI_animated_glb
	EntornVGI_physics
	EntornVGI_grab_diagnostics
	EntornVGI_network_preferences
	EntornVGI_network_game
)
	entornvgi_enable_warnings(${target})
endforeach()

set_target_properties(EntornVGI PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Treat every runtime asset as a build input. This updates a changed JSON,
# model, texture or shader without relinking the executable, while
# copy_if_different avoids rewriting unchanged large assets.
set(ENTORNVGI_RUNTIME_RESOURCE_OUTPUTS)
foreach(resource assets shaders textures)
	if(resource STREQUAL "assets")
		set(resource_source "${ENTORNVGI_SOURCE_ASSETS_DIR}")
	else()
		set(resource_source "${CMAKE_CURRENT_SOURCE_DIR}/${resource}")
	endif()
	file(GLOB_RECURSE resource_files CONFIGURE_DEPENDS
		LIST_DIRECTORIES false
		"${resource_source}/*"
	)
	foreach(source_file IN LISTS resource_files)
		file(RELATIVE_PATH resource_relative_path
			"${resource_source}"
			"${source_file}"
		)
		set(destination_file "${CMAKE_BINARY_DIR}/bin/${resource}/${resource_relative_path}")
		get_filename_component(destination_directory "${destination_file}" DIRECTORY)
		add_custom_command(OUTPUT "${destination_file}"
			COMMAND ${CMAKE_COMMAND} -E make_directory "${destination_directory}"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${source_file}" "${destination_file}"
			DEPENDS "${source_file}"
			VERBATIM
			COMMENT "Syncing ${resource}/${resource_relative_path}"
		)
		list(APPEND ENTORNVGI_RUNTIME_RESOURCE_OUTPUTS "${destination_file}")
	endforeach()
endforeach()
set(hand_texture_source "${ENTORNVGI_UI_RESOURCE_ROOT}/hand.png")
set(hand_texture_output "${CMAKE_BINARY_DIR}/bin/res/ui/hand.png")
add_custom_command(OUTPUT "${hand_texture_output}"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/res/ui"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different "${hand_texture_source}" "${hand_texture_output}"
	DEPENDS "${hand_texture_source}"
	VERBATIM
	COMMENT "Syncing res/ui/hand.png"
)
list(APPEND ENTORNVGI_RUNTIME_RESOURCE_OUTPUTS "${hand_texture_output}")
add_custom_target(EntornVGI_runtime_resources ALL
	DEPENDS ${ENTORNVGI_RUNTIME_RESOURCE_OUTPUTS}
)
add_dependencies(EntornVGI EntornVGI_runtime_resources)
