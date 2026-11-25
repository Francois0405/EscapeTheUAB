# Capa de red reutilizable. Sin OpenGL/Bullet/OpenXR. ENet es opcional y esta encapsulado en
# ENetTransport.cpp; sin ENet la libreria compila igual (CreateENetTransport devuelve nullptr).
set(ENTORNVGI_ENET_TARGET "")
if(ENTORNVGI_REQUIRE_ENET AND NOT ENTORNVGI_ENABLE_NETWORK)
	message(FATAL_ERROR "ENTORNVGI_REQUIRE_ENET=ON requiere ENTORNVGI_ENABLE_NETWORK=ON.")
endif()
if(ENTORNVGI_ENABLE_NETWORK)
	find_package(unofficial-enet CONFIG QUIET)      # vcpkg: vcpkg install enet
	if(TARGET unofficial::enet::enet)
		set(ENTORNVGI_ENET_TARGET unofficial::enet::enet)
	else()
		find_path(ENET_INCLUDE_DIR enet/enet.h)     # Linux: libenet-dev (macOS uses the vcpkg target above)
		find_library(ENET_LIBRARY enet)
		if(ENET_INCLUDE_DIR AND ENET_LIBRARY)
			add_library(EntornVGI_enet INTERFACE)
			target_include_directories(EntornVGI_enet INTERFACE ${ENET_INCLUDE_DIR})
			target_link_libraries(EntornVGI_enet INTERFACE ${ENET_LIBRARY})
			set(ENTORNVGI_ENET_TARGET EntornVGI_enet)
		endif()
	endif()
	if(NOT ENTORNVGI_ENET_TARGET)
		if(ENTORNVGI_REQUIRE_ENET)
			message(FATAL_ERROR
				"ENTORNVGI_REQUIRE_ENET=ON, pero ENet no esta disponible. En Windows/macOS usa vcpkg (enet); "
				"en Linux instala el paquete de desarrollo ENet de tu distribucion (por ejemplo libenet-dev) "
				"y vuelve a configurar. Para una compilacion offline deliberada, usa -DENTORNVGI_REQUIRE_ENET=OFF."
			)
		endif()
		message(STATUS "ENet: no encontrado; transporte real NO disponible (compilacion offline y tests en memoria).")
	else()
		message(STATUS "ENet: encontrado; transporte real habilitado.")
	endif()
	else()
	message(STATUS "ENet: busqueda desactivada por ENTORNVGI_ENABLE_NETWORK=OFF; transporte real NO disponible.")
endif()

set(ENTORNVGI_NETWORK_TIENE_ENET OFF CACHE INTERNAL "ENet encontrado por la capa de red" FORCE)
if(ENTORNVGI_ENET_TARGET)
	set(ENTORNVGI_NETWORK_TIENE_ENET ON CACHE INTERNAL "ENet encontrado por la capa de red" FORCE)
endif()

add_library(EntornVGI_network STATIC
	NetProtocol.cpp
	NetSession.cpp
	NetService.cpp
	ENetTransport.cpp
)
target_include_directories(EntornVGI_network PUBLIC "${REPO_ROOT}/src" "${REPO_ROOT}/EntornVGI")
target_compile_features(EntornVGI_network PUBLIC cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(EntornVGI_network PUBLIC Threads::Threads)
if(ENTORNVGI_ENET_TARGET)
	target_link_libraries(EntornVGI_network PRIVATE ${ENTORNVGI_ENET_TARGET})
	if(WIN32)
		# ENet usa Winsock y multimedia en Windows; vcpkg no propaga estas bibliotecas.
		target_link_libraries(EntornVGI_network PRIVATE ws2_32 winmm)
	endif()
	target_compile_definitions(EntornVGI_network PRIVATE ENTORNVGI_HAS_ENET=1)
endif()
