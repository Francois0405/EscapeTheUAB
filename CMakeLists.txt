cmake_minimum_required(VERSION 3.25)

project(EntornVGI LANGUAGES C CXX)

include(CTest)

# Bullet Physics (fisica). vcpkg (Windows/macOS) y libbullet-dev (Linux) instalan el
# mismo BulletConfig.cmake, que define BULLET_INCLUDE_DIRS y BULLET_LIBRARIES.
# Se agrupa en un unico target para que el juego y las pruebas enlacen igual.
find_package(Bullet CONFIG REQUIRED)
add_library(EntornVGI_bullet INTERFACE)
target_include_directories(EntornVGI_bullet SYSTEM INTERFACE ${BULLET_INCLUDE_DIRS})
target_link_libraries(EntornVGI_bullet INTERFACE ${BULLET_LIBRARIES})

add_subdirectory(EntornVGI)

if(BUILD_TESTING)
	add_subdirectory(tests)
endif()
