cmake_minimum_required(VERSION 3.25)

project(EntornVGI LANGUAGES C CXX)

include(CTest)
find_path(ENTORNVGI_STB_INCLUDE_DIR
    NAMES stb_vorbis.c stb_vorbis.h
    PATH_SUFFIXES stb
    REQUIRED
)
if(EXISTS "${ENTORNVGI_STB_INCLUDE_DIR}/stb_vorbis.c")
    set(ENTORNVGI_STB_IMPLEMENTATION_SOURCE "${ENTORNVGI_STB_INCLUDE_DIR}/stb_vorbis.c")
else()
    set(ENTORNVGI_STB_IMPLEMENTATION_SOURCE "${PROJECT_SOURCE_DIR}/EntornVGI/third_party/stb_vorbis_impl.c")
endif()

# Bullet Physics (fisica). vcpkg (Windows/macOS) y libbullet-dev (Linux) instalan el
# mismo BulletConfig.cmake, que define BULLET_INCLUDE_DIRS y BULLET_LIBRARIES.
# Se agrupa en un unico target para que el juego y las pruebas enlacen igual.
# Linux: utilizar el módulo FindBullet de CMake.
# Windows/macOS: utilizar el paquete CONFIG de vcpkg.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Bullet MODULE REQUIRED)
else()
    find_package(Bullet CONFIG REQUIRED)
endif()
add_library(EntornVGI_bullet INTERFACE)
target_include_directories(EntornVGI_bullet SYSTEM INTERFACE ${BULLET_INCLUDE_DIRS})
target_link_libraries(EntornVGI_bullet INTERFACE ${BULLET_LIBRARIES})

# OpenXR (VR real). Es OPCIONAL: sin OpenXR el juego compila y funciona igual en pantalla.
#   * Windows: lo instala vcpkg (openxr-loader, ver vcpkg.json).
#   * Linux:   sudo apt install libopenxr-dev (si no esta, se compila sin OpenXR con un aviso).
#   * macOS:   no hay runtime OpenXR real; por defecto desactivado. Para probar solo la inicializacion:
#              cmake --preset macos-debug -DENTORNVGI_ENABLE_OPENXR=ON -DVCPKG_MANIFEST_FEATURES=openxr
# El cargador (OpenXR::openxr_loader) se ENLAZA de verdad; no basta con tener las cabeceras.
if(APPLE)
	set(ENTORNVGI_OPENXR_POR_DEFECTO OFF)
else()
	set(ENTORNVGI_OPENXR_POR_DEFECTO ON)
endif()
option(ENTORNVGI_ENABLE_OPENXR "Compilar el soporte OpenXR (VR real)" ${ENTORNVGI_OPENXR_POR_DEFECTO})
add_library(EntornVGI_openxr INTERFACE)
set(ENTORNVGI_OPENXR_ACTIVO OFF)
if(ENTORNVGI_ENABLE_OPENXR)
	find_package(OpenXR CONFIG QUIET)
	if(TARGET OpenXR::openxr_loader)
		target_link_libraries(EntornVGI_openxr INTERFACE OpenXR::openxr_loader)
		target_compile_definitions(EntornVGI_openxr INTERFACE ENTORNVGI_HAS_OPENXR=1)
		set(ENTORNVGI_OPENXR_ACTIVO ON)
	else()
		message(WARNING "OpenXR no encontrado: se compila SIN soporte VR (el juego en pantalla no se ve afectado).")
	endif()
endif()
message(STATUS "OpenXR: ${ENTORNVGI_OPENXR_ACTIVO}")

# OBJ indexed geometry optimization. Build is optional if dependency is unavailable.
option(ENTORNVGI_ENABLE_OBJ_MESHOPT "Optimize indexed OBJ geometry with meshoptimizer" ON)
set(ENTORNVGI_MESHOPT_TARGET "")
if(ENTORNVGI_ENABLE_OBJ_MESHOPT)
    find_package(meshoptimizer CONFIG QUIET)
    if(TARGET meshoptimizer::meshoptimizer)
        set(ENTORNVGI_MESHOPT_TARGET meshoptimizer::meshoptimizer)
    elseif(TARGET meshoptimizer)
        set(ENTORNVGI_MESHOPT_TARGET meshoptimizer)
    else()
        message(WARNING "meshoptimizer not found: OBJ loader retains the legacy glDrawArrays path")
    endif()
endif()
message(STATUS "OBJ meshoptimizer target: ${ENTORNVGI_MESHOPT_TARGET}")

add_subdirectory(EntornVGI)

if(BUILD_TESTING)
	add_subdirectory(tests)
endif()