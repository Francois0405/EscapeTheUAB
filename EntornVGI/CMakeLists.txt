set(REPO_ROOT "${PROJECT_SOURCE_DIR}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(IMGUI_DIR "${REPO_ROOT}/include/ImGui")

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

set(ENTORNVGI_SOURCES
	escena.cpp
	glut_geometry.cpp
	main.cpp
	material.cpp
	objLoader.cpp
	quatern.cpp
	SceneLoader.cpp
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

target_include_directories(EntornVGI PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${IMGUI_DIR}
	${REPO_ROOT}/third_party/SOIL2/src
)

target_compile_definitions(EntornVGI PRIVATE
	GLM_ENABLE_EXPERIMENTAL
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

target_link_libraries(EntornVGI PRIVATE
	OpenGL::GL
	${ENTORNVGI_GLFW_TARGET}
	${ENTORNVGI_GLEW_TARGET}
	${ENTORNVGI_GLM_TARGET}
	soil2
	Threads::Threads
	${CMAKE_DL_LIBS}
)

if(WIN32)
	add_custom_command(TARGET EntornVGI POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_RUNTIME_DLLS:EntornVGI>
			"$<TARGET_FILE_DIR:EntornVGI>"
		COMMAND_EXPAND_LISTS
		COMMENT "Copying runtime DLL dependencies next to EntornVGI"
	)
endif()

# Avisos activados pero nunca tratados como errores.
if(MSVC)
	target_compile_options(EntornVGI PRIVATE /W3)
else()
	target_compile_options(EntornVGI PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

set_target_properties(EntornVGI PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Treat every runtime asset as a build input. This updates a changed JSON,
# model, texture or shader without relinking the executable, while
# copy_if_different avoids rewriting unchanged large assets.
set(ENTORNVGI_RUNTIME_RESOURCE_OUTPUTS)
foreach(resource assets shaders textures)
	file(GLOB_RECURSE resource_files CONFIGURE_DEPENDS
		LIST_DIRECTORIES false
		"${CMAKE_CURRENT_SOURCE_DIR}/${resource}/*"
	)
	foreach(source_file IN LISTS resource_files)
		file(RELATIVE_PATH resource_relative_path
			"${CMAKE_CURRENT_SOURCE_DIR}/${resource}"
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
add_custom_target(EntornVGI_runtime_resources ALL
	DEPENDS ${ENTORNVGI_RUNTIME_RESOURCE_OUTPUTS}
)
add_dependencies(EntornVGI EntornVGI_runtime_resources)
