# Capa de red reutilizable. Sin OpenGL/Bullet/OpenXR. ENet es opcional y esta encapsulado en
# ENetTransport.cpp; sin ENet la libreria compila igual (CreateENetTransport devuelve nullptr).
set(ENTORNVGI_ENET_TARGET "")
if(ENTORNVGI_ENABLE_NETWORK)
	find_package(unofficial-enet CONFIG QUIET)      # vcpkg: vcpkg install enet
	if(TARGET unofficial::enet::enet)
		set(ENTORNVGI_ENET_TARGET unofficial::enet::enet)
	else()
		find_path(ENET_INCLUDE_DIR enet/enet.h)     # Linux: libenet-dev, macOS: brew install enet
		find_library(ENET_LIBRARY enet)
		if(ENET_INCLUDE_DIR AND ENET_LIBRARY)
			add_library(EntornVGI_enet INTERFACE)
			target_include_directories(EntornVGI_enet INTERFACE ${ENET_INCLUDE_DIR})
			target_link_libraries(EntornVGI_enet INTERFACE ${ENET_LIBRARY})
			if(WIN32)
				target_link_libraries(EntornVGI_enet INTERFACE ws2_32 winmm)
			endif()
			set(ENTORNVGI_ENET_TARGET EntornVGI_enet)
		endif()
	endif()
	if(NOT ENTORNVGI_ENET_TARGET)
		message(STATUS "ENet no encontrado: la red compila sin transporte real (modo offline y tests en memoria).")
	endif()
endif()

set(ENTORNVGI_NETWORK_TIENE_ENET OFF CACHE INTERNAL "ENet encontrado por la capa de red")
if(ENTORNVGI_ENET_TARGET)
	set(ENTORNVGI_NETWORK_TIENE_ENET ON CACHE INTERNAL "ENet encontrado por la capa de red")
endif()

add_library(EntornVGI_network STATIC
	NetProtocol.cpp
	NetSession.cpp
	NetService.cpp
	ENetTransport.cpp
)
target_include_directories(EntornVGI_network PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_features(EntornVGI_network PUBLIC cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(EntornVGI_network PUBLIC Threads::Threads)
if(ENTORNVGI_ENET_TARGET)
	target_link_libraries(EntornVGI_network PRIVATE ${ENTORNVGI_ENET_TARGET})
	target_compile_definitions(EntornVGI_network PRIVATE ENTORNVGI_HAS_ENET=1)
endif()
